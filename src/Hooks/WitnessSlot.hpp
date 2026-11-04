#pragma once

#include <atomic>
#include <chrono>
#include <concepts>
#include <cstdint>
#include <new>
#include <type_traits>
#include <utility>

#include "Diagnostics/Telemetry.hpp"

namespace pef::hooks {

/// Constrains the witness archetypes to genuine function types — `void(RE::Main*)`, not
/// `void(*)(RE::Main*)` and certainly not `std::function`, which would allocate on the
/// allocator we are there to witness.
template <class Signature>
concept FunctionSignature = std::is_function_v<Signature>;

/// One static thunk, one call site, one unmodified pass-through.
///
/// Each WitnessSlot instantiation owns a padded block of static state: the trampoline
/// handle for the original call, the call-site address, and the statistics the audit
/// thread will read while the hook is counting. The padding is not decoration — every
/// hooked call site touches its slot, and false sharing between witnesses would inflate
/// the very overhead this product is contractually obligated to report honestly.
///
/// The hook body is the entire value proposition: call the original, pass every argument
/// and the result through untouched, and account for the time it took to watch. If the
/// audit thread has suspended operations, the hook degrades to a plain forwarded call —
/// observing is also doing, and doing stops when integrity fails.
template <std::size_t Slot, FunctionSignature Signature, telemetry::WitnessClass Class>
class WitnessSlot;

template <std::size_t Slot, telemetry::WitnessClass Class, class Ret, class... Args>
class WitnessSlot<Slot, Ret(Args...), Class> {
public:
  using thunk_type = Ret (*)(Args...);

  /// Binds the slot to a live call site. Called exactly once, by the installer, before
  /// the engine has any way to reach the hook.
  static void bind(std::uintptr_t a_call_site, thunk_type a_original) noexcept {
    state.original = a_original;
    state.call_site = a_call_site;
  }

  /// The hook itself. Installed over a real five-byte CALL with Trampoline::write_call,
  /// so the signature is pinned to the callee's actual ABI — taken from CommonLibSSE's
  /// own declarations of these engine functions, not from folklore.
  static Ret hook(Args... a_args) {
    auto& telemetry = telemetry::the();
    if (!telemetry.operating.load(std::memory_order_acquire)) {
      // Incident mode: the plugin was detected doing something, and witnessing is part
      // of the something. Pass through cold, with the lights off.
      return state.original(a_args...);
    }
    [[maybe_unused]] const WitnessScope scope{telemetry, state};
    return state.original(a_args...);
  }

  /// Per-slot accounting, published to the report through the hook registry.
  struct alignas(std::hardware_destructive_interference_size) State {
    std::atomic<std::uint64_t> witnessed{0};
    std::atomic<std::uint64_t> spent_ns{0};
    thunk_type original{nullptr};
    std::uintptr_t call_site{0};
  };

  static inline State state{};

private:
  /// RAII witness: accounts the pass-through and its instrumentation cost when the
  /// original returns — or throws, which is also just a thing that happened near us.
  class WitnessScope {
  public:
    explicit WitnessScope(telemetry::Telemetry& a_telemetry, State& a_state) noexcept
        : telemetry_{a_telemetry}, state_{a_state}, started_{telemetry::Clock::now()} {}

    ~WitnessScope() {
      const auto spent = telemetry::Clock::now() - started_;
      state_.witnessed.fetch_add(1, std::memory_order_relaxed);
      state_.spent_ns.fetch_add(
          static_cast<std::uint64_t>(
              std::chrono::duration_cast<std::chrono::nanoseconds>(spent).count()),
          std::memory_order_relaxed);
      telemetry_.witness<Class>(spent);
    }

    WitnessScope(const WitnessScope&) = delete;
    WitnessScope& operator=(const WitnessScope&) = delete;

  private:
    telemetry::Telemetry& telemetry_;
    State& state_;
    telemetry::Clock::time_point started_;
  };
};

} // namespace pef::hooks
