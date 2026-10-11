#pragma once
#include "engine/math.hpp"
#include <array>
#include <cstddef>
#include <cstdint>
#include <stdexcept>

namespace engine {
enum class Key : std::size_t {
    left,
    right,
    up,
    down,
    pause,
    restart,
    escape,
    fullscreen,
    zoom_in,
    zoom_out,
    panel_up,
    panel_down,
    mute,
    music_volume,
    effects_volume,
    settings,
    tab,
    shift,
    accept,
    space,
    interact,
    diagnostics,
    debug_shapes,
    single_step,
    reload_shaders,
    shader_error,
    reload_textures,
    texture_error,
    count
};
// Stable engine symbols, outside the X11 KeySym range. Never persist native button IDs.
enum class MouseButton : std::uint32_t { left = 0x20000001, middle, right, side1, side2 };
inline constexpr std::uint32_t mouse_symbol(MouseButton button) {
    return static_cast<std::uint32_t>(button);
}
inline constexpr bool is_mouse_symbol(std::uint32_t symbol) {
    return symbol >= mouse_symbol(MouseButton::left) && symbol <= mouse_symbol(MouseButton::side2);
}
namespace modifiers {
inline constexpr std::uint32_t shift = 1, control = 2, alt = 4, all = 7;
}
// Left/right variants contribute to the same logical modifier.
inline constexpr std::uint32_t modifier_for_symbol(std::uint32_t symbol) {
    switch (symbol) {
    case 0xffe1:
    case 0xffe2:
        return modifiers::shift;
    case 0xffe3:
    case 0xffe4:
        return modifiers::control;
    case 0xffe9:
    case 0xffea:
        return modifiers::alt;
    default:
        return 0;
    }
}
struct Button {
    bool held{}, pressed{}, released{};
};
struct PointerFrame {
    Vec2 position{}, press_position{}, release_position{};
    Button primary;
    int wheel{};
    bool inside{};
};
struct InputFrame : std::array<Button, static_cast<std::size_t>(Key::count)> {
    std::uint32_t
        pressed_symbol{}; // First fresh native key/button press this tick; zero means none.
    std::uint32_t pressed_modifiers{}; // Snapshot paired with pressed_symbol.
    PointerFrame pointer;
    bool canceled{};
};
// Logical actions (from any device) and UI pointer capture are independent.
// Suppresses captured input and held buttons until a physical release is observed.
class InputGate {
  public:
    InputFrame route(InputFrame input, bool capture_actions, bool capture_pointer) {
        for (std::size_t i = 0; i < input.size(); ++i) {
            if (!input[i].held || input[i].released)
                blocked_[i] = false;
            if (capture_actions)
                blocked_[i] = input[i].held;
            if (capture_actions || blocked_[i])
                input[i] = {};
        }
        if (capture_actions || (capture_pointer && is_mouse_symbol(input.pressed_symbol))) {
            input.pressed_symbol = 0;
            input.pressed_modifiers = 0;
        }
        if (!input.pointer.primary.held || input.pointer.primary.released)
            pointer_blocked_ = false;
        if (capture_pointer)
            pointer_blocked_ = input.pointer.primary.held;
        if (capture_pointer || pointer_blocked_) {
            input.pointer.primary = {};
            input.pointer.wheel = 0;
        }
        return input;
    }

  private:
    std::array<bool, static_cast<std::size_t>(Key::count)> blocked_{};
    bool pointer_blocked_{};
};
inline const Button& button(const InputFrame& frame, Key key) {
    return frame.at(static_cast<std::size_t>(key));
}
class Input {
  public:
    void set(Key key, bool down) {
        auto& b = state_.at(static_cast<std::size_t>(key));
        if (b.held == down)
            return; // Ignore OS key repeat.
        b.held = down;
        if (down)
            b.pressed = true;
        else
            b.released = true;
    }
    void press_symbol(std::uint32_t symbol, std::uint32_t held_modifiers = 0) {
        if (held_modifiers & ~modifiers::all)
            throw std::invalid_argument("Unsupported modifier mask");
        if (symbol && !state_.pressed_symbol) {
            state_.pressed_symbol = symbol;
            state_.pressed_modifiers = held_modifiers;
        }
    }
    void move_pointer(Vec2 position, bool inside = true) {
        if (!std::isfinite(position.x) || !std::isfinite(position.y))
            throw std::invalid_argument("Nonfinite pointer position");
        state_.pointer.position = position;
        state_.pointer.inside = inside;
    }
    void set_primary(bool down) {
        auto& p = state_.pointer;
        if (p.primary.held == down)
            return;
        p.primary.held = down;
        if (down) {
            p.primary.pressed = true;
            p.press_position = p.position;
        } else {
            p.primary.released = true;
            p.release_position = p.position;
        }
    }
    void scroll(int steps) {
        state_.pointer.wheel = static_cast<int>(std::clamp<std::int64_t>(
            static_cast<std::int64_t>(state_.pointer.wheel) + steps, -120, 120));
    }
    void leave_pointer() { state_.pointer.inside = false; }
    void release_all() {
        state_.pressed_symbol = 0;
        state_.pressed_modifiers = 0;
        for (auto& b : state_) {
            b.released = b.held;
            b.held = false;
            b.pressed = false;
        }
        state_.pointer.primary = {};
        state_.pointer.wheel = 0;
        state_.pointer.inside = false;
        state_.canceled = true;
    }
    InputFrame consume() {
        const auto result = state_;
        state_.pressed_symbol = 0;
        state_.pressed_modifiers = 0;
        for (auto& b : state_) {
            b.pressed = false;
            b.released = false;
        }
        state_.pointer.primary.pressed = state_.pointer.primary.released = false;
        state_.pointer.wheel = 0;
        state_.canceled = false;
        return result;
    }

  private:
    InputFrame state_{};
};
} // namespace engine
