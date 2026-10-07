#include "engine/animation.hpp"
#include "engine/collision.hpp"
#include "engine/tilemap.hpp"
#include "test.hpp"
#include <limits>
#include <random>
#include <set>

namespace {
using namespace engine;
using namespace testing;
TileMapData tiles(std::uint32_t width = 35, std::uint32_t height = 19) {
    TileMapData m;
    m.width = width;
    m.height = height;
    m.origin = {-20, -10};
    m.tile_size = 2;
    m.texture = "tiles.etex";
    m.atlas_columns = 2;
    m.palette = {{0, false}, {0, false}, {1, true}};
    m.layers = {{-1, std::vector<std::uint16_t>(width * height)},
                {5, std::vector<std::uint16_t>(width * height)}};
    for (std::size_t i = 0; i < m.layers[0].cells.size(); ++i) {
        m.layers[0].cells[i] = i % 3 ? 1 : 2;
        m.layers[1].cells[i] = i % 7 ? 0 : 2;
    }
    return m;
}
std::shared_ptr<const AnimationSet> clips() {
    return std::make_shared<const AnimationSet>(
        AnimationSet{"actors.etex",
                     4,
                     2,
                     {{"loop", true, {{0, 2}, {5, 3}, {7, 1}}},
                      {"once", false, {{1, 2}, {2, 3}, {3, 1}}},
                      {"single", true, {{4, 1}}}}});
}
void put32(std::vector<std::uint8_t>& bytes, std::size_t offset, std::uint32_t value) {
    for (unsigned i = 0; i < 4; ++i)
        bytes.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
TEST(tilemap_binary_roundtrip_and_corruption) {
    const auto source = tiles();
    const auto bytes = encode_tilemap(source);
    CHECK(encode_tilemap(decode_tilemap(bytes)) == bytes);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        rejects([&] { decode_tilemap(std::span(bytes).first(i)); });
    auto bad = bytes;
    bad.push_back(0);
    rejects([&] { decode_tilemap(bad); });
    for (auto offset : {0U, 4U, 8U, 12U, 16U, 32U, 40U}) {
        bad = bytes;
        put32(bad, offset, 0xffffffffU);
        rejects([&] { decode_tilemap(bad, "broken.etmp"); });
    }
    bad = bytes;
    put32(bad, 44 + source.texture.size() + 8, 2); // Unknown palette flag.
    rejects([&] { decode_tilemap(bad); });
    bad = bytes;
    // Last cell cannot reference a palette entry outside 0..2.
    bad.back() = 0xff;
    rejects([&] { decode_tilemap(bad); });
    bad.assign(4 * 1024 * 1024 + 1, 0);
    rejects([&] { decode_tilemap(bad); });
}
TEST(tilemap_validation) {
    auto m = tiles();
    m.layers[1].order = m.layers[0].order;
    rejects([&] { TileMap map(m); });
    m = tiles();
    m.palette[0].solid = true;
    rejects([&] { encode_tilemap(m); });
    m = tiles();
    m.palette[1].atlas_cell = 2;
    rejects([&] { encode_tilemap(m); });
    m = tiles();
    m.tile_size = std::numeric_limits<float>::quiet_NaN();
    rejects([&] { encode_tilemap(m); });
    m = tiles();
    m.layers[0].cells.pop_back();
    rejects([&] { encode_tilemap(m); });
    m = tiles();
    m.texture = "../escape";
    rejects([&] { encode_tilemap(m); });
    rejects([] { validate_atlas_image(17, 16, 4, 2); });
    validate_atlas_image(16, 16, 4, 2);
    const auto uv = atlas_uv(5, 4, 2);
    NEAR(uv.min.x, 0.25F);
    NEAR(uv.min.y, 0.5F);
    NEAR(uv.max.x, 0.5F);
    NEAR(uv.max.y, 1.0F);
}
TEST(tile_visibility_matches_brute_force) {
    const TileMap map(tiles());
    const auto& m = map.data();
    std::mt19937 random(832);
    std::uniform_real_distribution<float> position(-40, 70), size(0.01F, 35);
    for (int iteration = 0; iteration < 700; ++iteration) {
        const Vec2 p{position(random), position(random)};
        const Rect view{p, p + Vec2{size(random), size(random)}};
        for (std::size_t layer = 0; layer < m.layers.size(); ++layer) {
            std::set<std::pair<float, float>> expected, actual;
            for (std::uint32_t y = 0; y < m.height; ++y)
                for (std::uint32_t x = 0; x < m.width; ++x) {
                    const auto min =
                        m.origin + Vec2{static_cast<float>(x) * 2, static_cast<float>(y) * 2};
                    if (m.layers[layer].cells[y * m.width + x] &&
                        overlaps(view, {min, min + Vec2{2, 2}}))
                        expected.emplace(min.x + 1, min.y + 1);
                }
            const auto stats = map.visit_visible(
                layer, view, [&](TileInstance t) { actual.emplace(t.center.x, t.center.y); });
            CHECK(actual == expected);
            CHECK(stats.tiles == actual.size());
        }
    }
}
TEST(tile_boundaries_and_chunk_culling) {
    TileMap map(tiles(256, 256));
    const auto stats = map.visit_visible(0, {{-20, -10}, {-18, -8}}, [](TileInstance) {});
    CHECK(stats.tiles == 1 && stats.cells == 1 && stats.chunks == 1);
    CHECK(map.chunk_count() == 512);
    CHECK(map.visit_visible(0, {{-22, -12}, {-20, -10}}, [](TileInstance) {}).tiles == 0);
    CHECK(map.visit_visible(0, {{-20, -10}, {-20, 50}}, [](TileInstance) {}).tiles == 0);
    rejects([&] { map.visit_visible(2, map.bounds(), [](TileInstance) {}); });
    rejects([&] { map.visit_visible(0, {{1, 1}, {0, 0}}, [](TileInstance) {}); });
    rejects([&] {
        map.visit_visible(0, {{0, 0}, {std::numeric_limits<float>::infinity(), 1}},
                          [](TileInstance) {});
    });
    auto empty = tiles();
    std::fill(empty.layers[0].cells.begin(), empty.layers[0].cells.end(), 0);
    TileMap sparse(std::move(empty));
    CHECK(sparse.visit_visible(0, sparse.bounds(), [](TileInstance) {}).cells == 0);
}
TEST(tile_decimal_boundaries_and_precision) {
    auto m = tiles(35, 19);
    m.origin = {-1.23F, 3.41F};
    m.tile_size = 0.1F;
    std::fill(m.layers[0].cells.begin(), m.layers[0].cells.end(), 2);
    TileMap map(m);
    std::vector<Rect> all;
    map.append_colliders(map.bounds(), all);
    CHECK(all.size() == 35 * 19);
    for (const auto& cell : all) {
        std::vector<Rect> matches;
        map.append_colliders(cell, matches);
        CHECK(matches.size() == 1);
        CHECK(map.visit_visible(0, cell, [](TileInstance) {}).tiles == 1);
    }
    m.origin.x = 1000000;
    m.tile_size = 0.01F;
    rejects([&] { TileMap invalid(m); });
}
TEST(tile_collision_union_and_swept_query) {
    TileMap map(tiles());
    std::vector<Rect> all, near;
    map.append_colliders(map.bounds(), all);
    CHECK(all.size() == map.solid_cells());
    std::size_t expected = 0;
    for (std::uint32_t y = 0; y < map.data().height; ++y)
        for (std::uint32_t x = 0; x < map.data().width; ++x) {
            const auto i = y * map.data().width + x;
            CHECK(map.solid(x, y) == (i % 3 == 0 || i % 7 == 0));
            if (i % 3 == 0 || i % 7 == 0)
                ++expected;
        }
    CHECK(expected == all.size());
    rejects([&] { map.solid(35, 0); });
    const Vec2 start{-30, -9}, delta{100, 0}, half{0.3F, 0.3F};
    map.append_colliders({start - half, start + delta + half}, near);
    CHECK(near.size() < all.size());
    const auto a = move_box(start, half, delta, all), b = move_box(start, half, delta, near);
    NEAR(a.x, -20.3F);
    NEAR(a.x, b.x);
    NEAR(a.y, b.y);
}
TEST(animation_binary_roundtrip_and_corruption) {
    const auto bytes = encode_animations(*clips());
    CHECK(encode_animations(decode_animations(bytes)) == bytes);
    for (std::size_t i = 0; i < bytes.size(); ++i)
        rejects([&] { decode_animations(std::span(bytes).first(i)); });
    auto bad = bytes;
    bad.push_back(0);
    rejects([&] { decode_animations(bad); });
    for (auto offset : {0U, 4U, 8U, 12U, 16U}) {
        bad = bytes;
        put32(bad, offset, 0xffffffffU);
        rejects([&] { decode_animations(bad); });
    }
    bad = bytes;
    put32(bad, bad.size() - 4, 0);
    rejects([&] { decode_animations(bad); });
    bad = bytes;
    put32(bad, 20 + clips()->texture.size(), 65); // Too many clips.
    rejects([&] { decode_animations(bad); });
    bad = bytes;
    put32(bad, 28 + clips()->texture.size() + clips()->clips[0].name.size(), 2);
    rejects([&] { decode_animations(bad); }); // Unknown loop flag.
    bad.assign(1024 * 1024 + 1, 0);
    rejects([&] { decode_animations(bad); });
    auto set = *clips();
    set.clips[1].name = "loop";
    rejects([&] { encode_animations(set); });
    set = *clips();
    set.clips[0].frames[0].atlas_cell = 8;
    rejects([&] { encode_animations(set); });
    set = *clips();
    set.clips[0].frames[0].ticks = 36001;
    rejects([&] { encode_animations(set); });
    rejects([] { AnimationPlayer player(nullptr); });
}
TEST(animation_boundaries_pause_and_completion) {
    AnimationPlayer player(clips());
    CHECK(player.frame_index() == 0);
    CHECK(player.advance(1).loops == 0);
    CHECK(player.frame_index() == 0);
    player.advance(1);
    CHECK(player.frame_index() == 1);
    player.advance(3);
    CHECK(player.frame_index() == 2);
    CHECK(player.advance(1).loops == 1);
    CHECK(player.frame_index() == 0);
    player.play("once");
    player.advance(2);
    CHECK(player.frame_index() == 1);
    player.pause(true);
    player.advance(1000);
    CHECK(player.frame_index() == 1);
    player.play("once", false);
    CHECK(player.paused());
    player.pause(false);
    player.advance(3);
    CHECK(player.frame_index() == 2);
    CHECK(!player.finished());
    CHECK(player.advance(1).completed);
    CHECK(player.finished());
    CHECK(!player.advance(100).completed);
    CHECK(player.frame_index() == 2);
    player.play("once", false);
    CHECK(player.finished());
    rejects([&] { player.play("missing"); });
    CHECK(player.finished());
    player.play("once");
    CHECK(!player.finished());
    CHECK(player.advance(std::numeric_limits<std::uint64_t>::max()).completed);
    player.play("single");
    CHECK(player.advance(100).loops == 100);
    CHECK(player.frame_index() == 0);
}
TEST(animation_large_delta_matches_single_ticks) {
    AnimationPlayer a(clips()), b(clips());
    std::uint64_t loops = 0;
    for (int i = 0; i < 10001; ++i)
        loops += a.advance(1).loops;
    CHECK(b.advance(10001).loops == loops);
    CHECK(a.frame_index() == b.frame_index());
    const auto max = std::numeric_limits<std::uint64_t>::max();
    b.play("loop");
    b.advance(5);
    CHECK(b.advance(max).loops == max / 6 + 1);
    CHECK(b.frame_index() == 1);
    b.play("single");
    CHECK(b.advance(max).loops == max);
}
} // namespace
int main() {
    return testing::run_tests();
}
