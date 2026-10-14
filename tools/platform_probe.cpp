#include "engine/renderer.hpp"
#include "engine/window.hpp"
#include <GL/glx.h>
#include <X11/Xlib.h>
#include <chrono>
#include <cstdlib>
#include <iostream>
#include <memory>
#include <string>
#include <thread>
#include <unistd.h>

namespace {
using namespace std::chrono_literals;
::Window find_window(Display* display, ::Window parent, const std::string& name, unsigned& budget,
                     unsigned depth = 0) {
    if (!budget || depth > 8)
        return 0;
    --budget;
    char* title{};
    bool matches = false;
    if (XFetchName(display, parent, &title) && title) {
        matches = name == title;
        XFree(title);
    }
    if (matches)
        return parent;
    ::Window root{}, owner{}, *children{};
    unsigned count{};
    if (!XQueryTree(display, parent, &root, &owner, &children, &count))
        return 0;
    ::Window result{};
    for (unsigned i = 0; i < count && !result && budget; ++i)
        result = find_window(display, children[i], name, budget, depth + 1);
    if (children)
        XFree(children);
    return result;
}
void require(bool value, const char* message) {
    if (!value)
        throw std::runtime_error(message);
}
struct Companion {
    Display* display;
    ::Window id;
    ~Companion() {
        XDestroyWindow(display, id);
        XSync(display, False);
    }
};
} // namespace
int main(int argc, char** argv) {
    if (argc == 1 || (argc == 2 && std::string_view(argv[1]) == "--help")) {
        std::cout
            << "platform_probe --exercise [--screenshot FILE.ppm]\n"
               "Opens its own window, exercises fullscreen, focus, minimize/restore and resize.\n"
               "No desktop settings are changed. Exit 2 means a WM request was unconfirmed;\n"
               "exit 1 means an engine check failed; 77 means DISPLAY is unavailable.\n";
        return 0;
    }
    if (!std::getenv("DISPLAY"))
        return 77;
    try {
        std::string screenshot;
        require(std::string_view(argv[1]) == "--exercise", "Expected --exercise");
        if (argc == 4 && std::string_view(argv[2]) == "--screenshot")
            screenshot = argv[3];
        else
            require(argc == 2, "Invalid probe arguments");
        const std::string name = "Engine platform probe " + std::to_string(getpid());
        engine::Window window(960, 600, name);
        engine::Renderer renderer;
        engine::Input input;
        std::unique_ptr<Display, decltype(&XCloseDisplay)> connection(XOpenDisplay(nullptr),
                                                                      &XCloseDisplay);
        require(connection != nullptr, "Cannot open probe connection");
        auto* display = connection.get();
        std::string phase = "STARTUP";
        const auto draw = [&](bool capture = false) {
            require(window.poll(input), "Probe closed before completion");
            if (window.drawable()) {
                const float w = static_cast<float>(window.width()),
                            h = static_cast<float>(window.height());
                renderer.begin(window.width(), window.height(), {{w * .5F, -h * .5F}, h});
                renderer.text({20, -20}, 3, "PLATFORM LIFECYCLE", {0.1F, 0.9F, 0.7F, 1});
                renderer.text({20, -65}, 2, phase, {});
                renderer.text({20, -100}, 2,
                              "PIXELS " + std::to_string(window.width()) + " X " +
                                  std::to_string(window.height()),
                              {});
                renderer.text({20, -130}, 2,
                              "FULLSCREEN " + std::to_string(window.fullscreen()) + "  FOCUS " +
                                  std::to_string(window.focused()),
                              {});
                renderer.quad({w * .5F, -h * .65F}, {w * .5F, h * .3F}, {0.05F, 0.6F, 0.3F, 1});
                renderer.end();
                require(renderer.healthy(), "GL error during lifecycle exercise");
                if (capture)
                    renderer.screenshot(screenshot);
                window.present();
            }
        };
        const auto wait = [&](auto condition, std::chrono::milliseconds duration = 2000ms) {
            const auto end = std::chrono::steady_clock::now() + duration;
            do {
                draw();
                if (condition())
                    return true;
                std::this_thread::sleep_for(5ms);
            } while (std::chrono::steady_clock::now() < end);
            return false;
        };
        require(wait([&] { return window.drawable(); }), "Visible window did not map");
        const bool vsync_off = window.set_vsync(false), vsync_on = window.set_vsync(true);
        window.set_vsync(false);
        std::cout << "[probe] vsync_request_off=" << vsync_off << " on=" << vsync_on << '\n';
        unsigned budget = 4096;
        const auto native = find_window(display, DefaultRootWindow(display), name, budget);
        require(native != 0, "Probe window not found");
        const auto initial_bytes = renderer.font_bytes();
        unsigned unconfirmed = 0;
        const auto report = [&](const char* operation, bool accepted) {
            std::cout << "[probe] " << operation << '=' << (accepted ? "confirmed" : "unconfirmed")
                      << " size=" << window.width() << 'x' << window.height() << std::endl;
            if (!accepted)
                ++unconfirmed;
        };
        phase = "ENTER FULLSCREEN";
        window.set_fullscreen(true);
        report("fullscreen_enter", wait([&] { return window.fullscreen(); }));
        phase = "LEAVE FULLSCREEN";
        window.set_fullscreen(false);
        report("fullscreen_leave", wait([&] { return !window.fullscreen(); }));
        // Real server focus transfers are confined to two windows owned by this probe.
        XSetWindowAttributes attributes{};
        attributes.override_redirect = True;
        Companion other{display, XCreateWindow(display, DefaultRootWindow(display), -20000, -20000,
                                               16, 16, 0, CopyFromParent, InputOutput,
                                               CopyFromParent, CWOverrideRedirect, &attributes)};
        XMapWindow(display, other.id);
        XSync(display, False);
        phase = "FOCUS LOSS AND RETURN";
        XSetInputFocus(display, native, RevertToParent, CurrentTime);
        XSync(display, False);
        require(wait([&] { return window.focused(); }), "Cannot focus probe");
        input.set(engine::Key::right, true);
        input.set_primary(true);
        input.consume();
        XSetInputFocus(display, other.id, RevertToParent, CurrentTime);
        XSync(display, False);
        require(wait([&] { return !window.focused(); }), "Focus loss not observed");
        const auto canceled = input.consume();
        require(canceled.canceled && !engine::button(canceled, engine::Key::right).held &&
                    !canceled.pointer.primary.held && !canceled.pointer.primary.released,
                "Visible focus loss left held input or synthesized a click");
        XSetInputFocus(display, native, RevertToParent, CurrentTime);
        XSync(display, False);
        require(wait([&] { return window.focused(); }), "Focus return not observed");
        report("visible_focus", true);
        phase = "MINIMIZE REQUEST";
        const bool submitted = window.minimize();
        const bool minimized = submitted && wait([&] { return !window.drawable(); });
        report("wm_minimize", minimized);
        if (minimized) {
            std::this_thread::sleep_for(150ms);
            window.restore();
            require(wait([&] { return window.drawable(); }), "WM minimize restore failed");
            report("wm_restore", true);
        }
        phase = "CLIENT UNMAP AND RESTORE";
        XUnmapWindow(display, native);
        XSync(display, False);
        require(wait([&] { return !window.drawable(); }), "Client unmap not observed");
        require(!window.focused(), "Unmapped window remained focused");
        window.restore();
        require(wait([&] { return window.drawable(); }), "Client restore failed");
        report("client_unmap_restore", true);
        phase = "RESIZE REQUEST 640 X 480";
        window.resize(640, 480);
        // A tiling WM may choose a different size. Record observed pixels, not requested size.
        wait([] { return false; }, 250ms);
        unsigned width{}, height{};
        glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_WIDTH, &width);
        glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_HEIGHT, &height);
        require(width == static_cast<unsigned>(window.width()) &&
                    height == static_cast<unsigned>(window.height()),
                "X11 dimensions disagree with GLX drawable pixels");
        report("drawable_pixel_dimensions", true);
        require(renderer.live_targets() == 0 && renderer.live_textures() == 0 &&
                    renderer.font_bytes() == initial_bytes,
                "Lifecycle exercise grew renderer resources");
        phase = "COMPLETE / UNCONFIRMED WM REQUESTS " + std::to_string(unconfirmed);
        draw(!screenshot.empty());
        std::cout << "[probe] complete=1 unconfirmed=" << unconfirmed
                  << " scaling=viewport-pixels-only\n";
        return unconfirmed ? 2 : 0;
    } catch (const std::exception& e) {
        std::cerr << "[probe] failed: " << e.what() << '\n';
        return 1;
    }
}
