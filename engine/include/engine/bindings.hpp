#pragma once
#include "engine/input.hpp"
#include <array>
#include <optional>
#include <string>
#include <string_view>

namespace engine {
// Stable persisted order. Letter symbols use lowercase ASCII, independent of X11 keycodes.
inline constexpr std::array bindable_actions{
    Key::left,    Key::right,    Key::up,   Key::down,         Key::pause,
    Key::restart, Key::interact, Key::mute, Key::music_volume, Key::effects_volume};
inline constexpr std::array<std::string_view, 10> binding_names{
    "MOVE LEFT", "MOVE RIGHT", "MOVE UP", "MOVE DOWN",    "PAUSE",
    "RESTART",   "INTERACT",   "MUTE",    "MUSIC VOLUME", "EFFECTS VOLUME"};
struct KeyBindings {
    std::array<std::uint32_t, 10> symbols{'a', 'd', 'w', 's', 'p', 'r', 'e', 'm', 'n', 'b'};
    bool operator==(const KeyBindings&) const = default;
    void validate() const {
        for (std::size_t i = 0; i < symbols.size(); ++i) {
            const auto symbol = symbols[i];
            if ((symbol < 'a' || symbol > 'z') && !is_mouse_symbol(symbol))
                throw std::invalid_argument("CHOOSE A-Z OR A MOUSE BUTTON");
            for (std::size_t j = 0; j < i; ++j)
                if (symbols[j] == symbol)
                    throw std::invalid_argument("INPUT ALREADY ASSIGNED");
        }
    }
    void assign(std::size_t action, std::uint32_t symbol) {
        auto candidate = *this;
        candidate.symbols.at(action) = symbol;
        candidate.validate();
        *this = candidate;
    }
    std::optional<Key> action(std::uint32_t symbol) const {
        for (std::size_t i = 0; i < symbols.size(); ++i)
            if (symbols[i] == symbol)
                return bindable_actions[i];
        return std::nullopt;
    }
    static std::string symbol_name(std::uint32_t symbol) {
        if (is_mouse_symbol(symbol)) {
            constexpr std::array names{"LMB", "MMB", "RMB", "SIDE1", "SIDE2"};
            return names[symbol - mouse_symbol(MouseButton::left)];
        }
        if (symbol < 'a' || symbol > 'z')
            throw std::invalid_argument("Invalid binding symbol");
        return std::string(1, static_cast<char>(symbol - 'a' + 'A'));
    }
    std::string key_name(Key key) const {
        for (std::size_t i = 0; i < bindable_actions.size(); ++i)
            if (bindable_actions[i] == key)
                return symbol_name(symbols[i]);
        throw std::invalid_argument("Action has no configurable binding");
    }
    std::string label(std::size_t index) const {
        return std::string(binding_names.at(index)) + " : " + symbol_name(symbols.at(index));
    }
};
} // namespace engine
