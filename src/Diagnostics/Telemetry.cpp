#include "Diagnostics/Telemetry.hpp"

#include <format>
#include <utility>

namespace pef::telemetry {

Telemetry& the() noexcept {
  static Telemetry instance;
  return instance;
}

void Telemetry::register_operation(std::source_location a_location) {
  // Release: paired with the audit thread's acquire load, so that the moment the
  // counter moves, "doing nothing" stops everywhere — not eventually, immediately.
  operations_performed.fetch_add(1, std::memory_order_release);
  const auto lock = std::lock_guard{offender_mutex};
  offender = std::format("{} at {}:{}", a_location.function_name(), a_location.file_name(),
                         a_location.line());
}

std::optional<std::string> Telemetry::offender_snapshot() const {
  const auto lock = std::lock_guard{offender_mutex};
  return offender;
}

} // namespace pef::telemetry
