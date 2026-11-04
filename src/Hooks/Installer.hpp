#pragma once

#include <atomic>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Diagnostics/Telemetry.hpp"

namespace pef::hooks {

/// A read-only view of one installed witness, assembled once at install time — before
/// the audit thread exists and before any hook can run, so no synchronization is needed
/// for the pointers, and none is taken for granted either.
struct HookStats {
  std::string name;
  telemetry::WitnessClass witness_class{};
  const std::atomic<std::uint64_t>* witnessed{};
  const std::atomic<std::uint64_t>* spent_ns{};
};

/// The result of arming the witness network.
struct Installation {
  std::size_t installed{0};
  std::size_t budget{0};
  std::vector<HookStats> registry{};
};

/// Discovers and installs the hook budget: every hook is a five-byte call-site patch
/// over a real E8 CALL, trampolined by SKSE, passing everything through unmodified.
///
/// The targets are the engine's own hot functions, pinned by relocations from
/// CommonLibSSE's tables, and the call sites are found by scanning the running image —
/// which is why the witness network is identical on SE, AE, and VR without a single
/// conditional in this file. Porting to VR required zero changes, which we list as a
/// feature anyway.
[[nodiscard]] Installation install(std::size_t a_budget);

} // namespace pef::hooks
