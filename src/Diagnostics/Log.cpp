#include "Diagnostics/Log.hpp"

#include <spdlog/spdlog.h>

#include <utility>

namespace pef::log {

namespace {

[[nodiscard]] spdlog::level::level_enum to_spdlog(Level a_level) noexcept {
  switch (a_level) {
  case Level::info:
    return spdlog::level::info;
  case Level::warning:
    return spdlog::level::warn;
  case Level::error:
    return spdlog::level::err;
  case Level::critical:
    return spdlog::level::critical;
  }
  return spdlog::level::info;
}

} // namespace

void write(Level a_level, std::string_view a_message) {
  spdlog::log(to_spdlog(a_level), "{}", a_message);
}

} // namespace pef::log
