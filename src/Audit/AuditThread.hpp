#pragma once

#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <stop_token>
#include <thread>

#include "Config/Settings.hpp"
#include "Hooks/Installer.hpp"
#include "Report/ReportWriter.hpp"

namespace pef::audit {

/// The Operational Integrity loop: a self-auditing worker thread that samples the
/// operations counter on a fixed period and asserts, every single time, that the plugin
/// has still done nothing. If the counter ever moves, the thread files an incident
/// report and suspends witnessing — doing nothing must stop immediately when it starts.
///
/// A real assertion would terminate the process; terminating the game to prove that we
/// never modified it would be modifying it. So the assertion is administrative instead:
/// loud, documented, and impossible to un-see.
class AuditThread {
public:
  AuditThread(const report::ReportWriter& a_report, const hooks::Installation& a_installation,
              const config::Settings& a_settings);

  /// Starts the loop. Must be called after the installation registry is complete; the
  /// thread-creation edge provides the ordering, and this comment provides the reminder.
  void start();

  /// Requests a stop and joins. Bounded by one audit period at most, and typically much
  /// less: the wake condition variable is stop-token aware, so the thread exits at the
  /// instant stop is requested rather than at the end of the interval.
  void stop() noexcept;

private:
  void run(std::stop_token a_stop);
  void audit_once();

  const report::ReportWriter& report_;
  const hooks::Installation& installation_;
  const config::Settings& settings_;
  std::chrono::milliseconds interval_{};
  std::uint64_t report_period_ticks_{1};
  std::uint64_t status_period_ticks_{1};
  std::uint64_t ticks_{0};
  std::uint64_t last_timed_calls_{0};
  std::uint64_t last_ns_{0};
  std::uint64_t last_frames_{0};
  std::uint64_t last_allocations_{0};
  std::jthread thread_;
};

} // namespace pef::audit
