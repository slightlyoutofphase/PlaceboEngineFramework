#include "Plugin.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <array>
#include <cstdlib>
#include <filesystem>
#include <mutex>
#include <string>
#include <utility>

#include "Audit/AuditThread.hpp"
#include "BuildInfo.hpp"
#include "Config/Settings.hpp"
#include "Crash/AlibiHandler.hpp"
#include "Diagnostics/Log.hpp"
#include "Diagnostics/Telemetry.hpp"
#include "Hooks/Installer.hpp"
#include "Report/ReportWriter.hpp"

// CommonLibSSE headers are written against their prefix header; the SKSE umbrella
// includes it first, so it precedes every individual CommonLib include in this file.
#include "SKSE/SKSE.h"

#include "REL/Module.h"

namespace pef {

namespace {

// ---------------------------------------------------------------------------------------------
// Paths. PEF writes exactly three files, all next to the game's logs, where crash
// investigators are already trained to look: PEF_Report.txt, PEF_Incident.txt, and
// PEF_Alibi.txt under My Games/<game>/.
// ---------------------------------------------------------------------------------------------

[[nodiscard]] std::filesystem::path game_directory() {
  auto buffer = std::array<wchar_t, MAX_PATH + 1>{};
  const auto length =
      ::GetModuleFileNameW(nullptr, buffer.data(), static_cast<DWORD>(buffer.size()));
  if (length == 0 || length >= buffer.size()) {
    return std::filesystem::current_path();
  }
  const auto executable = std::filesystem::path{std::wstring_view{buffer.data(), length}};
  return executable.parent_path();
}

/// My Games/<game>/ — the parent of the directory SKSE logs into. The alibi and the
/// reports live here so that every crash-log reader in the ecosystem finds them without
/// being asked.
[[nodiscard]] std::filesystem::path user_directory() {
  if (const auto logs = SKSE::log_directory()) {
    return logs->parent_path();
  }
  return game_directory();
}

/// Dependency Integrity: Null Core is a separate mod consisting of one README and zero
/// kilobytes. PEF refuses to load without it. This is a feature, it has a name, and the
/// name is in the README.
[[nodiscard]] bool null_core_present(const std::filesystem::path& a_game) {
  const auto core = a_game / "Data" / "NullCore";
  return std::filesystem::exists(core / "README.md") ||
         std::filesystem::exists(core / "README.txt");
}

// ---------------------------------------------------------------------------------------------
// The runtime: everything with a lifetime between load and finalization.
// ---------------------------------------------------------------------------------------------

struct Runtime {
  explicit Runtime(report::ReportWriter::Paths a_paths)
      : report{std::move(a_paths)}, audit{report, installation, settings} {}

  config::Settings settings{};
  hooks::Installation installation{};
  report::ReportWriter report;
  audit::AuditThread audit;
  std::once_flag finalize_once{};
};

[[nodiscard]] Runtime& runtime() {
  static Runtime instance{[] {
    const auto home = user_directory();
    return report::ReportWriter::Paths{
        home / "PEF_Report.txt",
        home / "PEF_Incident.txt",
    };
  }()};
  return instance;
}

/// The final report: written on process exit, once, idempotently. The audit thread has
/// been refreshing PEF_Report.txt all session, so this is a courtesy flush rather than a
/// rescue — the report was already current before the game decided it was done.
void finalize() {
  auto& instance = runtime();
  std::call_once(instance.finalize_once, [&instance] {
    instance.audit.stop();
    instance.report.write_status(telemetry::the(), instance.installation, instance.settings, true);
    crash::AlibiHandler::uninstall();
    log::info("shutdown report finalized: {:,} frames witnessed, 0 game state altered.",
              telemetry::the().frames_witnessed.load(std::memory_order_relaxed));
  });
}

/// SKSE milestone traffic. PEF acknowledges the game's milestones without affecting
/// them, which is also a precise description of its relationship to everything else.
void on_message(SKSE::MessagingInterface::Message* a_message) {
  if (a_message == nullptr) {
    return;
  }
  switch (a_message->type) {
  case SKSE::MessagingInterface::kPostLoad:
    log::info("post-load: other plugins are loading now. PEF suspects all of them equally.");
    break;
  case SKSE::MessagingInterface::kPostPostLoad:
    log::info("post-post-load: the loads have stopped loading. Dormant.");
    break;
  case SKSE::MessagingInterface::kPreLoadGame:
  case SKSE::MessagingInterface::kPostLoadGame:
    break; // load screens are witnessed silently; commentary would be doing something
  case SKSE::MessagingInterface::kSaveGame:
    log::info("a save was written. PEF neither caused it nor prevented it.");
    break;
  case SKSE::MessagingInterface::kDeleteGame:
    log::info("a save was deleted. PEF was not consulted, and would have said nothing.");
    break;
  case SKSE::MessagingInterface::kInputLoaded:
    log::info("input loaded. PEF witnessed the event and altered none of it.");
    break;
  case SKSE::MessagingInterface::kNewGame:
    log::info("a new game began. it was not caused by PEF — the strongest claim available.");
    break;
  case SKSE::MessagingInterface::kDataLoaded:
    log::info("data loaded. PEF remains dormant, as specified.");
    break;
  }
}

} // namespace

bool LoadPlugin(const SKSE::LoadInterface& a_skse) {
  SKSE::Init(&a_skse, SKSE::InitInfo{
                          .log = true,
                          .logPattern = "[%Y-%m-%d %H:%M:%S.%e] [%l] %v",
                          .trampoline = true,
                          .trampolineSize = build::trampoline_bytes,
                      });

  log::info("{}", build::tagline);
  log::info("runtime: {} {} — doing nothing with full SE/AE/VR parity, via CommonLibSSE-NG.",
            report::runtime_name(), REL::Module::get().version().string("."));

  // Configuration: parsed, validated, and acknowledged.
  const auto game = game_directory();
  auto settings = config::Settings{};
  {
    const auto ini = game / "Data" / "SKSE" / "PlaceboEngineFramework.ini";
    if (const auto loaded = config::load(ini)) {
      settings = loaded->settings;
      for (const auto& diagnostic : loaded->diagnostics) {
        log::warning("  {}", diagnostic);
      }
    } else {
      log::warning("settings: {} — defaults engaged. The defaults also do nothing, identically.",
                   loaded.error());
    }
  }
  for (const auto& key : settings.acknowledged) {
    log::info("  acknowledged: {} — acknowledged.", key);
  }
  log::info("profile: {} — byte-identical to Performance and Quality. This is marketing parity.",
            config::profile_name(settings.profile));

  // Dependency Integrity.
  if (!null_core_present(game)) {
    log::critical("Dependency Integrity: Null Core was not found under Data/NullCore/.");
    log::critical("PEF refuses to load without it. Null Core is 0 KB. This is the feature.");
    return false;
  }
  log::info("Null Core: present — 0 KB, every byte verified. Dependency Integrity satisfied.");

  auto& instance = runtime();
  instance.settings = std::move(settings);

  // The first responder.
  if (crash::AlibiHandler::install(user_directory() / "PEF_Alibi.txt")) {
    log::info("first-responder alibi handler installed at priority 1 (continues search).");
  } else {
    log::warning("alibi handler installation failed; crashes will proceed without us. On brand.");
  }

  // The witness network.
  instance.installation = hooks::install(build::hook_budget);
  log::info("witness network: {} of {} budgeted hooks installed, all pass-through.",
            instance.installation.installed, instance.installation.budget);
  if (instance.installation.installed < instance.installation.budget) {
    log::warning("the engine offered fewer hot call sites than the budget. The report will reflect "
                 "reality, which is the one thing PEF refuses to compromise on.");
  }

  // Operational Integrity.
  instance.audit.start();
  log::info("audit thread: {} ms interval, asserting operations_performed == 0, forever.",
            instance.settings.audit_interval_ms);

  if (SKSE::GetMessagingInterface()->RegisterListener(&on_message)) {
    log::info("messaging: registered. Milestones will be acknowledged, not affected.");
  } else {
    log::error("messaging: registration failed. Milestones will pass unwitnessed. Still dormant.");
  }

  std::atexit(&finalize);

  instance.report.write_status(telemetry::the(), instance.installation, instance.settings, false);
  log::info("operational status: dormant. Product functioning as specified.");
  return true;
}

} // namespace pef
