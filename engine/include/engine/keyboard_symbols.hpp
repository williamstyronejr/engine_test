#pragma once
#include <cstdint>
#include <string>

namespace engine {
// Normalize exact legacy X11/Unicode character aliases. Non-character functions
// pass through. ASCII letters use lowercase; non-ASCII case is preserved.
std::uint32_t canonical_character_symbol(std::uint32_t symbol);
// Canonical Latin-1/Unicode letters, numbers, punctuation and symbols (Unicode 16).
// Excludes controls, whitespace, combining marks, format/private/unassigned values.
bool is_international_binding_symbol(std::uint32_t symbol);
std::string international_symbol_name(std::uint32_t symbol);
} // namespace engine
