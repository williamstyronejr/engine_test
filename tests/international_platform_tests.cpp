#include "../examples/feature_lab/persistence.hpp"
#include "native_input.hpp"
#include <cstdlib>

// Test-only link interposition: alter a client-owned XKB snapshot, never the server
// or desktop layout. The production Window still receives real queued X11 events.
extern "C" XkbDescPtr __real_XkbGetMap(Display*, unsigned int, unsigned int);
extern "C" XkbDescPtr __wrap_XkbGetMap(Display* display, unsigned int which, unsigned int device) {
    auto* map = __real_XkbGetMap(display, which, device);
    if (!map)
        return nullptr;
    const auto code = XKeysymToKeycode(display, XK_q);
    auto* symbols = XkbResizeKeySyms(map, code, 4);
    if (!code || !symbols) {
        XkbFreeKeyboard(map, XkbAllComponentsMask, True);
        return nullptr;
    }
    auto& key = map->map->key_sym_map[code];
    key.group_info = 4;
    key.width = 1;
    symbols[0] = XK_eacute;
    symbols[1] = XK_Greek_alpha;
    symbols[2] = 0x010003b1; // Same character with its direct Unicode encoding.
    symbols[3] = 0x01004e2d;
    return map;
}
namespace {
using namespace engine;
using namespace testing;
using namespace feature_lab;
TEST(native_active_groups_capture_canonical_international_chords) {
    NativeInput n;
    for (unsigned int group = 0; group < 4; ++group) {
        const auto symbol = group == 0 ? 0xe9U : group == 3 ? 0x01004e2dU : 0x010003b1U;
        KeyBindings b;
        b.assign(6, symbol, modifiers::control);
        n.configure(b);
        const auto state = static_cast<unsigned int>(XkbBuildCoreState(0, group));
        // Establish the group before pressing the modifier (a change cancels holds).
        n.key(XK_F1, true, state);
        n.key(XK_F1, false, state);
        n.input.consume();
        n.key(XK_Control_L, true, state);
        n.key(XK_q, true, state);
        auto frame = n.input.consume();
        CHECK(frame.pressed_symbol == symbol && frame.pressed_modifiers == modifiers::control);
        CHECK(button(frame, Key::interact).pressed);
        n.key(XK_q, false, state);
        n.key(XK_Control_L, false, state);
        CHECK(button(n.input.consume(), Key::interact).released);
    }
}
TEST(native_international_group_transition_blocks_repeat_until_release) {
    NativeInput n;
    KeyBindings b;
    b.assign(0, 0xe9);
    b.assign(1, 0x010003b1);
    n.configure(b);
    n.key(XK_q, true);
    CHECK(button(n.input.consume(), Key::left).pressed);
    n.key(XK_q, true, XkbBuildCoreState(0, 1));
    auto frame = n.input.consume();
    CHECK(frame.canceled && !frame.pressed_symbol);
    CHECK(!button(frame, Key::left).held && !button(frame, Key::right).held);
    n.key(XK_q, false, XkbBuildCoreState(0, 1));
    n.key(XK_q, true, XkbBuildCoreState(0, 1));
    CHECK(button(n.input.consume(), Key::right).pressed);
}
TEST(feature_lab_captures_labels_and_roundtrips_international_input) {
    NativeInput n;
    Game game = load_game(AssetRoot(TEST_ASSET_ROOT));
    Settings settings;
    n.key(XK_F1, true);
    settings.update(game, n.input.consume(), 640, 480, false);
    n.key(XK_F1, false);
    n.input.consume();
    const auto click = [&](UiId id) {
        Rect bounds{};
        bool found = false;
        for (const auto& widget : settings.ui().widgets())
            if (widget.id == id) {
                bounds = widget.bounds;
                found = true;
            }
        CHECK(found);
        Input pointer;
        pointer.move_pointer((bounds.min + bounds.max) * 0.5F);
        pointer.set_primary(true);
        settings.update(game, pointer.consume(), 640, 480, false);
        pointer.set_primary(false);
        settings.update(game, pointer.consume(), 640, 480, false);
    };
    click(Settings::controls);
    click(Settings::binding_first);
    n.key(XK_q, true);
    CHECK(settings.update(game, n.input.consume(), 640, 480, false).bindings_changed);
    CHECK(settings.bindings.key_name(Key::left) == "EACUTE");
    Configuration config;
    config.bindings = settings.bindings;
    CHECK(decode_config(encode_config(config)).bindings == settings.bindings);
    n.window.set_bindings(settings.bindings, n.input);
    n.key(XK_q, true);
    CHECK(!button(n.input.consume(), Key::left).held);
    n.key(XK_q, false);
    n.key(XK_q, true);
    CHECK(button(n.input.consume(), Key::left).pressed);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
