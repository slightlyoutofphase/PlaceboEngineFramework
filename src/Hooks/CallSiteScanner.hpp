#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

namespace pef::hooks {

/// Finds call sites in the main executable's code section: addresses of real five-byte
/// E8 rel32 CALL instructions whose target is a given engine function.
///
/// PEF does not curate these addresses from comment-section folklore. It reads the bytes
/// out of the running image and hooks what is actually there. An E8 site is exactly the
/// instruction Trampoline::write_call<5> knows how to replace, which makes the discovery
/// and the mechanism the same fact stated twice.
[[nodiscard]] std::vector<std::uintptr_t> find_call_sites(std::uintptr_t a_callee,
                                                          std::size_t a_limit);

} // namespace pef::hooks
