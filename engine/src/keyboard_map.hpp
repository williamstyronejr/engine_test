#pragma once
#include "engine/bindings.hpp"
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <array>

namespace engine::detail {
// Immutable, bounded cache: group changes and key events need no server round trip.
using KeyboardSymbols = std::array<std::array<KeySym, 256>, XkbNumKbdGroups>;
inline KeyboardSymbols keyboard_symbols(const XkbDescRec& keyboard) {
    KeyboardSymbols result{};
    const auto* map = keyboard.map;
    if (!map || !map->key_sym_map || !map->syms)
        return result;
    for (unsigned int code = keyboard.min_key_code; code <= keyboard.max_key_code; ++code) {
        const auto& key = map->key_sym_map[code];
        const auto count = static_cast<unsigned int>(XkbNumGroups(key.group_info));
        if (!count || count > XkbNumKbdGroups || !key.width)
            continue;
        for (unsigned int group = 0; group < result.size(); ++group) {
            auto effective = group;
            if (effective >= count) {
                switch (XkbOutOfRangeGroupAction(key.group_info)) {
                case XkbClampIntoRange:
                    effective = count - 1;
                    break;
                case XkbRedirectIntoRange:
                    effective = XkbOutOfRangeGroupNumber(key.group_info);
                    if (effective >= count)
                        effective = 0;
                    break;
                default:
                    effective %= count;
                    break;
                }
            }
            // Level zero deliberately ignores Shift, Caps Lock and Num Lock.
            const auto offset = key.offset + effective * key.width;
            if (offset >= map->num_syms)
                continue;
            const auto symbol = canonical_keypad_symbol(
                canonical_character_symbol(static_cast<std::uint32_t>(map->syms[offset])));
            result[group][code] = symbol;
        }
    }
    return result;
}
} // namespace engine::detail
