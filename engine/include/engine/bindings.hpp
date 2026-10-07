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
    std::array<std::uint32_t, 10> letters{'a', 'd', 'w', 's', 'p', 'r', 'e', 'm', 'n', 'b'};
    bool operator==(const KeyBindings&) const = default;
    void validate() const {
        std::array<bool, 26> used{};
        for (const auto symbol : letters) {
            if (symbol < 'a' || symbol > 'z')
                throw std::invalid_argument("CHOOSE A LETTER A-Z");
            if (used[symbol - 'a'])
                throw std::invalid_argument("KEY ALREADY ASSIGNED");
            used[symbol - 'a'] = true;
        }
    }
    void assign(std::size_t action, std::uint32_t symbol) {
        auto candidate = *this;
        candidate.letters.at(action) = symbol;
        candidate.validate();
        *this = candidate;
    }
    std::optional<Key> action(std::uint32_t symbol) const {
        for (std::size_t i = 0; i < letters.size(); ++i)
            if (letters[i] == symbol)
                return bindable_actions[i];
        return std::nullopt;
    }
    std::string key_name(Key key) const {
        for (std::size_t i = 0; i < bindable_actions.size(); ++i)
            if (bindable_actions[i] == key)
                return std::string(1, static_cast<char>(letters[i] - 'a' + 'A'));
        throw std::invalid_argument("Action has no configurable letter binding");
    }
    std::string label(std::size_t index) const {
        return std::string(binding_names.at(index)) + " : " +
               static_cast<char>(letters.at(index) - 'a' + 'A');
    }
};
} // namespace engine
