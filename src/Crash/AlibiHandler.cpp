#include "Crash/AlibiHandler.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <format>

#include "BuildInfo.hpp"
#include "Diagnostics/Format.hpp"
#include "Diagnostics/Telemetry.hpp"

namespace pef::crash {

namespace {

/// Exception codes that indicate the process is genuinely dying, rather than merely
/// throwing a recoverable C++ exception on its way through the vectored chain. The
/// game's first-chance exception traffic is heavy and boring; only these get an alibi.
constexpr std::uint32_t fatal_codes[] = {
    0xC0000005, // access violation
    0xC0000006, // in-page error
    0xC00000FD, // stack overflow
    0xC000001D, // illegal instruction
    0xC0000096, // privileged instruction
    0xC0000374, // heap corruption
    0xC0000409, // fast-fail / stack buffer overrun
};

/// Minimum spacing between alibi filings, in milliseconds. A dying process can raise a
/// parade of exceptions; the alibi is filed once per parade, not once per float.
constexpr std::uint64_t refactory_period_ms{2000};

/// Staged at install time, read under acquire ordering inside the handler. A fixed
/// buffer instead of a std::wstring, because the handler does not allocate.
constinit std::array<wchar_t, 512> alibi_path{};
constinit std::atomic<std::size_t> alibi_path_length{0};
constinit std::atomic<void*> handler_handle{nullptr};
constinit std::atomic<std::uint64_t> last_filed_ms{0};

[[nodiscard]] bool is_fatal(std::uint32_t a_code) noexcept {
  return std::ranges::contains(fatal_codes, a_code);
}

/// Renders and writes the alibi. Everything here is reentrant by construction: stack
/// storage, integer formatting, and three Win32 calls that do not allocate.
void file_alibi(std::uint32_t a_code) noexcept {
  auto& telemetry = telemetry::the();
  const auto frames = telemetry.frames_witnessed.load(std::memory_order_relaxed);
  const auto alibis = telemetry.alibis_filed.load(std::memory_order_relaxed) + 1;
  const auto operating = telemetry.operating.load(std::memory_order_acquire);
  const auto operations = telemetry.operations_performed.load(std::memory_order_acquire);

  auto text = std::array<char, 512>{};
  const auto out = std::format_to_n(
      text.data(), text.size() - 1,
      "PEF status at time of crash: {}.\n"
      "Time spent altering game state: {}.\n"
      "Suspects: everyone else.\n"
      "\n"
      "Exception code:   0x{:08X}\n"
      "Alibi filed:      #{} (priority 1 — ahead of every other crash logger)\n"
      "Frames witnessed: {}\n"
      "This statement was prepared before the crash, which is what an alibi is.\n",
      operating ? "dormant" : "UNDER INVESTIGATION — see PEF_Incident.txt",
      operations == 0 ? "0.000 ms" : "nonzero, unquantified, and deeply embarrassing", a_code,
      alibis, format::grouped(frames));
  *out.out = '\0';

  const auto file = ::CreateFileW(alibi_path.data(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS,
                                  FILE_ATTRIBUTE_NORMAL, nullptr);
  if (file == INVALID_HANDLE_VALUE) {
    return; // The alibi was prepared; the medium declined. Nothing further is owed.
  }
  auto written = DWORD{0};
  ::WriteFile(file, text.data(), static_cast<DWORD>(out.size), &written, nullptr);
  ::CloseHandle(file);
  telemetry.alibis_filed.store(alibis, std::memory_order_relaxed);
}

} // namespace

long WINAPI AlibiHandler::on_exception(PEXCEPTION_POINTERS a_info) noexcept {
  auto& telemetry = telemetry::the();
  telemetry.exceptions_witnessed.fetch_add(1, std::memory_order_relaxed);

  if (alibi_path_length.load(std::memory_order_acquire) == 0) {
    return EXCEPTION_CONTINUE_SEARCH;
  }
  if (a_info != nullptr && a_info->ExceptionRecord != nullptr) {
    const auto code = static_cast<std::uint32_t>(a_info->ExceptionRecord->ExceptionCode);
    const auto now = ::GetTickCount64();
    // Writable by compare_exchange_strong: the operation stores the observed value
    // into it when the comparison fails, so it cannot be const.
    auto last = last_filed_ms.load(std::memory_order_relaxed);
    if (is_fatal(code) && (last == 0 || now - last >= refactory_period_ms)) {
      if (last_filed_ms.compare_exchange_strong(last, now, std::memory_order_relaxed,
                                                std::memory_order_relaxed)) {
        file_alibi(code);
      }
    }
  }
  return EXCEPTION_CONTINUE_SEARCH;
}

bool AlibiHandler::install(std::filesystem::path a_alibi_path) {
  const auto text = a_alibi_path.wstring();
  if (text.size() >= alibi_path.size()) {
    return false;
  }
  std::ranges::copy(text, alibi_path.data());
  alibi_path[text.size()] = L'\0';
  alibi_path_length.store(text.size(), std::memory_order_release);

  const auto handle = ::AddVectoredExceptionHandler(1, &AlibiHandler::on_exception);
  handler_handle.store(handle, std::memory_order_release);
  return handle != nullptr;
}

void AlibiHandler::uninstall() noexcept {
  const auto handle = handler_handle.exchange(nullptr, std::memory_order_acq_rel);
  if (handle != nullptr) {
    ::RemoveVectoredExceptionHandler(handle);
  }
  alibi_path_length.store(0, std::memory_order_release);
}

} // namespace pef::crash
