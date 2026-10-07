#pragma once
#include "engine/ui.hpp"
#include "game.hpp"

namespace feature_lab {
struct AudioSettings {
    float master{1}, music{0.3F}, effects{0.5F};
    bool muted{};
    bool operator==(const AudioSettings&) const = default;
};
struct SettingsStep {
    engine::InputFrame gameplay{};
    bool captured{}, restart{}, quit{}, fullscreen{}, audio_changed{}, vsync_changed{}, save{},
        load{};
};
class Settings {
  public:
    enum Id : engine::UiId {
        resume = 1,
        master,
        music,
        effects,
        mute,
        sync,
        fullscreen,
        restart,
        quit,
        slot,
        save,
        load
    };
    AudioSettings audio;
    unsigned selected_slot{1};
    std::string notice;
    bool gameplay_paused(const Game& game) const { return open_ ? previous_pause_ : game.paused; }
    void loaded(Game& game) {
        previous_pause_ = game.paused;
        if (open_)
            game.paused = true;
    }
    bool vsync{true}, vsync_available{true};
    bool opened() const { return open_; }
    const engine::Ui& ui() const { return ui_; }
    // Owns modal pause state; closing restores the pause state from before opening.
    SettingsStep update(Game& game, const engine::InputFrame& input, int width, int height,
                        bool audio_available) {
        SettingsStep result;
        const bool was_open = open_;
        if (engine::button(input, engine::Key::settings).pressed) {
            if (open_)
                close(game);
            else {
                open_ = true;
                previous_pause_ = game.paused;
                game.paused = true;
                rebuild(width, height, audio_available);
                ui_.reset_interaction();
            }
        } else if (open_ && engine::button(input, engine::Key::escape).pressed)
            close(game);
        else if (open_) {
            if (width != width_ || height != height_ || audio_available != audio_available_)
                rebuild(width, height, audio_available);
            const auto actions = ui_.update(input);
            for (const auto action : actions.events()) {
                switch (action.id) {
                case slot:
                    selected_slot = selected_slot % 3 + 1;
                    notice.clear();
                    rebuild(width, height, audio_available);
                    break;
                case save:
                    result.save = true;
                    break;
                case load:
                    result.load = true;
                    break;
                case resume:
                    close(game);
                    break;
                case restart:
                    close(game);
                    result.restart = true;
                    break;
                case quit:
                    result.quit = true;
                    break;
                case fullscreen:
                    result.fullscreen = true;
                    break;
                case master:
                    audio.master = action.value;
                    result.audio_changed = true;
                    break;
                case music:
                    audio.music = action.value;
                    result.audio_changed = true;
                    break;
                case effects:
                    audio.effects = action.value;
                    result.audio_changed = true;
                    break;
                case mute:
                    audio.muted = action.value != 0;
                    result.audio_changed = true;
                    break;
                case sync:
                    vsync = action.value != 0;
                    result.vsync_changed = true;
                    break;
                default:
                    break;
                }
            }
        }
        result.captured = was_open || open_;
        result.gameplay = gate_.route(input, result.captured, result.captured);
        if (result.restart)
            result.gameplay[static_cast<std::size_t>(engine::Key::restart)].pressed = true;
        return result;
    }
    void reject_vsync(bool previous) {
        vsync = previous;
        vsync_available = false;
        rebuild(width_, height_, audio_available_);
    }
    void draw(engine::Renderer& renderer, int width, int height) const;

  private:
    void close(Game& game) {
        open_ = false;
        game.paused = previous_pause_;
    }
    void rebuild(int width, int height, bool audio_available) {
        using namespace engine;
        width_ = width;
        height_ = height;
        audio_available_ = audio_available;
        const float w = static_cast<float>(std::max(width, 1));
        const float h = static_cast<float>(std::max(height, 1));
        const float scale = std::min({w / 560, h / 770, 1.0F});
        const float left = (w - 480 * scale) * 0.5F, top = (h - 722 * scale) * 0.5F;
        panel_ = {{left, top}, {left + 480 * scale, top + 722 * scale}};
        scale_ = scale;
        UiColumn column(
            {{left + 20 * scale, top + 72 * scale}, {left + 460 * scale, top + 672 * scale}},
            8 * scale);
        const auto resume_bounds = column.next(42 * scale);
        // Save controls follow the existing controls in both layout and focus order.
        const Rect slot_bounds{{left + 20 * scale, top + 572 * scale},
                               {left + 460 * scale, top + 614 * scale}};
        const Rect actions_bounds{{left + 20 * scale, top + 622 * scale},
                                  {left + 460 * scale, top + 664 * scale}};
        const float middle = (actions_bounds.min.x + actions_bounds.max.x) * 0.5F;
        const Rect save_bounds{actions_bounds.min, {middle - 4 * scale, actions_bounds.max.y}};
        const Rect load_bounds{{middle + 4 * scale, actions_bounds.min.y}, actions_bounds.max};
        const std::array widgets{
            UiWidget{resume, UiKind::button, resume_bounds, "RESUME"},

            UiWidget{master, UiKind::slider, column.next(56 * scale), "MASTER VOLUME", audio.master,
                     audio_available},
            UiWidget{music, UiKind::slider, column.next(56 * scale), "MUSIC VOLUME", audio.music,
                     audio_available},
            UiWidget{effects, UiKind::slider, column.next(56 * scale), "EFFECTS VOLUME",
                     audio.effects, audio_available},
            UiWidget{mute, UiKind::checkbox, column.next(42 * scale), "MUTE",
                     audio.muted ? 1.0F : 0.0F, audio_available},
            UiWidget{sync, UiKind::checkbox, column.next(42 * scale), "VSYNC", vsync ? 1.0F : 0.0F,
                     vsync_available},
            UiWidget{fullscreen, UiKind::button, column.next(42 * scale), "TOGGLE FULLSCREEN"},
            UiWidget{restart, UiKind::button, column.next(42 * scale), "RESTART GAME"},
            UiWidget{quit, UiKind::button, column.next(42 * scale), "QUIT"},
            UiWidget{slot, UiKind::button, slot_bounds,
                     "SLOT " + std::to_string(selected_slot) + " / 3 - CHANGE"},
            UiWidget{save, UiKind::button, save_bounds, "SAVE"},
            UiWidget{load, UiKind::button, load_bounds, "LOAD"}};
        ui_.layout(panel_, widgets);
    }
    engine::Ui ui_;
    engine::InputGate gate_;
    engine::Rect panel_{};
    float scale_{1};
    int width_{}, height_{};
    bool open_{}, previous_pause_{}, audio_available_{};
};
} // namespace feature_lab
