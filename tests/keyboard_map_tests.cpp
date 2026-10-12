#include "../engine/src/keyboard_map.hpp"
#include "test.hpp"

namespace {
using namespace engine;
using namespace testing;
struct Layout {
    std::array<XkbSymMapRec, 256> keys{};
    std::array<KeySym, 16> symbols{XK_A, XK_exclam,     XK_q, XK_at,
                                   XK_z, XK_numbersign, XK_w, XK_dollar};
    XkbClientMapRec map{};
    XkbDescRec keyboard{};
    Layout() {
        map.key_sym_map = keys.data();
        map.syms = symbols.data();
        map.num_syms = static_cast<unsigned short>(symbols.size());
        keyboard.min_key_code = 8;
        keyboard.max_key_code = 255;
        keyboard.map = &map;
        keys[38].group_info = 4;
        keys[38].width = 2;
    }
};
TEST(active_group_uses_its_own_unshifted_symbol) {
    Layout layout;
    auto cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[0][38] == 'a' && cache[1][38] == 'q');
    CHECK(cache[2][38] == 'z' && cache[3][38] == 'w');
    // Unsupported international symbols must not silently become a group-zero action.
    layout.symbols[2] = XK_Cyrillic_ya;
    cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[1][38] == XK_Cyrillic_ya);
    CHECK(!KeyBindings{}.action(static_cast<std::uint32_t>(cache[1][38])));
}
TEST(per_key_group_wrap_clamp_and_redirect) {
    Layout layout;
    auto& key = layout.keys[38];
    key.group_info = 2;
    auto cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[2][38] == 'a' && cache[3][38] == 'q');
    key.group_info = XkbSetGroupInfo(2, XkbClampIntoRange, 0);
    cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[2][38] == 'q' && cache[3][38] == 'q');
    key.group_info = XkbSetGroupInfo(2, XkbRedirectIntoRange, 1);
    cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[2][38] == 'q' && cache[3][38] == 'q');
    key.group_info = XkbSetGroupInfo(2, XkbRedirectIntoRange, 3);
    cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[2][38] == 'a' && cache[3][38] == 'a');
}
TEST(single_group_fixed_keys_and_keypad_survive_all_groups) {
    Layout layout;
    layout.keys[38].group_info = 1;
    for (const auto symbol :
         std::array<KeySym, 5>{XK_Left, XK_Shift_L, XK_F1, XK_KP_End, XK_KP_Separator}) {
        layout.symbols[0] = symbol;
        const auto cache = detail::keyboard_symbols(layout.keyboard);
        for (const auto& group : cache)
            CHECK(group[38] == canonical_keypad_symbol(static_cast<std::uint32_t>(symbol)));
    }
}
TEST(unmapped_keys_and_absent_symbols_stay_unmapped) {
    Layout layout;
    layout.symbols[2] = NoSymbol;
    auto cache = detail::keyboard_symbols(layout.keyboard);
    CHECK(cache[1][38] == NoSymbol && cache[1][37] == NoSymbol);
    layout.keys[38].offset = 16;
    CHECK(detail::keyboard_symbols(layout.keyboard)[0][38] == NoSymbol);
    layout.keyboard.map = nullptr;
    CHECK(detail::keyboard_symbols(layout.keyboard) == detail::KeyboardSymbols{});
}
} // namespace
int main() {
    return testing::run_tests();
}
