#include "Config/Settings.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <format>
#include <fstream>
#include <iterator>
#include <optional>
#include <ranges>
#include <string_view>
#include <utility>

namespace pef::config {

using namespace std::literals;

namespace {

enum class Kind : std::uint8_t {
  flag,
  u32,
  profile_name,
};

/// The schema, as data. One row per known key: where it lives, how to parse it, what
/// range to enforce, and what it actually does (this column is a formality).
struct KeySpec {
  std::string_view section;
  std::string_view key;
  Kind kind{};
  bool Settings::* flag{};
  std::uint32_t Settings::* number{};
  std::uint32_t min{};
  std::uint32_t max{};
  std::string_view label;
};

constexpr std::array specs{
    KeySpec{
        .section = "Memory"sv,
        .key = "OptimizeAllocations"sv,
        .kind = Kind::flag,
        .flag = &Settings::optimize_allocations,
        .label = "either value does nothing; this one does it efficiently"sv,
    },
    KeySpec{
        .section = "Threading"sv,
        .key = "WorkerThreads"sv,
        .kind = Kind::u32,
        .number = &Settings::worker_threads,
        .min = 1,
        .max = 64,
        .label = "parsed, validated, then cherished; not used"sv,
    },
    KeySpec{
        .section = "Culling"sv,
        .key = "AggressiveCulling"sv,
        .kind = Kind::flag,
        .flag = &Settings::aggressive_culling,
        .label = "if 1, culls nothing slightly faster"sv,
    },
    KeySpec{
        .section = "Product"sv,
        .key = "EnablePEF"sv,
        .kind = Kind::flag,
        .flag = &Settings::enable_pef,
        .label = "toggling it off does nothing, because that is the product"sv,
    },
    KeySpec{
        .section = "Product"sv,
        .key = "Profile"sv,
        .kind = Kind::profile_name,
        .label = "byte-identical to the other two, by build law"sv,
    },
    KeySpec{
        .section = "Instrumentation"sv,
        .key = "AuditIntervalMs"sv,
        .kind = Kind::u32,
        .number = &Settings::audit_interval_ms,
        .min = 50,
        .max = 60000,
        .label = "the integrity assertion period; load-bearing"sv,
    },
    KeySpec{
        .section = "Instrumentation"sv,
        .key = "ReportRefreshSeconds"sv,
        .kind = Kind::u32,
        .number = &Settings::report_refresh_s,
        .min = 1,
        .max = 600,
        .label = "how often PEF_Report.txt is rewritten from a live snapshot"sv,
    },
    KeySpec{
        .section = "Instrumentation"sv,
        .key = "StatusLogSeconds"sv,
        .kind = Kind::u32,
        .number = &Settings::status_log_s,
        .min = 1,
        .max = 3600,
        .label = "period of the 'did nothing in 0.4 µs' status line"sv,
    },
};

[[nodiscard]] std::string_view trim(std::string_view a_text) {
  const auto is_space = [](char a_char) { return a_char == ' ' || a_char == '\t'; };
  const auto begin = std::ranges::find_if(a_text, [&](char a_char) { return !is_space(a_char); });
  const auto end = std::ranges::find_if(a_text | std::views::reverse, [&](char a_char) {
                     return !is_space(a_char);
                   }).base();
  return {begin, end < begin ? begin : end};
}

/// Booleans accept exactly "0" or "1". Anything else is not a boolean, it is a lifestyle,
/// and it keeps the default.
[[nodiscard]] std::optional<bool> parse_flag(std::string_view a_value) {
  if (a_value == "0"sv) {
    return false;
  }
  if (a_value == "1"sv) {
    return true;
  }
  return std::nullopt;
}

/// Integers via std::from_chars, with full-consumption required: "8 threads" is not a
/// thread count, it is a sentence.
[[nodiscard]] std::optional<std::uint32_t> parse_u32(std::string_view a_value) {
  auto value = std::uint32_t{};
  const auto [ptr, ec] = std::from_chars(a_value.data(), a_value.data() + a_value.size(), value);
  if (ec == std::errc{} && ptr == a_value.data() + a_value.size()) {
    return value;
  }
  return std::nullopt;
}

[[nodiscard]] std::optional<Profile> parse_profile(std::string_view a_value) {
  if (a_value == "Performance"sv) {
    return Profile::performance;
  }
  if (a_value == "Balanced"sv) {
    return Profile::balanced;
  }
  if (a_value == "Quality"sv) {
    return Profile::quality;
  }
  return std::nullopt;
}

[[nodiscard]] const KeySpec* find_spec(std::string_view a_section, std::string_view a_key) {
  const auto it = std::ranges::find_if(specs, [&](const KeySpec& a_spec) {
    return a_spec.section == a_section && a_spec.key == a_key;
  });
  return it != specs.end() ? &*it : nullptr;
}

/// Parses, validates, and assigns one known key — or keeps the default and files a
/// diagnostic explaining exactly how the INI failed to matter.
void apply(Settings& a_settings, const KeySpec& a_spec, std::string_view a_value,
           std::vector<std::string>& a_diagnostics) {
  switch (a_spec.kind) {
  case Kind::flag: {
    const auto parsed = parse_flag(a_value);
    if (!parsed) {
      a_diagnostics.push_back(std::format(
          "[{}] {} accepts 0 or 1 (both do nothing); '{}' kept the default. The setting "
          "is cherished regardless.",
          a_spec.section, a_spec.key, a_value));
      return;
    }
    a_settings.*a_spec.flag = *parsed;
    return;
  }
  case Kind::u32: {
    const auto parsed = parse_u32(a_value);
    if (!parsed || *parsed < a_spec.min || *parsed > a_spec.max) {
      a_diagnostics.push_back(
          std::format("[{}] {}={} is outside {}-{}; default kept. Range-checked, then cherished.",
                      a_spec.section, a_spec.key, a_value, a_spec.min, a_spec.max));
      return;
    }
    a_settings.*a_spec.number = *parsed;
    return;
  }
  case Kind::profile_name: {
    const auto parsed = parse_profile(a_value);
    if (!parsed) {
      a_diagnostics.push_back(
          std::format("[{}] Profile={} is not Performance, Balanced, or Quality (all three are "
                      "identical); Balanced kept.",
                      a_spec.section, a_value));
      return;
    }
    a_settings.profile = *parsed;
    return;
  }
  }
}

} // namespace

const char* profile_name(Profile a_profile) noexcept {
  switch (a_profile) {
  case Profile::performance:
    return "Performance";
  case Profile::balanced:
    return "Balanced";
  case Profile::quality:
    return "Quality";
  }
  return "Balanced";
}

std::vector<std::string> Settings::describe() const {
  auto lines = std::vector<std::string>{};
  lines.reserve(specs.size());
  for (const auto& spec : specs) {
    auto value = std::string{};
    if (spec.kind == Kind::flag) {
      value = this->*spec.flag ? "1" : "0";
    } else if (spec.kind == Kind::u32) {
      value = std::format("{}", this->*spec.number);
    } else {
      value = profile_name(profile);
    }
    lines.push_back(
        std::format("[{:<14}] {:<20} = {:<7} - {}", spec.section, spec.key, value, spec.label));
  }
  return lines;
}

std::expected<SettingsReport, std::string> load(const std::filesystem::path& a_ini) {
  auto text = std::string{};
  {
    auto file = std::ifstream{a_ini};
    if (!file.is_open()) {
      return std::unexpected{std::format("cannot open {}", a_ini.string())};
    }
    text.assign(std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{});
  }

  auto report = SettingsReport{};
  auto& settings = report.settings;
  auto section = std::string_view{"General"sv};

  auto rest = std::string_view{text};
  while (!rest.empty()) {
    const auto newline = rest.find('\n');
    const auto length = newline == std::string_view::npos ? rest.size() : newline;
    auto line = trim(rest.substr(0, length));
    rest = newline == std::string_view::npos ? std::string_view{} : rest.substr(newline + 1);

    if (const auto comment = line.find_first_of(";#"sv); comment != std::string_view::npos) {
      line = trim(line.substr(0, comment));
    }
    if (line.empty()) {
      continue;
    }
    if (line.front() == '[' && line.back() == ']') {
      section = trim(line.substr(1, line.size() - 2));
      continue;
    }

    const auto equals = line.find('=');
    if (equals == std::string_view::npos) {
      // A line without '=' is not rejected. It is acknowledged.
      report.diagnostics.push_back(std::format("acknowledged a line without '=': {}", line));
      continue;
    }
    const auto key = trim(line.substr(0, equals));
    const auto value = trim(line.substr(equals + 1));
    const auto* spec = find_spec(section, key);
    if (spec == nullptr) {
      settings.acknowledged.push_back(std::format("[{}] {}", section, key));
      continue;
    }
    apply(settings, *spec, value, report.diagnostics);
  }

  return report;
}

} // namespace pef::config
