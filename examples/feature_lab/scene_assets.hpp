#pragma once
#include "game.hpp"
#include "hud_text.hpp"
#include <iostream>
#include <limits>

namespace feature_lab {
class SceneAssets {
  public:
    static constexpr std::string_view key(Level level) {
        return level == Level::facility ? "facility.scene" : "annex.scene";
    }
    // Recording/playback uses fixed content; callers must also omit its watch group.
    explicit SceneAssets(bool locked = false) : locked_(locked) {}
    bool reload(Game& game, const engine::AssetRoot& assets,
                const std::function<void(const Game&)>& prepare = {}) {
        if (locked_)
            return false;
        const auto source = key(game.level);
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Scene revision exhausted");
            auto candidate = assets.text(source);
            game.reload_scene(std::move(candidate), prepare);
            ++revision_;
            applied_ = true;
            diagnostic_.clear();
        } catch (const std::exception& error) {
            applied_ = false;
            diagnostic_ = std::string(source) + ": " + error.what();
        }
        std::cerr << "[scene " << (applied_ ? "applied" : "retained") << "] revision=" << revision_
                  << " source=" << source << std::endl;
        if (!diagnostic_.empty())
            std::cerr << diagnostic_ << '\n';
        return applied_;
    }
    // New game / level / save replacement installs its own validated snapshot.
    void replaced() {
        revision_ = 1;
        applied_ = true;
        diagnostic_.clear();
    }
    bool locked() const { return locked_; }
    bool applied() const { return applied_; }
    std::uint64_t revision() const { return revision_; }
    const std::string& diagnostic() const { return diagnostic_; }
    std::string_view status(std::string& output) const {
        if (locked_)
            return format_text(output, "SCENE LOCKED / REPLAY CONTENT");
        return format_text(output, applied_ ? "SCENE READY R" : "SCENE FAILED - KEPT R", revision_);
    }
    std::string status() const {
        std::string output;
        status(output);
        return output;
    }

  private:
    std::uint64_t revision_{1};
    bool locked_{}, applied_{true};
    std::string diagnostic_;
};
} // namespace feature_lab
