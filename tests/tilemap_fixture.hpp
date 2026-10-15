#pragma once
#include "../examples/feature_lab/tilemap_assets.hpp"
#include "test.hpp"
#include <filesystem>
#include <unistd.h>

namespace testing {
struct TilemapFiles {
    static std::filesystem::path create() {
        auto pattern = (std::filesystem::temp_directory_path() / "tilemap-reload-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        return pattern;
    }
    std::filesystem::path path{create()};
    engine::AssetRoot root{path};
    TilemapFiles() {
        std::filesystem::copy(TEST_ASSET_ROOT, path,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    }
    ~TilemapFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    void write(const engine::TileMapData& map,
               feature_lab::Level level = feature_lab::Level::facility) {
        root.write_atomic(feature_lab::TilemapAssets::key(level), engine::encode_tilemap(map));
    }
};
inline void put(engine::TileMapData& map, engine::Vec2 point, std::uint16_t id,
                std::size_t layer = 1) {
    const auto x = static_cast<std::size_t>(std::floor((point.x - map.origin.x) / map.tile_size));
    const auto y = static_cast<std::size_t>(std::floor((point.y - map.origin.y) / map.tile_size));
    CHECK(x < map.width && y < map.height);
    map.layers.at(layer).cells.at(y * map.width + x) = id;
}
} // namespace testing
