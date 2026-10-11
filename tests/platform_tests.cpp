#include "engine/window.hpp"
#include <X11/Xlib.h>
#include <X11/keysym.h>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <unistd.h>

namespace {
void check(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
::Window find_window(Display* display, const std::string& name) {
    ::Window root{}, parent{}, *children{};
    unsigned int count{};
    check(XQueryTree(display, DefaultRootWindow(display), &root, &parent, &children, &count),
          "Cannot query test window");
    ::Window found{};
    for (unsigned int i = 0; i < count; ++i) {
        char* title{};
        if (XFetchName(display, children[i], &title) && title) {
            if (name == title)
                found = children[i];
            XFree(title);
        }
    }
    if (children)
        XFree(children);
    check(found != 0, "Hidden test window was not found");
    return found;
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY")) {
        std::cout << "SKIP: DISPLAY unavailable\n";
        return 77;
    }
    try {
        const std::string name = "engine-events-" + std::to_string(getpid());
        engine::Window window(64, 64, name, false);
        std::unique_ptr<Display, decltype(&XCloseDisplay)> connection(XOpenDisplay(nullptr),
                                                                      &XCloseDisplay);
        check(connection != nullptr, "Test X11 connection failed");
        Display* display = connection.get();
        const auto native = find_window(display, name);
        engine::Input input;
        auto send_key = [&](KeySym key, bool down, unsigned int state = 0) {
            XEvent event{};
            event.xkey.type = down ? KeyPress : KeyRelease;
            event.xkey.display = display;
            event.xkey.window = native;
            event.xkey.keycode = XKeysymToKeycode(display, key);
            check(event.xkey.keycode != 0, "Test key unavailable in native keymap");
            event.xkey.state = state;
            event.xkey.same_screen = True;
            check(XSendEvent(display, native, False, down ? KeyPressMask : KeyReleaseMask, &event),
                  "Key send failed");
            XSync(display, False);
            window.poll(input);
        };
        // Changing a binding while held must not generate a new action or raw press.
        send_key(XK_q, true);
        check(input.consume().pressed_symbol == 'q', "Unbound raw press missing");
        engine::KeyBindings configured;
        configured.assign(2, 'q');
        window.set_bindings(configured, input);
        input.consume();
        send_key(XK_q, true);
        auto rebound = input.consume();
        check(!engine::button(rebound, engine::Key::up).held && !rebound.pressed_symbol,
              "Held rebind generated input");
        send_key(XK_q, false);
        input.consume();
        send_key(XK_q, true);
        rebound = input.consume();
        check(engine::button(rebound, engine::Key::up).pressed && rebound.pressed_symbol == 'q',
              "Rebound press missing");
        send_key(XK_Up, true);
        send_key(XK_q, false);
        check(engine::button(input.consume(), engine::Key::up).held,
              "Rebound alias released early");
        send_key(XK_Up, false);
        input.consume();
        send_key(XK_w, true);
        check(!engine::button(input.consume(), engine::Key::up).held, "Old binding still active");
        send_key(XK_w, false);
        input.consume();
        auto invalid = configured;
        invalid.symbols[0] = 'q';
        bool rejected = false;
        try {
            window.set_bindings(invalid, input);
        } catch (const std::invalid_argument&) {
            rejected = true;
        }
        check(rejected, "Conflicting native configuration accepted");
        send_key(XK_q, true);
        check(engine::button(input.consume(), engine::Key::up).held,
              "Rejected table replaced live bindings");
        // A mapping notification cancels held input without modifying the desktop keymap.
        XEvent mapping{};
        mapping.xmapping.type = MappingNotify;
        mapping.xmapping.display = display;
        mapping.xmapping.window = native;
        mapping.xmapping.request = MappingKeyboard;
        mapping.xmapping.first_keycode = 8;
        mapping.xmapping.count = 248;
        check(XSendEvent(display, native, False, 0, &mapping), "Mapping event send failed");
        XSync(display, False);
        window.poll(input);
        check(input.consume().canceled, "Mapping change did not cancel input");
        send_key(XK_q, true);
        check(!engine::button(input.consume(), engine::Key::up).held,
              "Mapping change leaked held input");
        send_key(XK_q, false);
        window.set_bindings({}, input);
        input.consume();
        send_key(XK_w, true);
        check(engine::button(input.consume(), engine::Key::up).pressed, "Native key press missing");
        send_key(XK_w, true);
        check(!engine::button(input.consume(), engine::Key::up).pressed,
              "Autorepeat became a new press");
        send_key(XK_Up, true);
        send_key(XK_w, false);
        check(engine::button(input.consume(), engine::Key::up).held,
              "Releasing one alias released the other");
        send_key(XK_Up, false);
        check(engine::button(input.consume(), engine::Key::up).released,
              "Native key release missing");
        send_key(XK_Page_Down, true);
        check(engine::button(input.consume(), engine::Key::panel_down).held,
              "PageDown mapping failed");
        send_key(XK_Page_Down, false);
        send_key(XK_Page_Up, true);
        check(engine::button(input.consume(), engine::Key::panel_up).pressed,
              "PageUp mapping failed");
        send_key(XK_Page_Up, false);
        send_key(XK_m, true);
        check(engine::button(input.consume(), engine::Key::mute).pressed,
              "Mute key mapping failed");
        send_key(XK_m, false);
        send_key(XK_n, true);
        check(engine::button(input.consume(), engine::Key::music_volume).pressed,
              "Music volume mapping failed");
        send_key(XK_n, false);
        send_key(XK_b, true);
        check(engine::button(input.consume(), engine::Key::effects_volume).pressed,
              "Effects volume mapping failed");
        send_key(XK_b, false);
        for (const auto [symbol, key] : {std::pair{XK_F1, engine::Key::settings},
                                         {XK_Tab, engine::Key::tab},
                                         {XK_Shift_L, engine::Key::shift},
                                         {XK_Return, engine::Key::accept},
                                         {XK_space, engine::Key::space},
                                         {XK_e, engine::Key::interact},
                                         {XK_F2, engine::Key::diagnostics},
                                         {XK_F3, engine::Key::debug_shapes},
                                         {XK_F10, engine::Key::single_step},
                                         {XK_F5, engine::Key::reload_shaders},
                                         {XK_F6, engine::Key::shader_error},
                                         {XK_F7, engine::Key::reload_textures},
                                         {XK_F8, engine::Key::texture_error}}) {
            send_key(static_cast<KeySym>(symbol), true);
            check(engine::button(input.consume(), key).pressed, "UI key mapping failed");
            send_key(static_cast<KeySym>(symbol), false);
            input.consume();
        }
        // The physical key's unshifted group-zero symbol selects the binding.
        const std::array<KeySym, 27> extended_keys{XK_0,
                                                   XK_1,
                                                   XK_2,
                                                   XK_3,
                                                   XK_4,
                                                   XK_5,
                                                   XK_6,
                                                   XK_7,
                                                   XK_8,
                                                   XK_9,
                                                   XK_grave,
                                                   XK_bracketleft,
                                                   XK_bracketright,
                                                   XK_backslash,
                                                   XK_semicolon,
                                                   XK_apostrophe,
                                                   XK_comma,
                                                   XK_period,
                                                   XK_slash,
                                                   XK_BackSpace,
                                                   XK_Home,
                                                   XK_End,
                                                   XK_Insert,
                                                   XK_Delete,
                                                   XK_F4,
                                                   XK_F9,
                                                   XK_F12};
        for (const auto symbol : extended_keys) {
            configured = {};
            configured.assign(2, static_cast<std::uint32_t>(symbol));
            window.set_bindings(configured, input);
            input.consume();
            send_key(symbol, true, ShiftMask); // Shift does not turn 1 into a distinct ! binding.
            auto frame = input.consume();
            check(engine::button(frame, engine::Key::up).pressed && frame.pressed_symbol == symbol,
                  "Extended key mapping/capture failed");
            send_key(symbol, true);
            frame = input.consume();
            check(!engine::button(frame, engine::Key::up).pressed && !frame.pressed_symbol,
                  "Extended key repeat generated input");
            send_key(XK_Up, true);
            send_key(symbol, false);
            check(engine::button(input.consume(), engine::Key::up).held,
                  "Extended key released fixed alias");
            send_key(XK_Up, false);
            check(engine::button(input.consume(), engine::Key::up).released,
                  "Extended key alias remained stuck");
            send_key(symbol, true);
            input.consume();
            configured.assign(2, 'w');
            configured.assign(6, static_cast<std::uint32_t>(symbol));
            window.set_bindings(configured, input);
            input.consume();
            send_key(symbol, true);
            frame = input.consume();
            check(!engine::button(frame, engine::Key::interact).held && !frame.pressed_symbol,
                  "Held extended rebind leaked input");
            send_key(symbol, false);
            input.consume();
            send_key(symbol, true);
            send_key(symbol, false);
            frame = input.consume();
            const auto& action = engine::button(frame, engine::Key::interact);
            check(action.pressed && action.released && !action.held,
                  "Fast extended key press lost action edges");
        }
        window.set_bindings({}, input);
        input.consume();
        const auto send_button = [&](unsigned int button, bool down, int x, int y) {
            XEvent event{};
            event.xbutton.type = down ? ButtonPress : ButtonRelease;
            event.xbutton.display = display;
            event.xbutton.window = native;
            event.xbutton.button = button;
            event.xbutton.x = x;
            event.xbutton.y = y;
            event.xbutton.same_screen = True;
            check(XSendEvent(display, native, False, down ? ButtonPressMask : ButtonReleaseMask,
                             &event),
                  "Pointer send failed");
            XSync(display, False);
            window.poll(input);
        };
        send_button(Button1, true, 12, 15);
        send_button(Button1, false, 28, 30);
        auto pointer = input.consume().pointer;
        check(pointer.primary.pressed && pointer.primary.released && !pointer.primary.held,
              "Fast native pointer click missing");
        check(pointer.press_position.x == 12 && pointer.release_position.x == 28,
              "Native click positions lost");
        send_button(Button4, true, 28, 30);
        send_button(Button4, false, 28, 30);
        check(input.consume().pointer.wheel == 1, "Native scroll mapping failed");
        // All supported buttons map to actions; wheel/unknown buttons never become bindings.
        for (const auto [native_button, symbol] : {std::pair{1U, engine::MouseButton::left},
                                                   {2U, engine::MouseButton::middle},
                                                   {3U, engine::MouseButton::right},
                                                   {8U, engine::MouseButton::side1},
                                                   {9U, engine::MouseButton::side2}}) {
            configured = {};
            configured.assign(2, engine::mouse_symbol(symbol));
            window.set_bindings(configured, input);
            input.consume();
            send_button(native_button, true, 20, 20);
            auto frame = input.consume();
            check(engine::button(frame, engine::Key::up).pressed &&
                      frame.pressed_symbol == engine::mouse_symbol(symbol),
                  "Mouse binding press missing");
            send_button(native_button, true, 20, 20);
            frame = input.consume();
            check(!engine::button(frame, engine::Key::up).pressed && !frame.pressed_symbol,
                  "Duplicate mouse down generated a press");
            send_key(XK_Up, true);
            input.consume();
            send_button(native_button, false, 20, 20);
            frame = input.consume();
            check(engine::button(frame, engine::Key::up).held &&
                      !engine::button(frame, engine::Key::up).released,
                  "Mouse release canceled held keyboard alias");
            send_button(native_button, true, 20, 20);
            input.consume();
            send_key(XK_Up, false);
            frame = input.consume();
            check(engine::button(frame, engine::Key::up).held &&
                      !engine::button(frame, engine::Key::up).released,
                  "Keyboard release canceled held mouse alias");
            // Remapping a held mouse suppresses raw capture, gameplay and primary UI input.
            configured.assign(2, 'w');
            configured.assign(6, engine::mouse_symbol(symbol));
            window.set_bindings(configured, input);
            check(input.consume().canceled, "Mouse rebind did not cancel input");
            send_button(native_button, true, 20, 20);
            frame = input.consume();
            check(!engine::button(frame, engine::Key::interact).held && !frame.pressed_symbol &&
                      !frame.pointer.primary.held,
                  "Held mouse rebind leaked input");
            send_button(native_button, false, 20, 20);
            frame = input.consume();
            check(!frame.pointer.primary.released, "Rebind release synthesized UI click");
            send_button(native_button, true, 20, 20);
            send_button(native_button, false, 20, 20);
            frame = input.consume();
            const auto& action = engine::button(frame, engine::Key::interact);
            check(action.pressed && action.released && !action.held,
                  "Fast bound mouse click lost action edges");
        }
        for (unsigned int unsupported : {4U, 5U, 6U, 7U, 10U, 255U}) {
            send_button(unsupported, true, 20, 20);
            send_button(unsupported, false, 20, 20);
            check(!input.consume().pressed_symbol, "Unsupported mouse button became raw binding");
        }
        // Keyboard and mouse share an ordered, bounded capture slot.
        send_button(Button3, true, 20, 20);
        send_key(XK_q, true);
        check(input.consume().pressed_symbol == engine::mouse_symbol(engine::MouseButton::right),
              "First mouse press was overwritten by a key");
        send_button(Button3, false, 20, 20);
        send_key(XK_q, false);
        input.consume();
        send_key(XK_q, true);
        send_button(Button3, true, 20, 20);
        check(input.consume().pressed_symbol == 'q', "First key press was overwritten by mouse");
        send_button(Button3, false, 20, 20);
        send_key(XK_q, false);
        input.consume();
        configured = {};
        configured.assign(6, engine::mouse_symbol(engine::MouseButton::left));
        window.set_bindings(configured, input);
        input.consume();
        send_button(Button1, true, 20, 20);
        input.consume();
        check(XSendEvent(display, native, False, 0, &mapping), "Mouse mapping event failed");
        XSync(display, False);
        window.poll(input);
        check(input.consume().canceled, "Mapping change did not cancel mouse input");
        send_button(Button1, true, 20, 20);
        auto blocked_mouse = input.consume();
        check(!engine::button(blocked_mouse, engine::Key::interact).held &&
                  !blocked_mouse.pointer.primary.held && !blocked_mouse.pressed_symbol,
              "Mapping change leaked held mouse input");
        send_button(Button1, false, 20, 20);
        input.consume();
        XEvent motion{};
        motion.xmotion.type = MotionNotify;
        motion.xmotion.window = native;
        motion.xmotion.x = 45;
        motion.xmotion.y = 50;
        XSendEvent(display, native, False, PointerMotionMask, &motion);
        XSync(display, False);
        window.poll(input);
        pointer = input.consume().pointer;
        check(pointer.position.x == 45 && pointer.position.y == 50 && pointer.inside,
              "Native pointer motion missing");
        send_button(Button1, true, 20, 20);
        send_key(XK_d, true);
        XEvent focus{};
        focus.xfocus.type = FocusOut;
        focus.xfocus.window = native;
        XSendEvent(display, native, False, FocusChangeMask, &focus);
        XSync(display, False);
        window.poll(input);
        const auto lost = input.consume();
        check(!engine::button(lost, engine::Key::right).held &&
                  !engine::button(lost, engine::Key::right).pressed,
              "Focus loss left stuck input");
        check(lost.canceled && !lost.pointer.primary.held && !lost.pointer.primary.released,
              "Focus loss synthesized a click");
        check(!engine::button(lost, engine::Key::interact).held && !lost.pressed_symbol,
              "Focus loss left mouse action or capture input");
        send_button(Button1, false, 20, 20);
        input.consume();
        send_button(Button1, true, 20, 20);
        check(engine::button(input.consume(), engine::Key::interact).pressed,
              "Mouse binding did not recover after focus loss");
        send_button(Button1, false, 20, 20);
        input.consume();
        XEvent close{};
        close.xclient.type = ClientMessage;
        close.xclient.window = native;
        close.xclient.format = 32;
        close.xclient.message_type = XInternAtom(display, "WM_PROTOCOLS", False);
        close.xclient.data.l[0] =
            static_cast<long>(XInternAtom(display, "WM_DELETE_WINDOW", False));
        XSendEvent(display, native, False, NoEventMask, &close);
        XSync(display, False);
        check(!window.poll(input), "Window close request was ignored");
        std::cout << "PASS native keys, pointer, wheel, repeat, aliases, focus loss and close\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "FAIL platform events: " << e.what() << '\n';
        return 1;
    }
}
