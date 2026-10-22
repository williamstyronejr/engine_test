#pragma once
#include "engine/assets.hpp"
#include "engine/scene.hpp"
#include <string>
#include <vector>

namespace feature_lab {
struct AssetCheck {
    std::string name;
    std::string error; // Empty on success. Checks run in stable order.
};
struct AssetValidation {
    std::vector<AssetCheck> checks;
    std::size_t failures() const;
};
// Read-only Feature Lab content checks; no device, fallback assets or writes.
// Covers the shipped inventory, referenced textures, and both level constructors.
AssetValidation validate_assets(const engine::AssetRoot& assets);
enum class AssetFormat { scene, tilemap, animation, texture, font, sound };
// One root-relative file, using the selected decoder regardless of its extension.
// Does not resolve referenced assets or apply Feature Lab gameplay requirements.
// Scene limits default to the engine maximum; other formats require default limits.
AssetValidation validate_asset_file(const engine::AssetRoot& assets, AssetFormat format,
                                    std::string_view key, engine::Scene::Limits limits = {});
} // namespace feature_lab
