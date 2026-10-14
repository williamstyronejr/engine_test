#include "engine/font.hpp"
#include "font_fixture.hpp"
#include "test.hpp"
#include <filesystem>
#include <limits>
#include <unistd.h>

namespace {
using namespace engine;
using namespace testing;
TEST(font_round_trip_and_shipped_asset) {
    const auto source = metric_font();
    const auto encoded = encode_font(source);
    const auto decoded = decode_font(encoded, "metric.efnt");
    CHECK(encode_font(decoded) == encoded);
    const auto shipped = load_font(AssetRoot(TEST_ASSET_ROOT), "fonts/ui.efnt");
    CHECK(shipped.data().glyphs.size() >= 40);
    CHECK(shipped.glyph('I').advance < shipped.glyph('W').advance);
    CHECK(shipped.glyph('?').code == '?');
    const BitmapFont built(builtin_font());
    NEAR(built.measure("HELLO").x, 30);
    CHECK(built.data().atlas.rgba.size() == 16384);
}
TEST(font_parser_rejects_truncation_headers_counts_and_trailing_data) {
    const auto bytes = encode_font(metric_font());
    for (std::size_t i = 0; i < bytes.size(); ++i)
        rejects([&] { decode_font(std::span(bytes).first(i)); });
    for (const auto offset : {0U, 4U, 16U}) {
        auto bad = bytes;
        bad[offset] = 255;
        rejects([&] { decode_font(bad); });
    }
    auto trailing = bytes;
    trailing.push_back(0);
    rejects([&] { decode_font(trailing); });
    std::vector<std::uint8_t> oversized(BitmapFont::max_file_bytes + 1);
    rejects([&] { decode_font(oversized); });
    try {
        decode_font({}, "fonts/broken.efnt");
        CHECK(false);
    } catch (const std::runtime_error& e) {
        CHECK(std::string(e.what()).find("fonts/broken.efnt") != std::string::npos);
    }
}
TEST(font_validation_rejects_invalid_metrics_and_invisible_fallback) {
    const auto invalid = [](auto change) {
        auto font = metric_font();
        change(font);
        rejects([&] { BitmapFont snapshot(font); });
        rejects([&] { encode_font(font); });
    };
    invalid([](auto& f) { f.glyphs.push_back(f.glyphs.back()); });
    invalid([](auto& f) { f.glyphs.back().code = 127; });
    invalid([](auto& f) { f.glyphs.back().code = 0xffffffff; });
    invalid([](auto& f) { f.glyphs.back().x = 0xffffffff; });
    invalid([](auto& f) { f.glyphs.back().width = 2; });
    invalid([](auto& f) { f.glyphs.back().width = 0; });
    invalid([](auto& f) { f.glyphs.back().advance = 0; });
    invalid([](auto& f) { f.glyphs.back().advance = 257; });
    invalid([](auto& f) { f.glyphs.back().offset_y = -257; });
    invalid([](auto& f) { f.glyphs.back().offset_x = 257; });
    invalid([](auto& f) { f.glyphs.front().width = f.glyphs.front().height = 1; });
    invalid([](auto& f) { f.glyphs.erase(f.glyphs.begin()); });
    invalid([](auto& f) { f.fallback = 32; });
    invalid([](auto& f) { f.fallback = 'Z'; });
    invalid([](auto& f) { f.atlas.rgba.back() = 0; });
    invalid([](auto& f) { f.line_height = 0; });
    invalid([](auto& f) { f.line_height = 513; });
    invalid([](auto& f) { f.atlas.rgba.pop_back(); });
    invalid([](auto& f) { f.atlas = {1025, 1, std::vector<std::uint8_t>(1025 * 4, 255)}; });
}
TEST(font_layout_offsets_advances_newlines_and_unsigned_fallback) {
    const BitmapFont font(metric_font());
    const std::string text("A I\nZ\xff\0", 7);
    const auto size = font.measure(text, 2);
    NEAR(size.x, 18);
    NEAR(size.y, 20);
    TextCursor cursor(font, text, 2);
    PositionedGlyph g;
    CHECK(cursor.next(g));
    NEAR(g.bounds.min.x, -2);
    NEAR(g.bounds.min.y, 2);
    NEAR(g.bounds.max.x, 2);
    NEAR(g.bounds.max.y, 6);
    NEAR(g.uv.max.x, 0.5F);
    NEAR(g.uv.max.y, 0.5F);
    CHECK(cursor.next(g)); // Space advances but emits no quad.
    NEAR(g.bounds.min.x, 14);
    NEAR(g.bounds.min.y, -2);
    for (int i = 0; i < 3; ++i) {
        CHECK(cursor.next(g));
        NEAR(g.bounds.min.x, static_cast<float>(i * 4));
        NEAR(g.bounds.min.y, 10);
        NEAR(g.uv.min.x, 0.75F);
        NEAR(g.uv.min.y, 0.75F);
    }
    CHECK(!cursor.next(g));
    for (unsigned int c = 0; c < 256; ++c)
        CHECK(font.glyph(static_cast<unsigned char>(c)).code ==
              (c == 32 || c == 'A' || c == 'I' ? c : '?'));
    NEAR(font.measure("").x, 0);
    NEAR(font.measure("").y, 0);
    NEAR(font.measure("\n").y, 10);
    NEAR(font.measure("A  ").x, 8);
}
TEST(font_layout_limits_and_snapshot_ownership) {
    auto data = metric_font();
    const BitmapFont font(data);
    data.glyphs.clear();
    data.atlas.rgba.clear();
    NEAR(font.measure("AA").x, 8);
    const std::string longest(BitmapFont::max_text_bytes, 'A');
    NEAR(font.measure(longest).x, 16384);
    rejects([&] { font.measure(longest + "A"); });
    for (float size : {0.0F, -1.0F, 1025.0F, std::numeric_limits<float>::infinity(),
                       std::numeric_limits<float>::quiet_NaN()}) {
        rejects([&] { font.measure("", size); });
        rejects([&] { TextCursor cursor(font, "A", size); });
    }
    for (int repeat = 0; repeat < 100; ++repeat) {
        TextCursor cursor(font, longest);
        PositionedGlyph g;
        std::size_t count = 0;
        while (cursor.next(g))
            ++count;
        CHECK(count == longest.size());
    }
}
TEST(font_missing_and_corrupt_assets_have_readable_fallback) {
    const auto path =
        std::filesystem::temp_directory_path() / ("engine-font-" + std::to_string(getpid()));
    struct Cleanup {
        std::filesystem::path path;
        ~Cleanup() {
            std::error_code ec;
            std::filesystem::remove_all(path, ec);
        }
    } cleanup{path};
    std::filesystem::create_directories(path);
    AssetRoot root(path);
    std::string error;
    auto fallback = load_font_or_fallback(root, "missing.efnt", error);
    CHECK(error.find("missing.efnt") != std::string::npos);
    CHECK(fallback.glyph('A').code == 'A');
    root.write_text("bad.efnt", "broken");
    fallback = load_font_or_fallback(root, "bad.efnt", error);
    CHECK(error.find("bad.efnt") != std::string::npos);
    CHECK(fallback.glyph('z').code == '?');
    root.write_atomic("good.efnt", encode_font(metric_font()));
    const auto good = load_font_or_fallback(root, "good.efnt", error);
    CHECK(error.empty());
    NEAR(good.measure("A").x, 4);
    rejects([&] { load_font(root, "../outside.efnt"); });
}
} // namespace
int main() {
    return testing::run_tests();
}
