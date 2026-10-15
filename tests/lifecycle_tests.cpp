#include "engine/renderer.hpp"
#include "engine/ui.hpp"
#include "native_input.hpp"
#include <GL/glx.h>
#include <X11/Xatom.h>
#include <chrono>
#include <thread>

namespace {
using namespace engine;
using namespace testing;
using namespace std::chrono_literals;
void sync(NativeInput& f) {
    XSync(f.display.get(), False);
    f.window.poll(f.input);
}
void focus(NativeInput& f, int type, int mode = NotifyNormal, int detail = NotifyNonlinear) {
    XEvent event{};
    event.xfocus.type = type;
    event.xfocus.window = f.native;
    event.xfocus.mode = mode;
    event.xfocus.detail = detail;
    f.send(event, FocusChangeMask);
}
TEST(fullscreen_observation_validates_property_and_tracks_removal) {
    NativeInput f;
    const auto state = XInternAtom(f.display.get(), "_NET_WM_STATE", False);
    const auto full = XInternAtom(f.display.get(), "_NET_WM_STATE_FULLSCREEN", False);
    const auto other = XInternAtom(f.display.get(), "_NET_WM_STATE_ABOVE", False);
    CHECK(!f.window.fullscreen());
    unsigned long atoms[] = {other, full};
    XChangeProperty(f.display.get(), f.native, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(atoms), 2);
    sync(f);
    CHECK(f.window.fullscreen());
    XChangeProperty(f.display.get(), f.native, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(atoms), 1);
    sync(f);
    CHECK(!f.window.fullscreen());
    XChangeProperty(f.display.get(), f.native, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(atoms + 1), 1);
    sync(f);
    CHECK(f.window.fullscreen());
    XDeleteProperty(f.display.get(), f.native, state);
    sync(f);
    CHECK(!f.window.fullscreen());
    XChangeProperty(f.display.get(), f.native, state, XA_STRING, 8, PropModeReplace,
                    reinterpret_cast<const unsigned char*>("bad"), 3);
    sync(f);
    CHECK(!f.window.fullscreen());
    std::array<unsigned long, 257> oversized{};
    oversized[0] = full;
    XChangeProperty(f.display.get(), f.native, state, XA_ATOM, 32, PropModeReplace,
                    reinterpret_cast<const unsigned char*>(oversized.data()), 257);
    sync(f);
    CHECK(!f.window.fullscreen());
}
TEST(fullscreen_requests_are_explicit_flushed_and_do_not_predict_wm_acceptance) {
    NativeInput f;
    XSelectInput(f.display.get(), DefaultRootWindow(f.display.get()), SubstructureNotifyMask);
    XSync(f.display.get(), False);
    f.window.set_fullscreen(true);
    f.window.set_fullscreen(false);
    f.window.toggle_fullscreen();
    const auto state = XInternAtom(f.display.get(), "_NET_WM_STATE", False);
    const auto full = XInternAtom(f.display.get(), "_NET_WM_STATE_FULLSCREEN", False);
    unsigned received = 0;
    const auto end = std::chrono::steady_clock::now() + 2s;
    while (received < 3 && std::chrono::steady_clock::now() < end) {
        while (XPending(f.display.get())) {
            XEvent e{};
            XNextEvent(f.display.get(), &e);
            if (e.type != ClientMessage || e.xclient.window != f.native ||
                e.xclient.message_type != state)
                continue;
            const std::array<long, 3> actions{1, 0, 2};
            CHECK(received < actions.size());
            CHECK(e.xclient.format == 32 && e.xclient.data.l[0] == actions[received]);
            CHECK(e.xclient.data.l[1] == static_cast<long>(full) && e.xclient.data.l[3] == 1);
            ++received;
        }
        std::this_thread::sleep_for(1ms);
    }
    CHECK(received == 3 && !f.window.fullscreen());
}
TEST(temporary_focus_grabs_preserve_input_real_focus_loss_cancels_it) {
    NativeInput f;
    focus(f, FocusIn);
    CHECK(f.window.focused());
    f.input.consume();
    f.key(XK_w, true);
    f.mouse(Button1, true);
    f.input.consume();
    for (int mode : {NotifyGrab, NotifyUngrab}) {
        focus(f, FocusOut, mode);
        auto frame = f.input.consume();
        CHECK(f.window.focused() && !frame.canceled);
        CHECK(button(frame, Key::up).held && frame.pointer.primary.held);
    }
    focus(f, FocusOut, NotifyNormal, NotifyInferior);
    CHECK(f.window.focused() && !f.input.consume().canceled);
    focus(f, FocusOut);
    const auto lost = f.input.consume();
    CHECK(!f.window.focused() && lost.canceled && !button(lost, Key::up).held);
    CHECK(!lost.pointer.primary.held && !lost.pointer.primary.released && !lost.pressed_symbol);
    focus(f, FocusIn, NotifyGrab);
    CHECK(!f.window.focused());
    focus(f, FocusIn);
    CHECK(f.window.focused());
    f.key(XK_w, false);
    f.mouse(Button1, false);
    f.input.consume();
    f.key(XK_w, true);
    f.mouse(Button1, true);
    const auto recovered = f.input.consume();
    CHECK(button(recovered, Key::up).pressed && recovered.pointer.primary.pressed);
}
TEST(real_unmap_restore_preserves_context_and_cancels_input) {
    NativeInput f;
    Renderer renderer;
    const auto texture = renderer.upload({1, 1, {255, 0, 0, 255}});
    const auto bytes = renderer.texture_bytes(), font = renderer.font_bytes();
    CHECK(!f.window.drawable());
    XMoveWindow(f.display.get(), f.native, -20000, -20000);
    sync(f);
    for (int i = 0; i < 4; ++i) {
        f.window.restore();
        XSync(glXGetCurrentDisplay(), False);
        f.window.poll(f.input);
        CHECK(f.window.drawable());
        focus(f, FocusIn);
        f.key(XK_w, true);
        f.mouse(Button1, true);
        f.input.consume();
        XUnmapWindow(f.display.get(), f.native);
        sync(f);
        const auto canceled = f.input.consume();
        CHECK(!f.window.drawable() && !f.window.focused() && canceled.canceled);
        CHECK(!button(canceled, Key::up).held && !canceled.pointer.primary.held);
        f.window.present(); // A suspended drawable is a safe no-op.
        f.window.restore();
        XSync(glXGetCurrentDisplay(), False);
        f.window.poll(f.input);
        renderer.begin(f.window.width(), f.window.height(), {{0, 0}, 2});
        renderer.sprite(texture, Transform::from({}, 0, {2, 2}));
        renderer.end();
        CHECK(renderer.pixel(32, 32)[0] == 255);
        CHECK(renderer.live_textures() == 1 && renderer.texture_bytes() == bytes);
        CHECK(renderer.font_bytes() == font && renderer.healthy());
    }
}
TEST(resize_matrix_matches_glx_pixels_camera_ui_and_pointer) {
    NativeInput f;
    Renderer renderer;
    // Keep the exact-size fixture unmapped: mapped native sizes can be overridden
    // asynchronously even with override_redirect set.
    for (const auto size :
         {RenderTargetSize{320, 240}, RenderTargetSize{640, 480}, RenderTargetSize{1280, 720},
          RenderTargetSize{1920, 1080}, RenderTargetSize{900, 1200}, RenderTargetSize{320, 200}}) {
        f.window.resize(size.width, size.height);
        glXWaitX(); // Order native resize before rendering into the hidden drawable.
        // Allow bounded convergence of the X11 event and GLX drawable sizes.
        unsigned gl_width{}, gl_height{};
        const auto deadline = std::chrono::steady_clock::now() + 2s;
        do {
            f.window.poll(f.input);
            renderer.begin(f.window.width(), f.window.height(), {{0, 0}, 2});
            renderer.end();
            // Window::present intentionally skips unmapped windows. Retire the
            // hidden test buffer explicitly so GLX allocates the resized back buffer.
            glXSwapBuffers(glXGetCurrentDisplay(), glXGetCurrentDrawable());
            glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_WIDTH, &gl_width);
            glXQueryDrawable(glXGetCurrentDisplay(), glXGetCurrentDrawable(), GLX_HEIGHT,
                             &gl_height);
            if (f.window.width() == size.width && f.window.height() == size.height &&
                gl_width == static_cast<unsigned>(size.width) &&
                gl_height == static_cast<unsigned>(size.height))
                break;
            std::this_thread::sleep_for(2ms);
        } while (std::chrono::steady_clock::now() < deadline);
        XWindowAttributes actual{};
        XGetWindowAttributes(f.display.get(), f.native, &actual);
        std::cout << "[resize] requested=" << size.width << 'x' << size.height
                  << " observed=" << f.window.width() << 'x' << f.window.height()
                  << " GLX=" << gl_width << 'x' << gl_height << " X11=" << actual.width << 'x'
                  << actual.height << " override=" << actual.override_redirect << '\n';
        CHECK(f.window.width() == size.width && f.window.height() == size.height);
        CHECK(gl_width == static_cast<unsigned>(size.width) &&
              gl_height == static_cast<unsigned>(size.height));
        const float w = static_cast<float>(size.width), h = static_cast<float>(size.height);
        Camera camera{{12, -9}, 20};
        const Vec2 pointer{w * 0.5F, h * 0.5F};
        const auto world = camera.screen_to_world(pointer, size.width, size.height);
        const auto screen = camera.world_to_screen(world, size.width, size.height);
        NEAR(screen.x, pointer.x);
        NEAR(screen.y, pointer.y);
        Ui ui;
        const UiWidget widget{
            1, UiKind::button, {{w * .25F, h * .25F}, {w * .75F, h * .75F}}, "RESIZE"};
        ui.layout({{0, 0}, {w, h}}, std::span(&widget, 1));
        renderer.begin(size.width, size.height, {{w * .5F, -h * .5F}, h}, {0, 0, 0, 1});
        draw_ui(renderer, ui);
        renderer.end();
        CHECK(renderer.pixel(size.width / 2, size.height / 2)[2] > 40);
        XSync(glXGetCurrentDisplay(), False);
        f.window.poll(f.input);
        f.input.consume(); // Discard startup/focus transitions before the controlled click.
        for (int type : {ButtonPress, ButtonRelease}) {
            XEvent event{};
            event.xbutton.type = type;
            event.xbutton.window = f.native;
            event.xbutton.button = Button1;
            event.xbutton.x = size.width / 2;
            event.xbutton.y = size.height / 2;
            f.send(event, type == ButtonPress ? ButtonPressMask : ButtonReleaseMask);
        }
        const auto frame = f.input.consume();
        const auto result = ui.update(frame);
        CHECK(result.count == 1 && result.events()[0].id == 1);
        CHECK(renderer.healthy());
    }
    rejects([&] { f.window.resize(0, 480); });
    rejects([&] { f.window.resize(20000, 480); });
    CHECK(f.window.width() == 320 && f.window.height() == 200);
}
TEST(close_requires_correct_window_protocol_and_format) {
    NativeInput f;
    XEvent e{};
    e.xclient.type = ClientMessage;
    e.xclient.window = f.native;
    e.xclient.message_type = XInternAtom(f.display.get(), "WM_PROTOCOLS", False);
    e.xclient.data.l[0] =
        static_cast<long>(XInternAtom(f.display.get(), "WM_DELETE_WINDOW", False));
    e.xclient.format = 8;
    f.send(e, 0);
    CHECK(f.window.poll(f.input));
    e.xclient.format = 32;
    e.xclient.window = DefaultRootWindow(f.display.get());
    f.send(e, 0);
    CHECK(f.window.poll(f.input));
    e.xclient.window = f.native;
    f.send(e, 0);
    CHECK(!f.window.poll(f.input) && !f.window.drawable());
}
} // namespace
int main() {
    if (!std::getenv("DISPLAY"))
        return 77;
    return testing::run_tests();
}
