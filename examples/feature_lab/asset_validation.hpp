#pragma once
#include "engine/assets.hpp"
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
} // namespace feature_lab
