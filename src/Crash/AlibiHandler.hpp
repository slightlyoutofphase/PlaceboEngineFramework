#pragma once

// Self-contained: the handler's signature is expressed in Win32 types, so the header
// pulls them itself rather than relying on the include order of whoever includes it.
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>

#include <filesystem>

namespace pef::crash {

/// The first responder: a priority-1 vectored exception handler that executes before
/// every other crash logger in the process, files PEF's alibi, and then returns
/// EXCEPTION_CONTINUE_SEARCH because the crash belongs to somebody else.
///
/// The handler is written to a discipline suitable for code that runs on a thread in
/// the middle of dying: no locks, no heap, no spdlog (its sinks take mutexes, and a
/// mutex on a crashing thread is how one crash becomes two). The alibi text is rendered
/// into a stack buffer and written with one CreateFileW / WriteFile / CloseHandle.
class AlibiHandler {
public:
  /// Installs the handler and stages the alibi path. The path is rendered eagerly —
  /// inside the handler there will be time for exactly one file, not one decision.
  /// Priority 1 means first in the vectored chain: ahead of Crash Logger, ahead of
  /// Trainwreck, ahead of everyone whose fault this actually is.
  [[nodiscard]] static bool install(std::filesystem::path a_alibi_path);

  /// Removes the handler. Called during finalization — the only moment at which it is
  /// statistically safe to assume nobody is currently crashing.
  static void uninstall() noexcept;

private:
  static long WINAPI on_exception(PEXCEPTION_POINTERS a_info) noexcept;
};

} // namespace pef::crash
