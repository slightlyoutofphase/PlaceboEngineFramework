#pragma once

#include <atomic>
#include <chrono>
#include <cstdint>
#include <mutex>
#include <optional>
#include <source_location>
#include <string>

namespace pef::telemetry {

/// The one clock that matters: monotonic, steady, and indifferent to load screens.
using Clock = std::chrono::steady_clock;

/// What a witness hook saw. The distinction exists because "frames witnessed" and
/// "allocations witnessed" sound different in the shutdown report, and sounding
/// different is half the product.
enum class WitnessClass : std::uint8_t {
  frame,
  allocation,
};

/// The global proof-of-nothing ledger. Every member is safe to touch from the audit
/// thread, from hook thunks on any game thread, and — for the atomic members only —
/// from the vectored exception handler, which runs on a thread that is having the worst
/// day of its life and must not be asked to take a lock.
struct Telemetry {
  std::atomic<std::uint64_t> frames_witnessed{0};
  std::atomic<std::uint64_t> allocations_witnessed{0};
  std::atomic<std::uint64_t> operations_performed{0};
  std::atomic<std::uint64_t> exceptions_witnessed{0};
  std::atomic<std::uint64_t> alibis_filed{0};
  std::atomic<std::uint64_t> instrumentation_ns{0};
  std::atomic<std::uint64_t> timed_calls{0};

  /// True while PEF is productively doing nothing. The audit thread clears it if the
  /// operations counter ever moves, which suspends witnessing immediately: doing nothing
  /// must stop the moment anything starts.
  std::atomic<bool> operating{true};

  /// Records one pass-through observation. Relaxed ordering throughout: these are
  /// statistics, not synchronization, and no reader derives ordering from them. The
  /// instrumentation time is charged to the product, because the product is the reason
  /// the clock was started.
  template <WitnessClass Class> void witness(Clock::duration a_instrumentation) noexcept {
    const auto ns = std::chrono::duration_cast<std::chrono::nanoseconds>(a_instrumentation).count();
    instrumentation_ns.fetch_add(static_cast<std::uint64_t>(ns), std::memory_order_relaxed);
    timed_calls.fetch_add(1, std::memory_order_relaxed);
    if constexpr (Class == WitnessClass::frame) {
      frames_witnessed.fetch_add(1, std::memory_order_relaxed);
    } else {
      allocations_witnessed.fetch_add(1, std::memory_order_relaxed);
    }
  }

  /// The only call that could make PEF do something. Nothing calls it. It exists so that
  /// if anything ever does, the incident report can name the exact line that broke the
  /// product's one promise — arming the alarm is not the same as triggering it.
  void register_operation(std::source_location a_location = std::source_location::current());

  /// Snapshot of the offender recorded by the last register_operation() call, if the
  /// impossible ever happens. Copied under the mutex, away from any crashing thread.
  [[nodiscard]] std::optional<std::string> offender_snapshot() const;

private:
  mutable std::mutex offender_mutex{};
  std::optional<std::string> offender{};
};

/// Process-wide telemetry. Hook thunks are free functions invoked by the engine with no
/// receiver, so a singleton is the honest shape for state they must be able to reach.
Telemetry& the() noexcept;

} // namespace pef::telemetry
