#pragma once
#include "engine/asset_watch.hpp"
#include "font_assets.hpp"
#include "shader_tools.hpp"
#include "texture_assets.hpp"
#include <functional>

namespace feature_lab {
class AssetReload {
  public:
    static constexpr std::uint32_t shader_group = 1, texture_group = 2, font_group = 4,
                                   animation_group = 8, facility_map_group = 16,
                                   annex_map_group = 32, facility_scene_group = 64,
                                   annex_scene_group = 128;
    // Level callbacks return true when they attempt the active level's reload.
    AssetReload(const engine::AssetRoot& assets, const TextureAssets& textures,
                std::function<void()> animation_reload = {},
                std::function<bool(std::string_view)> tilemap_reload = {},
                std::function<bool(std::string_view)> scene_reload = {})
        : watch_(assets, files(textures, static_cast<bool>(animation_reload),
                               static_cast<bool>(tilemap_reload), static_cast<bool>(scene_reload))),
          animation_reload_(std::move(animation_reload)),
          tilemap_reload_(std::move(tilemap_reload)), scene_reload_(std::move(scene_reload)) {
        std::cerr << "[watch] enabled files=" << watch_.size() << '\n';
    }
    // Call once per frame, outside render passes. A manual command takes priority
    // over an automatic transaction that becomes ready in the same frame.
    void update(engine::Renderer& renderer, const engine::AssetRoot& assets, ShaderTools& shaders,
                TextureAssets& textures, FontAssets& fonts, std::uint32_t manual = 0,
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
        if (changed & font_group) {
            ++font_reloads_;
            fonts.reload(renderer, assets);
        }
        if (changed & animation_group) {
            ++animation_reloads_;
            animation_reload_();
        }
        if ((changed & facility_map_group) && tilemap_reload_("facility.etmp"))
            ++tilemap_reloads_;
        if ((changed & annex_map_group) && tilemap_reload_("annex.etmp"))
            ++tilemap_reloads_;
        if ((changed & facility_scene_group) && scene_reload_("facility.scene"))
            ++scene_reloads_;
        if ((changed & annex_scene_group) && scene_reload_("annex.scene"))
            ++scene_reloads_;
    }
    std::uint64_t scene_reloads() const { return scene_reloads_; }
    std::uint64_t tilemap_reloads() const { return tilemap_reloads_; }
    std::uint64_t animation_reloads() const { return animation_reloads_; }
    std::uint64_t font_reloads() const { return font_reloads_; }
    std::uint64_t shader_reloads() const { return shader_reloads_; }
    std::uint64_t texture_reloads() const { return texture_reloads_; }

  private:
    static std::vector<engine::WatchedAsset> files(const TextureAssets& textures, bool animations,
                                                   bool tilemaps, bool scenes) {
        std::vector<engine::WatchedAsset> result{{"shaders/sprite.vert", shader_group},
                                                 {"shaders/sprite.frag", shader_group},
                                                 {std::string(FontAssets::key), font_group}};
        if (animations)
            result.push_back({"facility.eani", animation_group});
        if (tilemaps) {
            result.push_back({"facility.etmp", facility_map_group});
            result.push_back({"annex.etmp", annex_map_group});
        }
        if (scenes) {
            result.push_back({"facility.scene", facility_scene_group});
            result.push_back({"annex.scene", annex_scene_group});
        }
        for (const auto& key : textures.keys())
            result.push_back({key, texture_group});
        return result;
    }
    engine::AssetWatcher watch_;
    std::function<void()> animation_reload_;
    std::function<bool(std::string_view)> tilemap_reload_, scene_reload_;
    std::uint64_t shader_reloads_{}, texture_reloads_{}, font_reloads_{}, animation_reloads_{};
    std::uint64_t tilemap_reloads_{}, scene_reloads_{};
    bool reported_{}, notifications_{};
};
} // namespace feature_lab
