#include "Hooks/CallSiteScanner.hpp"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <Windows.h>

#include <cstring>
#include <optional>
#include <span>

namespace pef::hooks {

namespace {

struct CodeSection {
  std::uintptr_t address{};
  std::size_t size{};
};

/// The main executable's first executable code section, or nothing. Pure header walking:
/// no allocation, no locks, and no code executed that anyone could later blame us for.
[[nodiscard]] std::optional<CodeSection> first_code_section() noexcept {
  const auto* base = reinterpret_cast<const std::byte*>(::GetModuleHandleW(nullptr));
  if (base == nullptr) {
    return std::nullopt;
  }

  const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
  if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew < 0) {
    return std::nullopt;
  }

  const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(base + dos->e_lfanew);
  if (nt->Signature != IMAGE_NT_SIGNATURE ||
      nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
    return std::nullopt;
  }

  const auto* section = IMAGE_FIRST_SECTION(nt);
  for (std::size_t i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++section) {
    const auto executable = (section->Characteristics & IMAGE_SCN_MEM_EXECUTE) != 0;
    const auto code = (section->Characteristics & IMAGE_SCN_CNT_CODE) != 0;
    if (executable && code && section->Misc.VirtualSize >= 5) {
      return CodeSection{reinterpret_cast<std::uintptr_t>(base) + section->VirtualAddress,
                         section->Misc.VirtualSize};
    }
  }
  return std::nullopt;
}

} // namespace

std::vector<std::uintptr_t> find_call_sites(std::uintptr_t a_callee, std::size_t a_limit) {
  auto sites = std::vector<std::uintptr_t>{};
  if (a_limit == 0) {
    return sites;
  }
  const auto section = first_code_section();
  if (!section) {
    return sites;
  }

  const auto code = std::span{reinterpret_cast<const std::byte*>(section->address), section->size};
  for (std::size_t i = 0; i + 5 <= code.size() && sites.size() < a_limit; ++i) {
    if (code[i] != std::byte{0xE8}) {
      continue;
    }
    const auto site = section->address + i;
    auto displacement = std::int32_t{};
    std::memcpy(&displacement, code.data() + i + 1, sizeof(displacement));
    // E8 cd: CALL rel32, target = site + 5 + displacement. A byte that merely looks like
    // an E8 inside some other instruction would have to encode the exact distance to the
    // callee to pass this test; the trampoline's patch-safety check audits instruction
    // boundaries as a second opinion. We accept the residual risk because the alternative
    // is a hook count nobody can verify.
    const auto target = static_cast<std::uintptr_t>(static_cast<std::int64_t>(site) + 5 +
                                                    static_cast<std::int64_t>(displacement));
    if (target == a_callee) {
      sites.push_back(site);
    }
  }
  return sites;
}

} // namespace pef::hooks
