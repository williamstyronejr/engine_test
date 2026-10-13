#pragma once
#include "engine/asset_watch.hpp"
#include "shader_tools.hpp"
#include "texture_assets.hpp"

namespace feature_lab {
class AssetReload {
  public:
    static constexpr std::uint32_t shader_group = 1, texture_group = 2;
    AssetReload(const engine::AssetRoot& assets, const TextureAssets& textures)
        : watch_(assets, files(textures)) {
        std::cerr << "[watch] enabled files=" << watch_.size() << '\n';
    }
    // Call once per frame, outside render passes. A manual command takes priority
    // over an automatic transaction that becomes ready in the same frame.
    void update(engine::Renderer& renderer, const engine::AssetRoot& assets, ShaderTools& shaders,
                TextureAssets& textures, std::uint32_t manual = 0,
                engine::AssetWatcher::Time now = engine::AssetWatcher::Clock::now()) {
        const auto changed = watch_.poll(now) & ~manual;
        const bool notifications = watch_.notifications_active();
        if (!reported_ || notifications != notifications_) {
            reported_ = true;
            notifications_ = notifications;
            std::cerr << "[watch] "
                      << (notifications ? "notifications active" : "metadata fallback") << '\n';
        }
        if (changed & shader_group) {
            ++shader_reloads_;
            shaders.reload(renderer, assets);
        }
        if (changed & texture_group) {
            ++texture_reloads_;
            textures.reload(renderer);
        }
    }
    std::uint64_t shader_reloads() const { return shader_reloads_; }
    std::uint64_t texture_reloads() const { return texture_reloads_; }

  private:
    static std::vector<engine::WatchedAsset> files(const TextureAssets& textures) {
        std::vector<engine::WatchedAsset> result{{"shaders/sprite.vert", shader_group},
                                                 {"shaders/sprite.frag", shader_group}};
        for (const auto& key : textures.keys())
            result.push_back({key, texture_group});
        return result;
    }
    engine::AssetWatcher watch_;
    std::uint64_t shader_reloads_{}, texture_reloads_{};
    bool reported_{}, notifications_{};
};
} // namespace feature_lab
