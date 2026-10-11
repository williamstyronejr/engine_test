#include "engine/window.hpp"
#include "test.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cstdlib>
#include <memory>
#include <unistd.h>

namespace {
using namespace engine;
using namespace testing;
struct Native {
    std::string name = "engine-chords-" + std::to_string(getpid());
    engine::Window window{64, 64, name, false};
    std::unique_ptr<Display, decltype(&XCloseDisplay)> display{XOpenDisplay(nullptr),
                                                               &XCloseDisplay};
    ::Window native{};
    Input input;
    Native() {
        CHECK(display != nullptr);
        ::Window root{}, parent{}, *children{};
        unsigned count{};
        CHECK(XQueryTree(display.get(), DefaultRootWindow(display.get()), &root, &parent, &children,
                         &count));
        for (unsigned i = 0; i < count; ++i) {
            char* title{};
            if (XFetchName(display.get(), children[i], &title) && title) {
                if (name == title)
                    native = children[i];
                XFree(title);
            }
        }
        if (children)
            XFree(children);
        CHECK(native != 0);
    }
    void send(XEvent& event, long mask) {
        CHECK(XSendEvent(display.get(), native, False, mask, &event));
        XSync(display.get(), False);
        window.poll(input);
    }
    void key(KeySym symbol, bool down) {
        XEvent e{};
        e.xkey.type = down ? KeyPress : KeyRelease;
        e.xkey.display = display.get();
        e.xkey.window = native;
        e.xkey.keycode = XKeysymToKeycode(display.get(), symbol);
        CHECK(e.xkey.keycode != 0);
        e.xkey.same_screen = True;
        send(e, down ? KeyPressMask : KeyReleaseMask);
    }
    void mouse(unsigned code, bool down) {
        XEvent e{};
        e.xbutton.type = down ? ButtonPress : ButtonRelease;
        e.xbutton.display = display.get();
        e.xbutton.window = native;
        e.xbutton.button = code;
        e.xbutton.x = 20;
        e.xbutton.y = 20;
        e.xbutton.same_screen = True;
        send(e, down ? ButtonPressMask : ButtonReleaseMask);
    }
    void configure(const KeyBindings& b) {
        window.set_bindings(b, input);
        input.consume();
    }
    void cancel(bool mapping) {
        XEvent e{};
        if (mapping) {
            e.xmapping.type = MappingNotify;
            e.xmapping.display = display.get();
            e.xmapping.window = native;
            e.xmapping.request = MappingKeyboard;
            e.xmapping.first_keycode = 8;
            e.xmapping.count = 248;
        } else {
            e.xfocus.type = FocusOut;
            e.xfocus.window = native;
        }
        send(e, mapping ? 0 : FocusChangeMask);
    }
};
TEST(chord_specificity_is_latched_until_base_release) {
    Native n;
    KeyBindings b;
    b.assign(0, 'e', modifiers::control);
    b.assign(1, 'e', modifiers::control | modifiers::shift);
    n.configure(b);
    n.key(XK_Control_L, true);
    n.key(XK_Shift_L, true);
    CHECK(!n.input.consume().pressed_symbol); // Modifier presses never consume capture.
    n.key(XK_e, true);
    auto f = n.input.consume();
    CHECK(button(f, Key::right).pressed && !button(f, Key::left).held &&
          !button(f, Key::interact).held);
    CHECK(f.pressed_symbol == 'e' &&
          f.pressed_modifiers == (modifiers::control | modifiers::shift));
    n.key(XK_Shift_L, false);
    n.key(XK_Control_L, false);
    f = n.input.consume();
    CHECK(button(f, Key::right).held && !button(f, Key::left).pressed &&
          !button(f, Key::interact).pressed);
    n.key(XK_e, true); // Repeat after modifiers change must not select the plain action.
    f = n.input.consume();
    CHECK(button(f, Key::right).held && !f.pressed_symbol && !button(f, Key::right).pressed);
    n.key(XK_e, false);
    CHECK(button(n.input.consume(), Key::right).released);
    n.key(XK_e, true);
    CHECK(button(n.input.consume(), Key::interact).pressed);
    n.key(XK_Control_L, true);
    f = n.input.consume();
    CHECK(button(f, Key::interact).held && !button(f, Key::left).pressed);
}
TEST(all_modifier_masks_and_right_hand_variants_capture) {
    Native n;
    for (std::uint32_t mask = 0; mask <= modifiers::all; ++mask) {
        KeyBindings b;
        b.assign(6, 'e', mask);
        n.configure(b);
        if (mask & modifiers::shift)
            n.key(XK_Shift_R, true);
        if (mask & modifiers::control)
            n.key(XK_Control_R, true);
        if (mask & modifiers::alt)
            n.key(XK_Alt_L, true);
        n.key(XK_e, true);
        n.key(XK_e, false);
        auto f = n.input.consume();
        CHECK(f.pressed_symbol == 'e' && f.pressed_modifiers == mask);
        CHECK(button(f, Key::interact).pressed && button(f, Key::interact).released);
        if (mask & modifiers::shift)
            n.key(XK_Shift_R, false);
        if (mask & modifiers::control)
            n.key(XK_Control_R, false);
        if (mask & modifiers::alt)
            n.key(XK_Alt_L, false);
        n.input.consume();
    }
}
TEST(left_right_modifier_aliases_and_missing_modifier) {
    Native n;
    KeyBindings b;
    b.assign(6, 'e', modifiers::control);
    n.configure(b);
    n.key(XK_e, true);
    n.key(XK_Control_L, true);
    CHECK(!button(n.input.consume(), Key::interact).held); // Base first is not a chord.
    n.key(XK_e, false);
    n.key(XK_Control_R, true);
    n.key(XK_Control_L, false);
    n.input.consume();
    n.key(XK_e, true);
    CHECK(button(n.input.consume(), Key::interact).pressed);
    n.key(XK_Control_R, false);
    n.key(XK_e, false);
    CHECK(button(n.input.consume(), Key::interact).released);
}
TEST(mouse_chords_keep_fixed_aliases_and_primary_ui_clicks) {
    Native n;
    KeyBindings b;
    b.assign(2, mouse_symbol(MouseButton::left), modifiers::alt);
    n.configure(b);
    n.key(XK_Alt_L, true);
    n.mouse(1, true);
    auto f = n.input.consume();
    CHECK(button(f, Key::up).pressed && f.pointer.primary.pressed);
    CHECK(f.pressed_symbol == mouse_symbol(MouseButton::left) &&
          f.pressed_modifiers == modifiers::alt);
    n.key(XK_Up, true);
    n.key(XK_Alt_L, false);
    n.mouse(1, false);
    f = n.input.consume();
    CHECK(button(f, Key::up).held && !button(f, Key::up).released && f.pointer.primary.released);
    n.key(XK_Up, false);
    CHECK(button(n.input.consume(), Key::up).released);
    n.mouse(1, true);
    n.mouse(1, false);
    f = n.input.consume();
    CHECK(!button(f, Key::up).pressed && f.pointer.primary.pressed);
}
TEST(remap_and_mapping_change_block_held_chords) {
    Native n;
    KeyBindings b;
    b.assign(0, 'e', modifiers::control);
    n.configure(b);
    n.key(XK_Control_L, true);
    n.key(XK_e, true);
    n.input.consume();
    b.assign(0, 'q');
    b.assign(6, 'e', modifiers::control);
    n.configure(b);
    n.key(XK_e, true);
    auto f = n.input.consume();
    CHECK(!f.pressed_symbol && !button(f, Key::interact).held);
    n.key(XK_e, false);
    n.key(XK_Control_L, false);
    n.key(XK_Control_L, true);
    n.key(XK_e, true);
    n.input.consume();
    n.cancel(true);
    f = n.input.consume();
    CHECK(f.canceled && !f.pressed_symbol && !f.pressed_modifiers &&
          !button(f, Key::interact).held);
    n.key(XK_e, true);
    CHECK(!button(n.input.consume(), Key::interact).held);
    n.key(XK_e, false);
    n.key(XK_Control_L, false);
    n.key(XK_Control_L, true);
    n.key(XK_e, true);
    CHECK(button(n.input.consume(), Key::interact).pressed);
    n.cancel(false);
    f = n.input.consume();
    CHECK(f.canceled && !f.pressed_modifiers && !button(f, Key::interact).held);
    n.key(XK_e, false);
    n.key(XK_Control_L, false);
    n.key(XK_Control_L, true);
    n.key(XK_e, true);
    CHECK(button(n.input.consume(), Key::interact).pressed);
}
TEST(capture_pairs_first_trigger_with_modifiers_and_fixed_keys_stay_accessible) {
    Native n;
    n.key(XK_Control_L, true);
    n.key(XK_e, true);
    n.key(XK_Control_L, false);
    n.mouse(3, true);
    auto f = n.input.consume();
    CHECK(f.pressed_symbol == 'e' && f.pressed_modifiers == modifiers::control);
    n.key(XK_Shift_L, true);
    n.key(XK_F1, true);
    f = n.input.consume();
    CHECK(button(f, Key::settings).pressed && f.pressed_modifiers == modifiers::shift);
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    return testing::run_tests();
}
