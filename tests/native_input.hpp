#pragma once
#include "engine/window.hpp"
#include "test.hpp"
#include <X11/XKBlib.h>
#include <X11/keysym.h>
#include <memory>
#include <unistd.h>

namespace testing {
using namespace engine;
struct NativeInput {
    std::string name = "engine-input-" + std::to_string(getpid());
    engine::Window window{64, 64, name, false};
    std::unique_ptr<Display, decltype(&XCloseDisplay)> display{XOpenDisplay(nullptr),
                                                               &XCloseDisplay};
    ::Window native{};
    Input input;
    NativeInput() {
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
    void key(KeySym symbol, bool down, unsigned int state = 0) {
        XEvent e{};
        e.xkey.type = down ? KeyPress : KeyRelease;
        e.xkey.display = display.get();
        e.xkey.window = native;
        e.xkey.keycode = XKeysymToKeycode(display.get(), symbol);
        CHECK(e.xkey.keycode != 0);
        e.xkey.same_screen = True;
        e.xkey.state = state;
        send(e, down ? KeyPressMask : KeyReleaseMask);
    }
    unsigned int num_lock_mask() const {
        std::unique_ptr<XModifierKeymap, decltype(&XFreeModifiermap)> mapping(
            XGetModifierMapping(display.get()), &XFreeModifiermap);
        CHECK(mapping != nullptr);
        const auto code = XKeysymToKeycode(display.get(), XK_Num_Lock);
        CHECK(code != 0);
        unsigned int mask = 0;
        for (int modifier = 0; modifier < 8; ++modifier)
            for (int slot = 0; slot < mapping->max_keypermod; ++slot)
                if (mapping->modifiermap[modifier * mapping->max_keypermod + slot] == code)
                    mask |= 1U << modifier;
        CHECK(mask != 0);
        return mask;
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
} // namespace testing
