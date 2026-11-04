#include "Audit/AuditThread.hpp"

#include <utility>

#include "Diagnostics/Log.hpp"
#include "Diagnostics/Telemetry.hpp"

namespace pef::audit {

namespace {

[[nodiscard]] std::uint64_t ticks_per(std::uint32_t a_seconds,
                                      std::chrono::milliseconds a_interval) noexcept {
  const auto ticks = (static_cast<std::uint64_t>(a_seconds) * 1000) /
                     static_cast<std::uint64_t>(a_interval.count());
  return ticks < 1 ? 1 : ticks;
}

} // namespace

AuditThread::AuditThread(const report::ReportWriter& a_report,
                         const hooks::Installation& a_installation,
                         const config::Settings& a_settings)
    : report_{a_report}, installation_{a_installation}, settings_{a_settings} {}

void AuditThread::start() {
  // The periods are computed here, at start, rather than at construction: settings are
  // loaded after the runtime object exists, and an audit thread that remembered the
  // defaults instead of the configuration would be a very different kind of product.
  interval_ = std::chrono::milliseconds{settings_.audit_interval_ms};
  report_period_ticks_ = ticks_per(settings_.report_refresh_s, interval_);
  status_period_ticks_ = ticks_per(settings_.status_log_s, interval_);
  thread_ = std::jthread{[this](std::stop_token a_stop) { run(a_stop); }};
}

void AuditThread::stop() noexcept {
  if (thread_.joinable()) {
    thread_.request_stop();
    try {
      thread_.join();
    } catch (...) {
      // join() after a joinable() check fails only at the OS level, and an audit thread
      // that cannot be joined has bigger problems than an unwitnessed exception here.
    }
  }
}

void AuditThread::run(std::stop_token a_stop) {
  // The stop-token-aware wait: shutdown wakes the thread at the instant stop is
  // requested instead of at the end of the audit period. Integrity work should not
  // have to wait for a timer to believe you.
  auto wake_mutex = std::mutex{};
  auto wake = std::condition_variable_any{};
  while (!a_stop.stop_requested()) {
    auto lock = std::unique_lock{wake_mutex};
    wake.wait_for(lock, a_stop, interval_, [&a_stop] { return a_stop.stop_requested(); });
    if (a_stop.stop_requested()) {
      break;
    }
    audit_once();
  }
}

void AuditThread::audit_once() {
  ++ticks_;
  auto& telemetry = telemetry::the();

  // The assertion. Relaxed loads are not enough here: acquire pairs with the release
  // store in register_operation() so that when the counter reads nonzero, every write
  // the offender made is visible before the incident report names it.
  const auto operations = telemetry.operations_performed.load(std::memory_order_acquire);
  if (operations != 0 && telemetry.operating.exchange(false, std::memory_order_acq_rel)) {
    log::critical("OPERATIONAL INTEGRITY VIOLATION: operations_performed is {}.", operations);
    log::critical("Doing nothing has been suspended. Witness hooks now pass through cold.");
    report_.write_incident(telemetry, telemetry.offender_snapshot());
  }

  if (ticks_ == 1 || ticks_ % report_period_ticks_ == 0) {
    report_.write_status(telemetry, installation_, settings_, false);
  }

  if (ticks_ == 1 || ticks_ % status_period_ticks_ == 0) {
    // Period deltas, so the log line stays honest about "this period" instead of
    // drifting into career totals. An honest statistic is just a lie with citations.
    const auto frames = telemetry.frames_witnessed.load(std::memory_order_relaxed);
    const auto allocations = telemetry.allocations_witnessed.load(std::memory_order_relaxed);
    const auto calls = telemetry.timed_calls.load(std::memory_order_relaxed);
    const auto ns = telemetry.instrumentation_ns.load(std::memory_order_relaxed);
    const auto period_calls = calls - last_timed_calls_;
    const auto period_ns = ns - last_ns_;
    if (period_calls > 0) {
      log::info("this period, PEF did nothing in {:.3} µs per witnessed call. efficiently.",
                static_cast<double>(period_ns) / 1000.0 / static_cast<double>(period_calls));
    }
    log::info("witnessed so far: {:,} frames, {:,} allocations; game state altered: {} times.",
              frames - last_frames_, allocations - last_allocations_, operations);
    last_timed_calls_ = calls;
    last_ns_ = ns;
    last_frames_ = frames;
    last_allocations_ = allocations;
  }
}

} // namespace pef::audit
