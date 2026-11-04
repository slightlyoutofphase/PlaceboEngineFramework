#pragma once

#include <cstdint>
#include <expected>
#include <filesystem>
#include <string>
#include <vector>

namespace pef::config {

enum class Profile : std::uint8_t {
  performance,
  balanced,
  quality,
};

/// The configuration. Every field is parsed, validated, and range-checked. Most of them
/// are then deliberately not consulted: the performance surface of this product is the
/// empty set, and a setting that changed it would be a defect. The [Instrumentation]
/// fields are the exception — the product does nothing, and the instrumentation is how
/// it proves that.
struct Settings {
  /// [Memory] OptimizeAllocations — valid values: 0 or 1. Both do nothing. The setting
  /// exists so you feel involved.
  bool optimize_allocations{false};

  /// [Threading] WorkerThreads — parsed, validated, then cherished. Not used.
  std::uint32_t worker_threads{8};

  /// [Culling] AggressiveCulling — if 1, culls nothing slightly faster.
  bool aggressive_culling{false};

  /// [Product] EnablePEF — the 2.1 toggle. Toggling it off does nothing, because that
  /// is the product: the plugin is identical in both states, and so is your FPS.
  bool enable_pef{true};

  /// [Product] Profile — Performance, Balanced, or Quality. The three files are
  /// byte-identical, and the build system enforces it; the names exist so the choice
  /// feels consequential. This is called marketing parity.
  Profile profile{Profile::balanced};

  /// [Instrumentation] The integrity assertion period, in milliseconds.
  std::uint32_t audit_interval_ms{250};

  /// [Instrumentation] How often PEF_Report.txt is rewritten from a telemetry snapshot.
  std::uint32_t report_refresh_s{10};

  /// [Instrumentation] Period of the "did nothing in 0.4 µs" status line.
  std::uint32_t status_log_s{30};

  /// Keys that were present in the INI, unknown to the schema, and acknowledged. Not
  /// rejected — acknowledged. There is a difference, and it is tonal.
  std::vector<std::string> acknowledged{};

  /// One line per known setting — "Key = Value — what it actually does" — ready for the
  /// startup banner and the status report.
  [[nodiscard]] std::vector<std::string> describe() const;
};

struct SettingsReport {
  Settings settings{};
  std::vector<std::string> diagnostics{};
};

/// Loads and validates the INI. Returns std::unexpected only when the file cannot be
/// opened; every other problem — an invalid value, an out-of-range number, a key nobody
/// documented — is a diagnostic, because a wrong INI still does nothing, and PEF is not
/// going to crash over nothing.
[[nodiscard]] std::expected<SettingsReport, std::string> load(const std::filesystem::path& a_ini);

/// "Performance", "Balanced", or "Quality". Whichever you choose, it is this string that
/// changes, and nothing else in the observable universe.
[[nodiscard]] const char* profile_name(Profile a_profile) noexcept;

} // namespace pef::config
