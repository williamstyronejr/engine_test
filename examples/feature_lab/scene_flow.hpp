#pragma once
#include "persistence.hpp"
#include <iostream>
namespace engine {
class Renderer;
}

namespace feature_lab {
struct FlowStep {
    engine::InputFrame gameplay{};
    bool quit{}, transitioned{}, replaced{};
};
// Title and gameplay share immutable facility content and renderer resources.
// A replacement world is committed by Game::restore only after full validation.
class SceneFlow {
  public:
    enum Id : engine::UiId { new_game = 1, continue_game, slot, resume, quit };
    SceneFlow(Game& game, bool show_title)
        : initial_(game.checkpoint()), title_(show_title), session_(!show_title) {
        if (title_) {
            previous_pause_ = game.paused;
            game.paused = true;
        }
    }
    bool title_open() const { return title_; }
    bool has_session() const { return session_; }
    unsigned selected_slot{1};
    std::string notice;
    const engine::Ui& ui() const { return ui_; }
    bool gameplay_paused(const Game& game) const { return title_ ? previous_pause_ : game.paused; }
    void enter_title(Game& game) {
        if (title_)
            return;
        previous_pause_ = game.paused;
        game.paused = true;
        title_ = true;
        notice.clear();
        width_ = height_ = 0;
        ui_.reset_interaction();
    }
    void start_new(Game& game) {
        game.restore(initial_);
        title_ = false;
        session_ = true;
        notice.clear();
    }
    void continue_slot(Game& game, const engine::UserStorage* storage) {
        if (!storage)
            throw std::runtime_error("User storage unavailable");
        load_slot(*storage, selected_slot, game);
        title_ = false;
        session_ = true;
        notice.clear();
    }
    void resume_session(Game& game) {
        if (!session_)
            throw std::logic_error("No active session");
        game.paused = previous_pause_;
        title_ = false;
        notice.clear();
    }
    void prepare(int width, int height, bool storage) {
        if (title_ && (width_ != width || height_ != height || storage_available_ != storage))
            rebuild(width, height, storage);
    }
    FlowStep update(Game& game, const engine::InputFrame& input, int width, int height,
                    const engine::UserStorage* storage) {
        FlowStep result;
        const bool was_title = title_;
        if (title_) {
            prepare(width, height, storage != nullptr);
            if (engine::button(input, engine::Key::escape).pressed)
                result.quit = true;
            else {
                const auto actions = ui_.update(input);
                for (const auto action : actions.events()) {
                    try {
                        switch (action.id) {
                        case new_game:
                            start_new(game);
                            result.replaced = true;
                            result.transitioned = true;
                            break;
                        case continue_game:
                            continue_slot(game, storage);
                            result.replaced = true;
                            result.transitioned = true;
                            break;
                        case resume:
                            resume_session(game);
                            result.transitioned = true;
                            break;
                        case slot:
                            selected_slot = selected_slot % 3 + 1;
                            notice.clear();
                            rebuild(width, height, storage != nullptr);
                            break;
                        case quit:
                            result.quit = true;
                            break;
                        default:
                            break;
                        }
                    } catch (const std::exception& error) {
                        notice = "TRANSITION FAILED - SESSION RETAINED";
                        std::cerr << "[scene transition] " << error.what() << '\n';
                    }
                    // At most one scene command per fixed tick.
                    break;
                }
            }
        }
        result.gameplay = gate_.route(input, was_title || title_, was_title || title_);
        return result;
    }
    void draw(engine::Renderer& renderer, int width, int height) const;

  private:
    void rebuild(int width, int height, bool storage) {
        width_ = width;
        height_ = height;
        storage_available_ = storage;
        scale_ = std::min({static_cast<float>(std::max(width, 1)) / 640,
                           static_cast<float>(std::max(height, 1)) / 650, 1.0F});
        const auto left = (static_cast<float>(width) - 560 * scale_) * 0.5F;
        const auto top = (static_cast<float>(height) - 570 * scale_) * 0.5F;
        panel_ = {{left, top}, {left + 560 * scale_, top + 570 * scale_}};
        engine::UiColumn column(
            {{left + 32 * scale_, top + 146 * scale_}, {left + 528 * scale_, top + 510 * scale_}},
            12 * scale_);
        using namespace engine;
        const std::array widgets{
            UiWidget{new_game, UiKind::button, column.next(52 * scale_), "NEW GAME"},
            UiWidget{continue_game, UiKind::button, column.next(52 * scale_), "CONTINUE SAVED GAME",
                     0, storage},
            UiWidget{slot, UiKind::button, column.next(44 * scale_),
                     "SLOT " + std::to_string(selected_slot) + " / 3 - CHANGE"},
            UiWidget{resume, UiKind::button, column.next(52 * scale_), "RESUME SESSION", 0,
                     session_},
            UiWidget{quit, UiKind::button, column.next(52 * scale_), "QUIT"}};
        ui_.layout(panel_, widgets);
    }
    Checkpoint initial_;
    engine::Ui ui_;
    engine::InputGate gate_;
    engine::Rect panel_{};
    float scale_{1};
    int width_{}, height_{};
    bool title_{}, session_{}, previous_pause_{}, storage_available_{};
};
} // namespace feature_lab
