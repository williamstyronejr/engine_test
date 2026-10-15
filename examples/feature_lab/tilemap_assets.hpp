#pragma once
#include "game.hpp"
#include <iostream>
#include <limits>

namespace feature_lab {
class TilemapAssets {
  public:
    static constexpr std::string_view key(Level level) {
        return level == Level::facility ? "facility.etmp" : "annex.etmp";
    }
    // Recording/playback uses fixed content; callers must also omit its watch group.
    explicit TilemapAssets(bool locked = false) : locked_(locked) {}
    bool reload(Game& game, const engine::AssetRoot& assets,
                const std::function<void(const engine::TileMap&)>& prepare = {}) {
        if (locked_)
            return false;
        const auto source = key(game.level);
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Tilemap revision exhausted");
            auto candidate = engine::decode_tilemap(assets.read(source, 4 * 1024 * 1024), source);
            game.reload_tilemap(std::move(candidate), prepare);
            ++revision_;
            applied_ = true;
            diagnostic_.clear();
        } catch (const std::exception& error) {
            applied_ = false;
            diagnostic_ = std::string(source) + ": " + error.what();
        }
        std::cerr << "[tilemap " << (applied_ ? "applied" : "retained")
                  << "] revision=" << revision_ << " source=" << source << std::endl;
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
            return "TILEMAP LOCKED / REPLAY CONTENT";
        return std::string(applied_ ? "TILEMAP READY R" : "TILEMAP FAILED - KEPT R") +
               std::to_string(revision_);
    }

  private:
    std::uint64_t revision_{1};
    bool locked_{}, applied_{true};
    std::string diagnostic_;
};
} // namespace feature_lab
