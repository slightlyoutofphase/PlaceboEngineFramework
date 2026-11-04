#pragma once

#include <format>
#include <string_view>

namespace pef::log {

enum class Level : std::uint8_t {
  info,
  warning,
  error,
  critical,
};

/// Forwards one rendered message to the SKSE logger. The message is passed to spdlog as
/// data, never as a format string, so log text containing braces is safe by construction.
void write(Level a_level, std::string_view a_message);

/// Formats with std::format and logs at info. Call sites stay terse; the formatting
/// engine stays singular; the log stays honest.
template <class... Args> void info(std::format_string<Args...> a_format, Args&&... a_args) {
  write(Level::info, std::format(a_format, std::forward<Args>(a_args)...));
}

template <class... Args> void warning(std::format_string<Args...> a_format, Args&&... a_args) {
  write(Level::warning, std::format(a_format, std::forward<Args>(a_args)...));
}

template <class... Args> void error(std::format_string<Args...> a_format, Args&&... a_args) {
  write(Level::error, std::format(a_format, std::forward<Args>(a_args)...));
}

template <class... Args> void critical(std::format_string<Args...> a_format, Args&&... a_args) {
  write(Level::critical, std::format(a_format, std::forward<Args>(a_args)...));
}

} // namespace pef::log
