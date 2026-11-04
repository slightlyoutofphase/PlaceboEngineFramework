#pragma once

namespace SKSE {
class LoadInterface;
}

namespace pef {

/// The SKSE entry orchestration: settings, dependency integrity, the witness network,
/// the alibi handler, the audit thread, and the final report. Defined in Plugin.cpp —
/// the whole lifetime lives behind one function, because a plugin that does nothing
/// should at least be succinct about the order in which it does it.
[[nodiscard]] bool LoadPlugin(const SKSE::LoadInterface& a_skse);

} // namespace pef
