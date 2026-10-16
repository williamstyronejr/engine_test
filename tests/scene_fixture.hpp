#pragma once
#include "../examples/feature_lab/scene_assets.hpp"
#include "test.hpp"
#include <filesystem>
#include <unistd.h>

namespace testing {
struct SceneFiles {
    static std::filesystem::path create() {
        auto pattern = (std::filesystem::temp_directory_path() / "scene-reload-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        return pattern;
    }
    std::filesystem::path path{create()};
    engine::AssetRoot root{path};
    SceneFiles() {
        std::filesystem::copy(TEST_ASSET_ROOT, path,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    }
    ~SceneFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    std::string text(feature_lab::Level level = feature_lab::Level::facility) const {
        return root.text(feature_lab::SceneAssets::key(level));
    }
    void write(std::string_view value, feature_lab::Level level = feature_lab::Level::facility) {
        root.write_text(feature_lab::SceneAssets::key(level), value);
    }
};
inline std::string with_wall(std::string_view text, engine::Vec2 point = {-24.5F, -9.5F},
                             std::string texture = "textures/white.etex") {
    auto scene = engine::parse_scene(text);
    auto wall = scene.create(100, "Reload wall", "decoration");
    scene.set_transform(wall, {point});
    scene.set_collider(wall, {{0.5F, 0.5F}});
    scene.set_sprite(wall, {std::move(texture), {1, 1}, 0, 1, 0, 1, 40});
    return engine::serialize_scene(scene);
}
} // namespace testing
