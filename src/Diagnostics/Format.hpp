#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace pef::format {

/// Renders an unsigned count with fixed ',' digit grouping: 1234567 becomes "1,234,567".
///
/// std::format has no grouping specifier of its own, and the locale-aware alternative
/// would make the shutdown report depend on the reader's regional settings — an unforced
/// variable for a document whose entire value is stating plain facts in plain digits.
/// The commas are placed here, by arithmetic, identically on every machine that will
/// ever open PEF_Report.txt.
[[nodiscard]] inline std::string grouped(std::uint64_t a_value) {
  const auto digits = std::to_string(a_value);
  std::string result;
  result.reserve(digits.size() + digits.size() / 3);
  for (std::size_t i = 0; i < digits.size(); ++i) {
    if (i != 0 && (digits.size() - i) % 3 == 0) {
      result += ',';
    }
    result += digits[i];
  }
  return result;
}

} // namespace pef::format
