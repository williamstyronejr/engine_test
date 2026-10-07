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
        auto send_key = [&](KeySym key, bool down) {
            XEvent event{};
            event.xkey.type = down ? KeyPress : KeyRelease;
            event.xkey.display = display;
            event.xkey.window = native;
            event.xkey.keycode = XKeysymToKeycode(display, key);
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
        invalid.letters[0] = 'q';
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
