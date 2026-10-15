#pragma once
#include "game.hpp"
#include <iostream>
#include <limits>

namespace feature_lab {
class AnimationAssets {
  public:
    static constexpr std::string_view key = "facility.eani";
    // Recording/playback uses fixed content; callers must also omit its watch group.
    explicit AnimationAssets(bool locked = false) : locked_(locked) {}
    bool reload(Game& game, const engine::AssetRoot& assets) {
        if (locked_)
            return false;
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Animation revision exhausted");
            auto candidate = std::make_shared<const engine::AnimationSet>(
                engine::decode_animations(assets.read(key, 1024 * 1024), key));
            game.reload_animations(std::move(candidate));
            ++revision_;
            applied_ = true;
            diagnostic_.clear();
        } catch (const std::exception& error) {
            applied_ = false;
            diagnostic_ = std::string(key) + ": " + error.what();
        }
        std::cerr << "[animation " << (applied_ ? "applied" : "retained")
                  << "] revision=" << revision_ << " source=" << key << std::endl;
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
    std::string status() const {
        if (locked_)
            return "ANIMATION LOCKED / REPLAY CONTENT";
        return std::string(applied_ ? "ANIMATION READY R" : "ANIMATION FAILED - KEPT R") +
               std::to_string(revision_);
    }

  private:
    std::uint64_t revision_{1};
    bool locked_{}, applied_{true};
    std::string diagnostic_;
};
} // namespace feature_lab
