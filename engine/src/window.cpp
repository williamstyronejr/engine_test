#include "engine/window.hpp"
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glxext.h>
#include <X11/XKBlib.h>
#include <X11/Xutil.h>
#include <X11/keysym.h>
#include <array>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <string>

namespace engine {
namespace {
thread_local int context_error = 0;
int capture_error(Display*, XErrorEvent* event) {
    context_error = event->error_code;
    return 0;
}
bool extension(std::string_view list, std::string_view name) {
    std::size_t position = 0;
    while ((position = list.find(name, position)) != std::string_view::npos) {
        if ((position == 0 || list[position - 1] == ' ') &&
            (position + name.size() == list.size() || list[position + name.size()] == ' '))
            return true;
        position += name.size();
    }
    return false;
}
std::optional<Key> key_for(KeySym key) {
    switch (key) {
    case XK_a:
    case XK_A:
    case XK_Left:
        return Key::left;
    case XK_d:
    case XK_D:
    case XK_Right:
        return Key::right;
    case XK_w:
    case XK_W:
    case XK_Up:
        return Key::up;
    case XK_s:
    case XK_S:
    case XK_Down:
        return Key::down;
    case XK_space:
        return Key::space;
    case XK_e:
    case XK_E:
        return Key::interact;
    case XK_F1:
        return Key::settings;
    case XK_F2:
        return Key::diagnostics;
    case XK_F3:
        return Key::debug_shapes;
    case XK_F5:
        return Key::reload_shaders;
    case XK_F6:
        return Key::shader_error;
    case XK_F7:
        return Key::reload_textures;
    case XK_F8:
        return Key::texture_error;
    case XK_F10:
        return Key::single_step;
    case XK_Tab:
    case XK_ISO_Left_Tab:
        return Key::tab;
    case XK_Shift_L:
    case XK_Shift_R:
        return Key::shift;
    case XK_Return:
    case XK_KP_Enter:
        return Key::accept;
    case XK_p:
    case XK_P:
        return Key::pause;
    case XK_r:
    case XK_R:
        return Key::restart;
    case XK_Escape:
        return Key::escape;
    case XK_equal:
    case XK_plus:
        return Key::zoom_in;
    case XK_minus:
    case XK_underscore:
        return Key::zoom_out;
    case XK_Page_Up:
        return Key::panel_up;
    case XK_Page_Down:
        return Key::panel_down;
    case XK_m:
    case XK_M:
        return Key::mute;
    case XK_n:
    case XK_N:
        return Key::music_volume;
    case XK_b:
    case XK_B:
        return Key::effects_volume;
    case XK_F11:
        return Key::fullscreen;
    default:
        return std::nullopt;
    }
}
} // namespace
struct Window::Impl {
    Display* display{};
    ::Window window{};
    Colormap colormap{};
    GLXContext context{};
    Atom close_atom{};
    int width{}, height{};
    bool mapped{}, closing{};
    std::array<bool, 256> physical{};
    std::array<std::optional<Key>, 256> bindings{};
    ~Impl() {
        if (!display)
            return;
        if (context) {
            glXMakeCurrent(display, 0, nullptr);
            glXDestroyContext(display, context);
        }
        if (window)
            XDestroyWindow(display, window);
        if (colormap)
            XFreeColormap(display, colormap);
        XCloseDisplay(display);
    }
};
Window::Window(int width, int height, std::string_view title_text, bool visible)
    : impl_(std::make_unique<Impl>()) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Window dimensions must be in 1..16384");
    auto& p = *impl_;
    p.display = XOpenDisplay(nullptr);
    if (!p.display)
        throw std::runtime_error(
            "Cannot open X11 display. Set DISPLAY and run under X11 or XWayland.");
    const int screen = DefaultScreen(p.display);
    const int attributes[] = {GLX_X_RENDERABLE,
                              True,
                              GLX_DRAWABLE_TYPE,
                              GLX_WINDOW_BIT,
                              GLX_RENDER_TYPE,
                              GLX_RGBA_BIT,
                              GLX_X_VISUAL_TYPE,
                              GLX_TRUE_COLOR,
                              GLX_RED_SIZE,
                              8,
                              GLX_GREEN_SIZE,
                              8,
                              GLX_BLUE_SIZE,
                              8,
                              GLX_ALPHA_SIZE,
                              8,
                              GLX_DOUBLEBUFFER,
                              True,
                              GLX_FRAMEBUFFER_SRGB_CAPABLE_ARB,
                              True,
                              None};
    int count = 0;
    GLXFBConfig* configs = glXChooseFBConfig(p.display, screen, attributes, &count);
    if (!configs || count == 0) {
        if (configs)
            XFree(configs);
        throw std::runtime_error("No sRGB GLX framebuffer configuration");
    }
    const GLXFBConfig config = configs[0];
    XFree(configs);
    XVisualInfo* visual = glXGetVisualFromFBConfig(p.display, config);
    if (!visual)
        throw std::runtime_error("GLX configuration has no X11 visual");
    p.colormap =
        XCreateColormap(p.display, RootWindow(p.display, screen), visual->visual, AllocNone);
    XSetWindowAttributes settings{};
    settings.colormap = p.colormap;
    // Hidden test drawables bypass asynchronous window-manager sizing decisions.
    // Visible application windows remain managed normally.
    settings.override_redirect = visible ? False : True;
    settings.event_mask = StructureNotifyMask | KeyPressMask | KeyReleaseMask | FocusChangeMask |
                          PointerMotionMask | ButtonPressMask | ButtonReleaseMask |
                          EnterWindowMask | LeaveWindowMask | ExposureMask;
    p.window = XCreateWindow(p.display, RootWindow(p.display, screen), 0, 0,
                             static_cast<unsigned int>(width), static_cast<unsigned int>(height), 0,
                             visual->depth, InputOutput, visual->visual,
                             CWColormap | CWEventMask | CWOverrideRedirect, &settings);
    XFree(visual);
    if (!p.window)
        throw std::runtime_error("XCreateWindow failed");
    p.width = width;
    p.height = height;
    p.close_atom = XInternAtom(p.display, "WM_DELETE_WINDOW", False);
    XSetWMProtocols(p.display, p.window, &p.close_atom, 1);
    title(title_text);
    Bool repeat_supported{};
    XkbSetDetectableAutoRepeat(p.display, True, &repeat_supported);
    for (std::size_t i = 8; i < p.bindings.size(); ++i)
        p.bindings[i] = key_for(XkbKeycodeToKeysym(p.display, static_cast<KeyCode>(i), 0, 0));
    auto create = reinterpret_cast<PFNGLXCREATECONTEXTATTRIBSARBPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXCreateContextAttribsARB")));
    const char* extensions = glXQueryExtensionsString(p.display, screen);
    if (!create || !extensions || !extension(extensions, "GLX_ARB_create_context_profile"))
        throw std::runtime_error("GLX core context creation is unavailable");
    const int context_attributes[] = {GLX_CONTEXT_MAJOR_VERSION_ARB,
                                      4,
                                      GLX_CONTEXT_MINOR_VERSION_ARB,
                                      6,
                                      GLX_CONTEXT_PROFILE_MASK_ARB,
                                      GLX_CONTEXT_CORE_PROFILE_BIT_ARB,
#ifndef NDEBUG
                                      GLX_CONTEXT_FLAGS_ARB,
                                      GLX_CONTEXT_DEBUG_BIT_ARB,
#endif
                                      None};
    XSync(p.display, False);
    context_error = 0;
    auto previous_handler = XSetErrorHandler(capture_error);
    p.context = create(p.display, config, nullptr, True, context_attributes);
    XSync(p.display, False);
    XSetErrorHandler(previous_handler);
    if (!p.context || context_error)
        throw std::runtime_error("Cannot create OpenGL 4.6 Core context (X error " +
                                 std::to_string(context_error) + ")");
    if (!glXMakeCurrent(p.display, p.window, p.context))
        throw std::runtime_error("Cannot make OpenGL context current");
    GLint major{}, minor{}, profile{};
    glGetIntegerv(GL_MAJOR_VERSION, &major);
    glGetIntegerv(GL_MINOR_VERSION, &minor);
    glGetIntegerv(GL_CONTEXT_PROFILE_MASK, &profile);
    if (major < 4 || (major == 4 && minor < 6) || !(profile & GL_CONTEXT_CORE_PROFILE_BIT))
        throw std::runtime_error("OpenGL 4.6 Core is required; returned context is unsuitable");
    std::cout << "[platform] X11/GLX (may be XWayland)\n[graphics] " << glGetString(GL_VENDOR)
              << " / " << glGetString(GL_RENDERER) << "\n[OpenGL] " << glGetString(GL_VERSION)
              << '\n';
    if (visible)
        XMapWindow(p.display, p.window);
    XFlush(p.display);
}
Window::~Window() = default;
bool Window::poll(Input& input) {
    auto& p = *impl_;
    while (XPending(p.display)) {
        XEvent e{};
        XNextEvent(p.display, &e);
        switch (e.type) {
        case ClientMessage:
            if (e.xclient.message_type == XInternAtom(p.display, "WM_PROTOCOLS", False) &&
                static_cast<Atom>(e.xclient.data.l[0]) == p.close_atom)
                p.closing = true;
            break;
        case ConfigureNotify:
            p.width = e.xconfigure.width;
            p.height = e.xconfigure.height;
            break;
        case MapNotify:
            p.mapped = true;
            break;
        case UnmapNotify:
            p.mapped = false;
            input.release_all();
            p.physical.fill(false);
            break;
        case FocusOut:
            input.release_all();
            p.physical.fill(false);
            break;
        case MotionNotify:
            input.move_pointer({static_cast<float>(e.xmotion.x), static_cast<float>(e.xmotion.y)},
                               e.xmotion.x >= 0 && e.xmotion.y >= 0 && e.xmotion.x < p.width &&
                                   e.xmotion.y < p.height);
            break;
        case EnterNotify:
            input.move_pointer(
                {static_cast<float>(e.xcrossing.x), static_cast<float>(e.xcrossing.y)});
            break;
        case LeaveNotify:
            input.leave_pointer();
            break;
        case ButtonPress:
        case ButtonRelease:
            input.move_pointer({static_cast<float>(e.xbutton.x), static_cast<float>(e.xbutton.y)},
                               e.xbutton.x >= 0 && e.xbutton.y >= 0 && e.xbutton.x < p.width &&
                                   e.xbutton.y < p.height);
            if (e.xbutton.button == Button1)
                input.set_primary(e.type == ButtonPress);
            if (e.type == ButtonPress &&
                (e.xbutton.button == Button4 || e.xbutton.button == Button5))
                input.scroll(e.xbutton.button == Button4 ? 1 : -1);
            break;
        case KeyRelease:
            if (XPending(p.display)) { // Legacy autorepeat fallback.
                XEvent next{};
                XPeekEvent(p.display, &next);
                if (next.type == KeyPress && next.xkey.time == e.xkey.time &&
                    next.xkey.keycode == e.xkey.keycode)
                    break;
            }
            [[fallthrough]];
        case KeyPress: {
            const auto code = static_cast<std::size_t>(e.xkey.keycode);
            if (code >= p.physical.size())
                break;
            p.physical[code] = e.type == KeyPress;
            if (const auto key = p.bindings[code]) {
                bool down = false;
                for (std::size_t i = 0; i < p.physical.size(); ++i)
                    if (p.bindings[i] == key && p.physical[i])
                        down = true;
                input.set(*key, down);
            }
            break;
        }
        default:
            break;
        }
    }
    return !p.closing;
}
void Window::present() {
    glXSwapBuffers(impl_->display, impl_->window);
}
bool Window::set_vsync(bool enabled) {
    const char* ext = glXQueryExtensionsString(impl_->display, DefaultScreen(impl_->display));
    if (!ext || !extension(ext, "GLX_EXT_swap_control"))
        return false;
    auto swap_interval = reinterpret_cast<PFNGLXSWAPINTERVALEXTPROC>(
        glXGetProcAddressARB(reinterpret_cast<const GLubyte*>("glXSwapIntervalEXT")));
    if (!swap_interval)
        return false;
    swap_interval(impl_->display, impl_->window, enabled ? 1 : 0);
    return true;
}
void Window::toggle_fullscreen() {
    XEvent e{};
    e.xclient.type = ClientMessage;
    e.xclient.window = impl_->window;
    e.xclient.message_type = XInternAtom(impl_->display, "_NET_WM_STATE", False);
    e.xclient.format = 32;
    e.xclient.data.l[0] = 2; // EWMH toggle.
    e.xclient.data.l[1] =
        static_cast<long>(XInternAtom(impl_->display, "_NET_WM_STATE_FULLSCREEN", False));
    e.xclient.data.l[3] = 1;
    XSendEvent(impl_->display, DefaultRootWindow(impl_->display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &e);
}
void Window::resize(int width, int height) {
    if (width <= 0 || height <= 0 || width > 16384 || height > 16384)
        throw std::invalid_argument("Invalid window size");
    XResizeWindow(impl_->display, impl_->window, static_cast<unsigned int>(width),
                  static_cast<unsigned int>(height));
    XSync(impl_->display, False);
}
int Window::width() const {
    return impl_->width;
}
int Window::height() const {
    return impl_->height;
}
bool Window::drawable() const {
    return impl_->mapped && impl_->width > 0 && impl_->height > 0;
}
void Window::title(std::string_view value) {
    const std::string text(value);
    XStoreName(impl_->display, impl_->window, text.c_str());
}
} // namespace engine
