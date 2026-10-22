#include "../examples/feature_lab/asset_validation.hpp"
#include "../examples/feature_lab/game.hpp"
#include "test.hpp"
#include <filesystem>
#include <unistd.h>

namespace {
using namespace testing;
using namespace engine;
using namespace feature_lab;
struct Fixture {
    std::filesystem::path path;
    Fixture() {
        std::string pattern = (std::filesystem::temp_directory_path() / "engine-validate-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create validation fixture");
        path = pattern;
        std::filesystem::copy(TEST_ASSET_ROOT, path, std::filesystem::copy_options::recursive);
    }
    ~Fixture() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
    AssetRoot root() const { return AssetRoot(path); }
};
bool failure(const AssetValidation& report, std::string_view name, std::string_view text) {
    return std::any_of(report.checks.begin(), report.checks.end(), [&](const auto& check) {
        return check.name == name && !check.error.empty() && check.error.find(text) != std::string::npos;
    });
}
TEST(shipped_assets_pass_without_fallbacks_and_with_stable_order) {
    const AssetRoot root(TEST_ASSET_ROOT);
    const auto a = validate_assets(root), b = validate_assets(root);
    CHECK(a.failures() == 0 && a.checks.size() == 20 && b.checks.size() == a.checks.size());
    for (std::size_t i = 0; i < a.checks.size(); ++i)
        CHECK(a.checks[i].name == b.checks[i].name && a.checks[i].error == b.checks[i].error);
}
TEST(independent_malformed_files_are_all_reported_then_repaired) {
    Fixture fixture;
    auto root = fixture.root();
    for (const auto key : {"facility.scene", "annex.etmp", "facility.eani", "fonts/ui.efnt",
                           "sounds/music.wav", "textures/checker.etex", "shaders/sprite.vert"})
        root.write_text(key, "");
    const auto report = validate_assets(root);
    for (const auto key : {"facility.scene", "annex.etmp", "facility.eani", "fonts/ui.efnt",
                           "sounds/music.wav", "textures/checker.etex"})
        CHECK(failure(report, key, ""));
    CHECK(failure(report, "shaders/sprite.vert (source only)", "empty"));
    CHECK(failure(report, "FACILITY gameplay", ""));
    CHECK(failure(report, "ANNEX gameplay", ""));
    std::filesystem::copy(TEST_ASSET_ROOT, fixture.path,
                          std::filesystem::copy_options::recursive |
                          std::filesystem::copy_options::overwrite_existing);
    CHECK(validate_assets(root).failures() == 0);
}
TEST(scene_budget_and_semantic_errors_have_level_context) {
    Fixture fixture;
    auto root = fixture.root();
    const auto original = root.text("annex.scene");
    auto scene = parse_scene(original);
    while (scene.size() < Game::scene_limits.entities)
        scene.create(1000 + scene.size(), "extra", "decor");
    root.write_text("annex.scene", serialize_scene(scene));
    CHECK(validate_assets(root).failures() == 0);
    scene.create(2000, "overflow");
    root.write_text("annex.scene", serialize_scene(scene));
    auto report = validate_assets(root);
    CHECK(failure(report, "annex.scene", "Scene entity budget exceeded"));
    CHECK(failure(report, "ANNEX gameplay", "Scene entity budget exceeded"));
    scene = parse_scene(original);
    scene.destroy(scene.find(20));
    root.write_text("annex.scene", serialize_scene(scene));
    report = validate_assets(root);
    CHECK(failure(report, "ANNEX gameplay", "requires player"));
    scene = parse_scene(original);
    scene.set_transform(scene.find(20), {{1000, 1000}, 0, {1, 1}});
    root.write_text("annex.scene", serialize_scene(scene));
    CHECK(failure(validate_assets(root), "ANNEX gameplay", "outside the map"));
}
TEST(required_walk_animation_and_atlas_dimensions_are_checked) {
    Fixture fixture;
    auto root = fixture.root();
    auto clips = decode_animations(root.read("facility.eani", 1024 * 1024));
    for (auto& clip : clips.clips)
        if (clip.name == "walk")
            clip.loop = false;
    root.write_atomic("facility.eani", encode_animations(clips));
    CHECK(failure(validate_assets(root), "FACILITY gameplay", "required animation: walk"));
    std::filesystem::copy_file(std::filesystem::path(TEST_ASSET_ROOT) / "facility.eani",
                               fixture.path / "facility.eani",
                               std::filesystem::copy_options::overwrite_existing);
    root.write_atomic("textures/actors.etex", encode_texture({1, 1, {255, 255, 255, 255}}));
    CHECK(failure(validate_assets(root), "textures/actors.etex", "atlas grid"));
}
TEST(missing_referenced_textures_and_transition_residency_are_rejected) {
    Fixture fixture;
    auto root = fixture.root();
    auto text = root.text("annex.scene");
    const auto at = text.find("textures/checker.etex");
    CHECK(at != std::string::npos);
    text.replace(at, std::string_view("textures/checker.etex").size(), "textures/new.etex");
    root.write_text("annex.scene", text);
    auto report = validate_assets(root);
    CHECK(failure(report, "textures/new.etex", "Not a regular asset file"));
    root.write_atomic("textures/new.etex", encode_texture({1, 1, {255, 255, 255, 255}}));
    report = validate_assets(root);
    CHECK(!failure(report, "textures/new.etex", ""));
    CHECK(failure(report, "FACILITY -> ANNEX texture residency", "textures/new.etex"));
    CHECK(failure(report, "ANNEX -> FACILITY texture residency", "textures/checker.etex"));
}
TEST(asset_root_confinement_and_bounded_shader_read_are_preserved) {
    Fixture fixture, outside;
    std::filesystem::remove(fixture.path / "textures/white.etex");
    std::filesystem::create_symlink(outside.path / "textures/white.etex",
                                    fixture.path / "textures/white.etex");
    auto root = fixture.root();
    root.write_text("shaders/sprite.frag", std::string(65537, 'x'));
    const auto report = validate_assets(root);
    CHECK(failure(report, "textures/white.etex", "escapes its root"));
    CHECK(failure(report, "shaders/sprite.frag (source only)", "size limit"));
}
} // namespace
int main() { return testing::run_tests(); }
