#include "engine/assets.hpp"
#include "engine/scene.hpp"
#include "test.hpp"
#include <array>
#include <filesystem>
#include <limits>
#include <random>
#include <unistd.h>

namespace {
using namespace testing;
using namespace engine;
struct TemporaryRoot {
    std::filesystem::path path;
    TemporaryRoot() {
        std::string pattern =
            (std::filesystem::temp_directory_path() / "engine-content-XXXXXX").string();
        if (!mkdtemp(pattern.data()))
            throw std::runtime_error("Cannot create test temporary directory");
        path = pattern;
    }
    ~TemporaryRoot() {
        std::error_code ignored;
        std::filesystem::remove_all(path, ignored);
    }
};
const TextureData sample{2, 1, {255, 0, 0, 255, 0, 0, 255, 128}};
constexpr std::string_view scene_text = R"(scene 1
entity 2 "child \"quoted\"" "core" 1 2 3 0 1 1
sprite 2 "image.etex" 1 2 1 0.5 0 1 10
collider 2 0.5 1
entity 1 "parent" "group" 0 4 5 0 2 2
end
)";
TEST(texture_roundtrip_and_exact_length) {
    const auto bytes = encode_texture(sample);
    const auto decoded = decode_texture(bytes);
    CHECK(decoded.width == 2 && decoded.height == 1 && decoded.rgba == sample.rgba);
    for (std::size_t size = 0; size < bytes.size(); ++size)
        rejects([&] { decode_texture(std::span(bytes).first(size)); });
    auto extra = bytes;
    extra.push_back(0);
    rejects([&] { decode_texture(extra); });
    auto bad = bytes;
    bad[4] = 2;
    rejects([&] { decode_texture(bad); });
    bad = bytes;
    bad[8] = 0;
    rejects([&] { decode_texture(bad); });
    bad = bytes;
    bad[8] = 255;
    bad[9] = 255;
    rejects([&] { decode_texture(bad); });
    auto invalid = sample;
    invalid.rgba.pop_back();
    rejects([&] { encode_texture(invalid); });
    rejects([] { TextureData{4096, 4096, {}}.validate(); });
}
TEST(asset_root_paths_bounds_and_atomic_write) {
    TemporaryRoot directory, outside;
    AssetRoot root(directory.path);
    root.write_text("scene.txt", "old complete file");
    root.write_text("scene.txt", "new complete file");
    CHECK(root.text("scene.txt") == "new complete file");
    rejects([&] { root.read("scene.txt", 2); });
    rejects([&] { root.read(".", 100); });
    rejects([&] { root.read("../outside", 100); });
    rejects([&] { root.read("/etc/passwd", 100); });
    rejects([&] { root.write_text("missing/scene.txt", "bad"); });
    CHECK(root.text("scene.txt") == "new complete file");
    std::filesystem::create_symlink(outside.path, directory.path / "escape");
    rejects([&] { root.write_text("escape/invalid", "bad"); });
    std::filesystem::create_directory(directory.path / "not-a-file");
    rejects([&] { root.write_text("not-a-file", "bad"); }); // Rename fails; temporary cleaned up.
    rejects([&] { root.read("not-a-file", 100); });
    for (const auto& file : std::filesystem::directory_iterator(directory.path))
        CHECK(!file.path().filename().string().starts_with(".engine-save-"));
}
TEST(cache_shares_snapshots_and_preserves_failed_reload) {
    TemporaryRoot directory;
    AssetRoot root(directory.path);
    root.write_atomic("a.etex", encode_texture(sample));
    TextureCache cache(root);
    auto old = cache.load("a.etex");
    CHECK(cache.load("a.etex") == old);
    root.write_text("a.etex", "broken");
    rejects([&] { cache.reload("a.etex"); });
    CHECK(cache.load("a.etex") == old);
    auto changed = sample;
    changed.rgba[0] = 42;
    root.write_atomic("a.etex", encode_texture(changed));
    auto fresh = cache.reload("a.etex");
    CHECK(fresh != old && fresh->rgba[0] == 42 && old->rgba[0] == 255);
    CHECK(cache.load("a.etex") == fresh);
    fresh.reset();
    cache.collect();
    CHECK(cache.entries() == 0); // Old external snapshot remains valid.
    CHECK(old->rgba[0] == 255);
    std::string error;
    auto fallback = cache.load_or_fallback("missing.etex", error);
    CHECK(!error.empty() && fallback->width == 2 && fallback->rgba[0] == 255);
}
TEST(entity_lifetime_compaction_and_foreign_handles) {
    Scene a, b;
    const auto first = a.create(1, "a"), second = a.create(2, "b"),
               foreign = b.create(1, "foreign");
    a.destroy(first);
    CHECK(!a.valid(first));
    CHECK(a.get(second).id == 2);
    const auto reused = a.create(3, "reused");
    CHECK(reused.index == first.index);
    CHECK(reused.generation != first.generation);
    rejects([&] { a.get(first); });
    rejects([&] { a.get(foreign); });
    rejects([&] { a.create(2, "duplicate"); });
    rejects([&] { a.create(0, "zero"); });
    a.set_transform(second, {{7, 3}, 0, {1, 1}});
    NEAR(a.world_transform(second).x, 7);
    Scene moved(std::move(a));
    CHECK(moved.valid(second));
    CHECK(!a.valid(second));
    a.create(7, "moved-from reused");
    CHECK(!a.valid(second));
}
TEST(hierarchy_cycles_subtree_deletion_and_deferred_changes) {
    Scene scene;
    const auto root = scene.create(1, "root"), child = scene.create(2, "child"),
               grandchild = scene.create(3, "grandchild");
    scene.set_parent(child, root);
    scene.set_parent(grandchild, child);
    scene.set_transform(root, {{2, 3}, 0, {2, 2}});
    scene.set_transform(child, {{1, 2}, 0, {1, 1}});
    NEAR(scene.world_transform(grandchild).x, 4);
    NEAR(scene.world_transform(grandchild).y, 7);
    rejects([&] { scene.set_parent(root, grandchild); });
    rejects([&] { scene.set_parent(child, child); });
    scene.each([&](const SceneNode& node) {
        rejects([&] { scene.create(10, "during iteration"); });
        rejects([&] { scene.destroy(node.entity); });
        if (node.entity == child)
            scene.defer_destroy(child);
    });
    CHECK(scene.valid(child));
    scene.flush();
    CHECK(scene.valid(root) && !scene.valid(child) && !scene.valid(grandchild) &&
          scene.size() == 1);
    try {
        scene.each([](const SceneNode&) { throw std::runtime_error("visitor"); });
    } catch (...) {
    }
    scene.create(4, "after failed visitor");
    CHECK(scene.size() == 2);
}
TEST(scene_depth_capacity_and_transform_limits) {
    Scene scene;
    Entity last;
    for (std::size_t i = 0; i < Scene::max_depth; ++i) {
        const auto e = scene.create(i + 1, "node");
        scene.set_parent(e, last);
        last = e;
    }
    const auto excess = scene.create(100, "excess");
    rejects([&] { scene.set_parent(excess, last); });
    rejects([&] { scene.set_transform(excess, {{}, 0, {0, 1}}); });
    rejects(
        [&] { scene.set_transform(excess, {{}, std::numeric_limits<float>::infinity(), {1, 1}}); });
    Scene full;
    for (std::size_t i = 1; i <= Scene::capacity; ++i)
        full.create(i, "node");
    rejects([&] { full.create(Scene::capacity + 1, "overflow"); });
}
TEST(scene_roundtrip_and_atomic_replacement) {
    auto scene = parse_scene(scene_text, "fixture.scene");
    const auto old = scene.find(2);
    CHECK(scene.size() == 2);
    NEAR(scene.world_transform(old).x, 8);
    NEAR(scene.world_transform(old).y, 11);
    const auto serialized = serialize_scene(scene);
    auto copy = parse_scene(serialized);
    CHECK(serialize_scene(copy) == serialized);
    CHECK(copy.get(copy.find(2)).name == "child \"quoted\"");
    rejects([&] { replace_scene(scene, "scene 2\nend\n"); });
    CHECK(scene.valid(old));
    replace_scene(scene, serialized);
    CHECK(!scene.valid(old));
    CHECK(scene.find(2));
    TemporaryRoot directory;
    AssetRoot root(directory.path);
    root.write_text("scene.txt", serialized);
    CHECK(serialize_scene(parse_scene(root.text("scene.txt"))) == serialized);
}
TEST(scene_rejects_malformed_records) {
    for (const auto text :
         {"", "scene 1\n", "scene 2\nend\n", "scene 1\nend\ngarbage\n",
          "scene 1\nentity 1 a b 0 nan 0 0 1 1\nend\n",
          "scene 1\nentity 1 a b 999 0 0 0 1 1\nend\n", "scene 1\nentity 1 a b 1 0 0 0 1 1\nend\n",
          "scene 1\nentity 1 a b 0 0 0 0 1 1\nentity 1 a b 0 0 0 0 1 1\nend\n",
          "scene 1\nentity 1 a b 2 0 0 0 1 1\nentity 2 a b 1 0 0 0 1 1\nend\n",
          "scene 1\nentity 1 a b 0 0 0 0 1 1\nsprite 1 ../outside 1 1 1 1 1 1 0\nend\n",
          "scene 1\nentity 1 a b 0 0 0 0 1 1\ncollider 1 1 1\ncollider 1 1 1\nend\n",
          "scene 1\nentity 1 \"bad\\n\" b 0 0 0 0 1 1\nend\n",
          "scene 1\nentity 1 a b 0 0 0 0 1 1 EXTRA\nend\n"})
        rejects([&] { parse_scene(text); });
    try {
        parse_scene("scene 1\nunknown\nend\n", "bad.scene");
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("bad.scene:2:") != std::string::npos);
    }
    rejects([] { parse_scene(std::string(4097, ' ')); });
}
TEST(scene_seeded_roundtrip_stress) {
    std::mt19937 random(4711);
    std::uniform_real_distribution<float> number(-10, 10);
    Scene scene;
    for (std::uint64_t i = 1; i <= 150; ++i) {
        const auto e = scene.create(i, "node " + std::to_string(i));
        scene.set_transform(e, {{number(random), number(random)}, number(random), {1, 1}});
        if (i > 1)
            scene.set_parent(e, scene.find(i / 2));
        if (i % 3 == 0)
            scene.set_sprite(e, {"texture.etex", {1, 1}, 1, 1, 1, 1, static_cast<int>(i)});
    }
    const auto expected = serialize_scene(scene);
    for (int i = 0; i < 20; ++i) {
        replace_scene(scene, expected);
        CHECK(serialize_scene(scene) == expected);
    }
}
} // namespace
int main() {
    return testing::run_tests();
}
