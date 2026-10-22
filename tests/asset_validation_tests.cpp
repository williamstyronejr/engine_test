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
    explicit Fixture(bool bundle = true) {
        std::string pattern = (std::filesystem::temp_directory_path() / "engine-validate-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create validation fixture");
        path = pattern;
        if (bundle)
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
TEST(single_files_validate_in_isolation_without_extension_inference) {
    const AssetRoot shipped(TEST_ASSET_ROOT);
    const std::array examples{
        std::pair{AssetFormat::scene, "facility.scene"},
        std::pair{AssetFormat::tilemap, "annex.etmp"},
        std::pair{AssetFormat::animation, "facility.eani"},
        std::pair{AssetFormat::texture, "textures/actors.etex"},
        std::pair{AssetFormat::font, "fonts/ui.efnt"},
        std::pair{AssetFormat::sound, "sounds/door.wav"}};
    for (const auto& [format, source] : examples) {
        Fixture fixture(false);
        auto root = fixture.root();
        const auto data = shipped.read(source, 64 * 1024 * 1024);
        root.write_atomic("draft.data", data);
        auto report = validate_asset_file(root, format, "draft.data");
        CHECK(report.checks.size() == 1 && report.failures() == 0);
        CHECK(report.checks.front().name == "draft.data");
        // Only the selected file exists: even scene/animation/atlas references
        // are deliberately unresolved. Bundle requirements are not applied.
        CHECK(std::distance(std::filesystem::directory_iterator(fixture.path),
                            std::filesystem::directory_iterator{}) == 1);
        root.write_atomic("draft.data", std::span(data).first(data.size() / 2));
        CHECK(validate_asset_file(root, format, "draft.data").failures() == 1);
        root.write_atomic("draft.data", data);
        CHECK(validate_asset_file(root, format, "draft.data").failures() == 0);
        CHECK(root.read("draft.data", data.size()) == data);
    }
}
TEST(single_scene_limits_and_format_semantics_are_independent_of_the_game) {
    Fixture fixture(false);
    auto root = fixture.root();
    root.write_text("empty.scene", "scene 1\nend\n");
    CHECK(validate_asset_file(root, AssetFormat::scene, "empty.scene", {0}).failures() == 0);
    Scene scene;
    for (std::size_t i = 0; i < 257; ++i)
        scene.create(i + 1, "generic entity");
    root.write_text("draft.scene", serialize_scene(scene));
    CHECK(validate_asset_file(root, AssetFormat::scene, "draft.scene").failures() == 0);
    CHECK(validate_asset_file(root, AssetFormat::scene, "draft.scene", {257}).failures() == 0);
    CHECK(failure(validate_asset_file(root, AssetFormat::scene, "draft.scene", {256}),
                  "draft.scene", "Scene entity budget exceeded"));
    CHECK(failure(validate_asset_file(root, AssetFormat::scene, "draft.scene", {0}),
                  "draft.scene", "Scene entity budget exceeded"));
    CHECK(validate_asset_file(root, AssetFormat::scene, "draft.scene", {4097}).failures() == 1);
    CHECK(validate_asset_file(root, AssetFormat::texture, "draft.scene").failures() == 1);
    root.write_text("draft.scene", "scene 1\nentity 1 child decor 99 0 0 0 1 1\nend\n");
    CHECK(failure(validate_asset_file(root, AssetFormat::scene, "draft.scene"),
                  "draft.scene", "draft.scene:2:"));
    CHECK(validate_asset_file(root, AssetFormat::font, "empty.scene", {1}).failures() == 1);
}
TEST(single_file_reads_preserve_path_confinement_and_file_size_bounds) {
    Fixture fixture(false), outside(false);
    auto root = fixture.root();
    outside.root().write_text("external.scene", "scene 1\nend\n");
    std::filesystem::create_symlink(outside.path / "external.scene", fixture.path / "link.scene");
    CHECK(failure(validate_asset_file(root, AssetFormat::scene, "link.scene"),
                  "link.scene", "escapes its root"));
    CHECK(validate_asset_file(root, AssetFormat::scene, "../external.scene").failures() == 1);
    CHECK(validate_asset_file(root, AssetFormat::scene,
                              (outside.path / "external.scene").string()).failures() == 1);
    CHECK(validate_asset_file(root, AssetFormat::scene, "missing.scene").failures() == 1);
    root.write_text("huge.scene", std::string(4 * 1024 * 1024 + 1, ' '));
    CHECK(failure(validate_asset_file(root, AssetFormat::scene, "huge.scene"),
                  "huge.scene", "size limit"));
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
