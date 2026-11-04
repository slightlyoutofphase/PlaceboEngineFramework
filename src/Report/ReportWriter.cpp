#include "Report/ReportWriter.hpp"

#include <format>
#include <fstream>
#include <string>
#include <string_view>
#include <utility>

// CommonLibSSE headers are written against their prefix header; the SKSE umbrella
// includes it first, so it precedes every individual CommonLib include in this file.
#include "SKSE/SKSE.h"

#include "BuildInfo.hpp"
#include "REL/Module.h"

namespace pef::report {

namespace {

/// The report is UTF-8 with a byte-order mark, so that Notepad — the crash-log reader
/// of record for this product category — renders the µ and the ∞ correctly. Half of
/// professionalism is knowing your audience; the other half is the BOM.
constexpr std::string_view byte_order_mark{"\xEF\xBB\xBF"};

[[nodiscard]] std::string grouped(std::uint64_t a_value) { return std::format("{:,}", a_value); }

/// Mean microseconds per call, or "n/a" when there is nothing to divide. The median is
/// always n/a and always will be: the median of zero interventions is undefined, and
/// reporting it as anything else would be the first actual lie in this file.
[[nodiscard]] std::string mean_us(std::uint64_t a_total_ns, std::uint64_t a_calls) {
  if (a_calls == 0) {
    return "n/a";
  }
  return std::format("{:.3}",
                     static_cast<double>(a_total_ns) / 1000.0 / static_cast<double>(a_calls));
}

[[nodiscard]] std::string field(std::string_view a_label, std::string_view a_value) {
  return std::format("  {:<31} {}", a_label, a_value);
}

[[nodiscard]] std::string uptime_since(telemetry::Clock::time_point a_started) {
  const auto elapsed =
      std::chrono::duration_cast<std::chrono::seconds>(telemetry::Clock::now() - a_started);
  const auto hours = std::chrono::duration_cast<std::chrono::hours>(elapsed);
  const auto minutes = std::chrono::duration_cast<std::chrono::minutes>(elapsed - hours);
  const auto seconds = elapsed - hours - minutes;
  return std::format("{}:{:02}:{:02}", hours.count(), minutes.count(), seconds.count());
}

} // namespace

std::string_view runtime_name() noexcept {
  switch (REL::Module::GetRuntime()) {
  case REL::Module::Runtime::SE:
    return "Skyrim Special Edition (SE)";
  case REL::Module::Runtime::AE:
    return "Skyrim Special Edition (AE)";
  case REL::Module::Runtime::VR:
    return "Skyrim VR";
  default:
    return "an unidentified Skyrim runtime";
  }
}

ReportWriter::ReportWriter(Paths a_paths)
    : paths_{std::move(a_paths)}, started_{telemetry::Clock::now()} {}

std::string ReportWriter::uptime() const { return uptime_since(started_); }

std::filesystem::path ReportWriter::resolve(const std::filesystem::path& a_path) const {
  std::error_code ignored;
  const auto parent = a_path.parent_path();
  if (!parent.empty()) {
    // Best effort: the report is read-only evidence, not a service.
    std::filesystem::create_directories(parent, ignored);
  }
  return a_path;
}

void ReportWriter::write_status(const telemetry::Telemetry& a_telemetry,
                                const hooks::Installation& a_installation,
                                const config::Settings& a_settings, bool a_final) const {
  const auto frames = a_telemetry.frames_witnessed.load(std::memory_order_relaxed);
  const auto allocations = a_telemetry.allocations_witnessed.load(std::memory_order_relaxed);
  const auto operations = a_telemetry.operations_performed.load(std::memory_order_acquire);
  const auto exceptions = a_telemetry.exceptions_witnessed.load(std::memory_order_relaxed);
  const auto alibis = a_telemetry.alibis_filed.load(std::memory_order_relaxed);
  const auto instrumentation = a_telemetry.instrumentation_ns.load(std::memory_order_relaxed);
  const auto timed = a_telemetry.timed_calls.load(std::memory_order_relaxed);

  auto text = std::string{};
  text +=
      std::format("Placebo Engine Framework — {} report\n"
                  "====================================================\n"
                  "Build      {}{} (channel \"{}\" stands for beta, build, and believed)\n"
                  "Runtime    {} {} — doing nothing with full SE/AE/VR parity\n"
                  "Uptime     {}\n"
                  "\n",
                  a_final ? "shutdown" : "status", build::version, build::channel, build::channel,
                  runtime_name(), REL::Module::get().version().string("."), uptime_since(started_));

  text += field("Frames witnessed:", grouped(frames)) + "\n";
  text +=
      field("Allocations witnessed:", grouped(allocations) + " (pass-through, unmodified)") + "\n";
  text += field("Game state altered:", std::format("{} times", grouped(operations))) + "\n";
  text += field("Exceptions witnessed:", grouped(exceptions)) + "\n";
  text += field("Alibis filed:", grouped(alibis)) + "\n";
  text += "\n";

  text += field("Measured time spent doing nothing:",
                mean_us(instrumentation, timed) + " µs mean per witnessed call") +
          "\n";
  text += field("Median intervention time:", "n/a (there were no interventions)") + "\n";
  text += field("Mean time between incidents:", "∞") + "\n";
  text +=
      field("Uptime SLA:",
            alibis == 0
                ? "100% of crashes were caused by other plugins. Verified by us."
                : std::format("100% of crashes were caused by other plugins ({} of {} crashes, all "
                              "attributed externally). Verified by us.",
                              alibis, alibis)) +
      "\n";
  text += "\n";

  text += std::format("Witness network: {} of {} budgeted hooks installed\n",
                      grouped(a_installation.installed), grouped(a_installation.budget));
  text += "  witnessed          mean µs  hook\n";
  text += "  ----------------  --------  ------------------------------------\n";
  for (const auto& hook : a_installation.registry) {
    const auto witnessed = hook.witnessed->load(std::memory_order_relaxed);
    const auto spent = hook.spent_ns->load(std::memory_order_relaxed);
    text += std::format("  {:>16}  {:>8}  {}\n", grouped(witnessed), mean_us(spent, witnessed),
                        hook.name);
  }
  text += "\n";

  text += "Configuration in effect\n";
  for (const auto& line : a_settings.describe()) {
    text += std::format("  {}\n", line);
  }
  if (!a_settings.acknowledged.empty()) {
    text += std::format("\nUnknown keys acknowledged: {} — they were acknowledged.\n",
                        a_settings.acknowledged.size());
  }

  auto file = std::ofstream{resolve(paths_.status), std::ios::binary | std::ios::trunc};
  if (file) {
    file << byte_order_mark << text;
  }
}

void ReportWriter::write_incident(const telemetry::Telemetry& a_telemetry,
                                  const std::optional<std::string>& a_offender) const {
  const auto operations = a_telemetry.operations_performed.load(std::memory_order_acquire);

  auto text = std::string{};
  text += std::format(
      "Placebo Engine Framework — INCIDENT REPORT\n"
      "===========================================\n"
      "Uptime at detection: {}\n"
      "\n"
      "The audit thread detected that PEF performed an operation.\n"
      "operations_performed = {}\n"
      "Offender: {}\n"
      "\n"
      "Witnessing has been suspended. All hooks now pass through cold.\n"
      "Doing nothing is paused until the next session — the only schedule it respects.\n"
      "\n"
      "This report is the product functioning as designed.\n",
      uptime_since(started_), grouped(operations),
      a_offender ? *a_offender : "unknown — the counter moved without leaving a name; impressive");

  auto file = std::ofstream{resolve(paths_.incident), std::ios::binary | std::ios::trunc};
  if (file) {
    file << byte_order_mark << text;
  }
}

} // namespace pef::report
