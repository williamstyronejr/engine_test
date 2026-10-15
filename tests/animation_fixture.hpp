#pragma once
#include "../examples/feature_lab/animation_assets.hpp"
#include "test.hpp"
#include <filesystem>
#include <unistd.h>

namespace testing {
struct AnimationFiles {
    static std::filesystem::path create() {
        auto pattern =
            (std::filesystem::temp_directory_path() / "animation-reload-XXXXXX").string();
        CHECK(mkdtemp(pattern.data()));
        return pattern;
    }
    std::filesystem::path path{create()};
    engine::AssetRoot root{path};
    AnimationFiles() {
        std::filesystem::copy(TEST_ASSET_ROOT, path,
                              std::filesystem::copy_options::recursive |
                                  std::filesystem::copy_options::overwrite_existing);
    }
    ~AnimationFiles() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    void write(const engine::AnimationSet& set) {
        root.write_atomic(feature_lab::AnimationAssets::key, engine::encode_animations(set));
    }
};
inline engine::AnimationClip& clip(engine::AnimationSet& set, std::string_view name) {
    for (auto& c : set.clips)
        if (c.name == name)
            return c;
    throw std::runtime_error("Missing fixture clip");
}
} // namespace testing
