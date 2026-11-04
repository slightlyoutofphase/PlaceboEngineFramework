#include "Hooks/Installer.hpp"

#include <format>
#include <utility>

#include "Diagnostics/Log.hpp"
#include "Hooks/CallSiteScanner.hpp"
#include "Hooks/WitnessSlot.hpp"

// CommonLibSSE headers are written against their prefix header; the SKSE umbrella
// includes it first, so it precedes every individual CommonLib include in this file.
#include "SKSE/SKSE.h"

#include "RE/B/BSInputDeviceManager.h"
#include "RE/M/MemoryManager.h"
#include "REL/Relocation.h"

namespace pef::hooks {

namespace {

using telemetry::WitnessClass;

// The witness archetypes. The signatures are taken from CommonLibSSE's own declarations
// of the target functions, so the ABI is pinned to the library's, not to folklore:
//
//   RE::MemoryManager::Allocate(void*, std::size_t, std::int32_t, bool) -> void*
//   RE::MemoryManager::Reallocate(void*, void*, std::size_t, std::int32_t, bool) -> void*
//   RE::MemoryManager::Deallocate(void*, void*, bool) -> void
//   RE::BSInputDeviceManager::PollInputDevices(void*, float) -> void
//
// They are genuine function types, not function-pointer types: WitnessSlot constrains
// its Signature parameter to function types (std::is_function_v) and derives its own
// thunk_type pointer internally, so a pointer here would fail the constraint outright.
using AllocationSignature = void*(RE::MemoryManager*, std::size_t, std::int32_t, bool);
using ReallocationSignature = void*(RE::MemoryManager*, void*, std::size_t, std::int32_t, bool);
using DeallocationSignature = void(RE::MemoryManager*, void*, bool);
using FrameSignature = void(RE::BSInputDeviceManager*, float);

// Pool capacities: seats in the manifest. Capacity deliberately exceeds the budget so
// that the engine, not the installer, decides which hot call sites actually board.
inline constexpr std::size_t frame_pool_capacity{4};
inline constexpr std::size_t allocation_pool_capacity{32};
inline constexpr std::size_t reallocation_pool_capacity{8};
inline constexpr std::size_t deallocation_pool_capacity{8};

// Relocation pairs, transcribed from CommonLibSSE's own tables and verified present in
// every runtime's address library, including VR 1.4.15 (66859 / 66860 / 66861 / 67315
// all resolve there). RelocationID maps the SE identifier onto VR, which is precisely
// the zero-changes port we intend to keep taking credit for.
inline constexpr std::uint64_t allocate_se{66859}, allocate_ae{68115};
inline constexpr std::uint64_t reallocate_se{66860}, reallocate_ae{68116};
inline constexpr std::uint64_t deallocate_se{66861}, deallocate_ae{68117};
inline constexpr std::uint64_t poll_input_se{67315}, poll_input_ae{68617};

class Installer {
public:
  explicit Installer(std::size_t a_budget) : budget_{a_budget} {}

  [[nodiscard]] Installation run() {
    // The input poll that Main::Update performs every frame: the frame witness. It is
    // bound first so that "frames witnessed" is measured on the engine's own heartbeat,
    // and so the report can mean what it says when it says the engine kept running.
    bind_pool<FrameSignature, WitnessClass::frame, frame_pool_capacity>(
        REL::RelocationID{poll_input_se, poll_input_ae}.address(),
        "BSInputDeviceManager::PollInputDevices");

    // The memory manager, the works.
    bind_pool<AllocationSignature, WitnessClass::allocation, allocation_pool_capacity>(
        REL::RelocationID{allocate_se, allocate_ae}.address(), "MemoryManager::Allocate");
    bind_pool<ReallocationSignature, WitnessClass::allocation, reallocation_pool_capacity>(
        REL::RelocationID{reallocate_se, reallocate_ae}.address(), "MemoryManager::Reallocate");
    bind_pool<DeallocationSignature, WitnessClass::allocation, deallocation_pool_capacity>(
        REL::RelocationID{deallocate_se, deallocate_ae}.address(), "MemoryManager::Deallocate");

    return Installation{installed_, budget_, std::move(registry_)};
  }

private:
  template <class Signature, WitnessClass Class, std::size_t Capacity>
  void bind_pool(std::uintptr_t a_callee, std::string_view a_name) {
    if (a_callee == 0) {
      log::warning("relocation for {} did not resolve; pool skipped", a_name);
      return;
    }
    const auto sites = find_call_sites(a_callee, Capacity);
    if (sites.empty()) {
      log::warning("no call sites found for {}; the engine declined to be witnessed", a_name);
      return;
    }
    const auto before = installed_;
    bind_each<Signature, Class>(std::make_index_sequence<Capacity>{}, sites, a_name);
    log::info("{}: {} call site(s) discovered, {} bound as witnesses", a_name, sites.size(),
              installed_ - before);
  }

  template <class Signature, WitnessClass Class, std::size_t... Slots>
  void bind_each(std::index_sequence<Slots...>, const std::vector<std::uintptr_t>& a_sites,
                 std::string_view a_name) {
    (bind_one<Slots, Signature, Class>(a_sites, a_name), ...);
  }

  template <std::size_t Slot, class Signature, WitnessClass Class>
  void bind_one(const std::vector<std::uintptr_t>& a_sites, std::string_view a_name) {
    if (installed_ >= budget_ || Slot >= a_sites.size()) {
      return;
    }
    using Witness = WitnessSlot<Slot, Signature, Class>;
    auto& trampoline = SKSE::GetTrampoline();
    const auto original = reinterpret_cast<typename Witness::thunk_type>(
        trampoline.write_call<5>(a_sites[Slot], &Witness::hook));
    Witness::bind(a_sites[Slot], original);
    registry_.push_back(HookStats{
        std::format("{} @ {:#x}", a_name, a_sites[Slot]),
        Class,
        &Witness::state.witnessed,
        &Witness::state.spent_ns,
    });
    ++installed_;
  }

  std::size_t budget_;
  std::size_t installed_{0};
  std::vector<HookStats> registry_{};
};

} // namespace

Installation install(std::size_t a_budget) { return Installer{a_budget}.run(); }

} // namespace pef::hooks
