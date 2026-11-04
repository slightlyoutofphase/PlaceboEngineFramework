#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

#include "Config/Settings.hpp"
#include "Diagnostics/Telemetry.hpp"
#include "Hooks/Installer.hpp"

namespace pef::report {

/// "Skyrim Special Edition (SE)" / "(AE)" / "Skyrim VR" — used by the banner and the
/// reports, because claiming SE/AE/VR parity is best done while naming the runtime.
[[nodiscard]] std::string_view runtime_name() noexcept;

/// Writes the product's paper trail: the continuously refreshed status report, the
/// shutdown report that finalizes it, and the incident report that exists so that the
/// phrase "this should never happen" has somewhere official to live.
class ReportWriter {
public:
  struct Paths {
    std::filesystem::path status{};
    std::filesystem::path incident{};
  };

  explicit ReportWriter(Paths a_paths);

  /// Rewrites PEF_Report.txt from a telemetry snapshot. Called periodically by the
  /// audit thread so that the report is current even when the process dies without
  /// the courtesy of a shutdown — which is most of the interesting times to read it.
  void write_status(const telemetry::Telemetry& a_telemetry,
                    const hooks::Installation& a_installation, const config::Settings& a_settings,
                    bool a_final) const;

  /// Writes PEF_Incident.txt. Should never be called. Is fully implemented anyway.
  void write_incident(const telemetry::Telemetry& a_telemetry,
                      const std::optional<std::string>& a_offender) const;

  /// Uptime at the moment of asking, formatted as h:mm:ss.
  [[nodiscard]] std::string uptime() const;

private:
  [[nodiscard]] std::filesystem::path resolve(const std::filesystem::path& a_path) const;

  Paths paths_;
  telemetry::Clock::time_point started_;
};

} // namespace pef::report
