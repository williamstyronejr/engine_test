#include "engine/font.hpp"
#include "engine/staging_memory.hpp"
#include "engine/binary.hpp"
#include <algorithm>
#include <cmath>

namespace engine {
namespace {
std::array<unsigned char, 7> glyph(char c) {
    switch (c) {
    case '%':
        return {25, 26, 2, 4, 8, 11, 19};
    case '+':
        return {0, 4, 4, 31, 4, 4, 0};
    case 'A':
        return {14, 17, 17, 31, 17, 17, 17};
    case 'B':
        return {30, 17, 17, 30, 17, 17, 30};
    case 'C':
        return {14, 17, 16, 16, 16, 17, 14};
    case 'D':
        return {30, 17, 17, 17, 17, 17, 30};
    case 'E':
        return {31, 16, 16, 30, 16, 16, 31};
    case 'F':
        return {31, 16, 16, 30, 16, 16, 16};
    case 'G':
        return {14, 17, 16, 23, 17, 17, 15};
    case 'H':
        return {17, 17, 17, 31, 17, 17, 17};
    case 'I':
        return {14, 4, 4, 4, 4, 4, 14};
    case 'J':
        return {7, 2, 2, 2, 18, 18, 12};
    case 'K':
        return {17, 18, 20, 24, 20, 18, 17};
    case 'L':
        return {16, 16, 16, 16, 16, 16, 31};
    case 'M':
        return {17, 27, 21, 21, 17, 17, 17};
    case 'N':
        return {17, 25, 25, 21, 19, 19, 17};
    case 'O':
        return {14, 17, 17, 17, 17, 17, 14};
    case 'P':
        return {30, 17, 17, 30, 16, 16, 16};
    case 'Q':
        return {14, 17, 17, 17, 21, 18, 13};
    case 'R':
        return {30, 17, 17, 30, 20, 18, 17};
    case 'S':
        return {15, 16, 16, 14, 1, 1, 30};
    case 'T':
        return {31, 4, 4, 4, 4, 4, 4};
    case 'U':
        return {17, 17, 17, 17, 17, 17, 14};
    case 'V':
        return {17, 17, 17, 17, 17, 10, 4};
    case 'W':
        return {17, 17, 17, 21, 21, 21, 10};
    case 'X':
        return {17, 17, 10, 4, 10, 17, 17};
    case 'Y':
        return {17, 17, 10, 4, 4, 4, 4};
    case 'Z':
        return {31, 1, 2, 4, 8, 16, 31};
    case '0':
        return {14, 17, 19, 21, 25, 17, 14};
    case '1':
        return {4, 12, 4, 4, 4, 4, 14};
    case '2':
        return {14, 17, 1, 2, 4, 8, 31};
    case '3':
        return {30, 1, 1, 14, 1, 1, 30};
    case '4':
        return {2, 6, 10, 18, 31, 2, 2};
    case '5':
        return {31, 16, 16, 30, 1, 1, 30};
    case '6':
        return {14, 16, 16, 30, 17, 17, 14};
    case '7':
        return {31, 1, 2, 4, 8, 8, 8};
    case '8':
        return {14, 17, 17, 14, 17, 17, 14};
    case '9':
        return {14, 17, 17, 15, 1, 1, 14};
    case '-':
        return {0, 0, 0, 31, 0, 0, 0};
    case '/':
        return {1, 1, 2, 4, 8, 16, 16};
    case ':':
        return {0, 4, 4, 0, 4, 4, 0};
    case '.':
        return {0, 0, 0, 0, 0, 4, 4};
    case ' ':
        return {};
    default:
        return {31, 17, 5, 4, 0, 4, 0};
    }
}
} // namespace
void FontData::validate() const {
    atlas.validate();
    if (atlas.width > 1024 || atlas.height > 1024)
        throw std::runtime_error("Font atlas exceeds 1024x1024");
    if (!line_height || line_height > 512 || glyphs.empty() || glyphs.size() > 95)
        throw std::runtime_error("Invalid font line height or glyph count");
    std::array<bool, 128> seen{};
    bool has_fallback = false, has_space = false;
    for (const auto& g : glyphs) {
        if (g.code < 32 || g.code > 126 || seen[g.code])
            throw std::runtime_error("Font glyphs must be unique printable ASCII");
        seen[g.code] = true;
        if (g.width > 256 || g.height > 256 || g.x > atlas.width || g.y > atlas.height ||
            g.width > atlas.width - g.x || g.height > atlas.height - g.y ||
            (g.width == 0) != (g.height == 0) || !g.advance || g.advance > 256 ||
            g.offset_x < -256 || g.offset_x > 256 || g.offset_y < -256 || g.offset_y > 256)
            throw std::runtime_error("Invalid font glyph rectangle, offset or advance");
        if (g.code == 32) {
            if (g.width || g.height)
                throw std::runtime_error("Space must be an advance-only glyph");
            has_space = true;
        }
        if (g.code == fallback && g.width && g.height) {
            // A declared fallback must contain visible coverage, not just an empty rectangle.
            for (std::uint32_t y = g.y; y < g.y + g.height; ++y)
                for (std::uint32_t x = g.x; x < g.x + g.width; ++x)
                    has_fallback |=
                        atlas.rgba[(static_cast<std::size_t>(y) * atlas.width + x) * 4 + 3] != 0;
        }
    }
    if (!has_fallback || !has_space)
        throw std::runtime_error("Font needs a visible fallback glyph and an advance-only space");
}
FontData decode_font(std::span<const std::uint8_t> bytes, std::string_view source) {
    try {
        if (bytes.size() > BitmapFont::max_file_bytes)
            throw std::runtime_error("Font file exceeds size limit");
        binary::Reader in(bytes, "EFNT");
        FontData font;
        font.line_height = in.u32();
        font.fallback = in.u32();
        const auto count = in.u32();
        if (!count || count > 95)
            in.fail("Invalid font glyph count");
        in.require(static_cast<std::size_t>(count) * 32);
        font.glyphs.reserve(count);
        for (std::uint32_t i = 0; i < count; ++i)
            font.glyphs.push_back(
                {in.u32(), in.u32(), in.u32(), in.u32(), in.u32(), in.i32(), in.i32(), in.u32()});
        const auto size = in.u32();
        font.atlas = decode_texture(in.bytes(size));
        in.finish();
        font.validate();
        return font;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(source) + ": " + e.what());
    }
}
std::vector<std::uint8_t> encode_font(const FontData& font) {
    font.validate();
    binary::Writer out("EFNT");
    out.u32(font.line_height);
    out.u32(font.fallback);
    out.u32(static_cast<std::uint32_t>(font.glyphs.size()));
    for (const auto& g : font.glyphs) {
        out.u32(g.code);
        out.u32(g.x);
        out.u32(g.y);
        out.u32(g.width);
        out.u32(g.height);
        out.i32(g.offset_x);
        out.i32(g.offset_y);
        out.u32(g.advance);
    }
    const auto image = encode_texture(font.atlas);
    out.u32(static_cast<std::uint32_t>(image.size()));
    out.bytes(image);
    return out.take();
}
FontData builtin_font() {
    constexpr std::string_view characters = " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789%+-/:.?";
    FontData font;
    // Eight-pixel cells with transparent gutters, bottom-up RGBA atlas storage.
    font.atlas = {128, 32, std::vector<std::uint8_t>(128 * 32 * 4)};
    for (std::size_t i = 0; i < characters.size(); ++i) {
        const auto x = static_cast<std::uint32_t>(i % 16) * 8 + 1;
        const auto y = static_cast<std::uint32_t>(i / 16) * 8;
        const auto code = static_cast<unsigned char>(characters[i]);
        const auto rows = glyph(characters[i]);
        font.glyphs.push_back({code, x, y, code == 32 ? 0U : 5U, code == 32 ? 0U : 7U, 0, 0, 6});
        for (std::uint32_t row = 0; row < 7; ++row)
            for (std::uint32_t column = 0; column < 5; ++column)
                if (rows[row] & (1U << (4 - column))) {
                    const auto offset =
                        (static_cast<std::size_t>(y + 6 - row) * 128 + x + column) * 4;
                    std::fill_n(font.atlas.rgba.begin() + static_cast<std::ptrdiff_t>(offset), 4,
                                255);
                }
    }
    return font;
}
BitmapFont::BitmapFont(FontData data) : data_(std::move(data)) {
    data_.validate();
    const auto fallback = std::find_if(data_.glyphs.begin(), data_.glyphs.end(),
                                       [&](const auto& g) { return g.code == data_.fallback; });
    lookup_.fill(static_cast<std::uint8_t>(fallback - data_.glyphs.begin()));
    for (std::size_t i = 0; i < data_.glyphs.size(); ++i)
        lookup_[data_.glyphs[i].code] = static_cast<std::uint8_t>(i);
}
const FontGlyph& BitmapFont::glyph(unsigned char byte) const {
    return data_.glyphs[lookup_[byte < 128 ? byte : data_.fallback]];
}
void BitmapFont::validate_text(std::string_view text, float scale) {
    if (text.size() > max_text_bytes || !std::isfinite(scale) || scale <= 0 || scale > 1024)
        throw std::invalid_argument("Invalid text length or scale (max 4096 bytes, scale 1024)");
}
Vec2 BitmapFont::measure(std::string_view text, float scale) const {
    validate_text(text, scale);
    if (text.empty())
        return {};
    float width = 0, maximum = 0, height = static_cast<float>(data_.line_height);
    for (char byte : text) {
        const auto c = static_cast<unsigned char>(byte);
        if (c == '\n') {
            maximum = std::max(maximum, width);
            width = 0;
            height += static_cast<float>(data_.line_height);
        } else
            width += static_cast<float>(glyph(c).advance);
    }
    return {std::max(maximum, width) * scale, height * scale};
}
BitmapFont load_font(const AssetRoot& root, std::string_view key) {
    const auto bytes = root.read(key, BitmapFont::max_file_bytes);
    const StagingBytes encoded_memory(bytes.capacity());
    BitmapFont font(decode_font(bytes, key));
    const StagingBytes decoded_memory(font.data().atlas.rgba.capacity() +
                                     font.data().glyphs.capacity() * sizeof(FontGlyph));
    return font;
}
BitmapFont load_font_or_fallback(const AssetRoot& root, std::string_view key, std::string& error) {
    error.clear();
    try {
        return load_font(root, key);
    } catch (const std::exception& e) {
        error = e.what();
    }
    return BitmapFont(builtin_font());
}
TextCursor::TextCursor(const BitmapFont& font, std::string_view text, float scale)
    : font_(font), text_(text), scale_(scale) {
    BitmapFont::validate_text(text, scale);
}
bool TextCursor::next(PositionedGlyph& out) {
    while (!text_.empty()) {
        const auto c = static_cast<unsigned char>(text_.front());
        text_.remove_prefix(1);
        if (c == '\n') {
            pen_.x = 0;
            pen_.y += static_cast<float>(font_.data().line_height);
            continue;
        }
        const auto& g = font_.glyph(c);
        const auto start =
            (pen_ + Vec2{static_cast<float>(g.offset_x), static_cast<float>(g.offset_y)}) * scale_;
        pen_.x += static_cast<float>(g.advance);
        if (!g.width)
            continue;
        out.bounds = {start,
                      start +
                          Vec2{static_cast<float>(g.width), static_cast<float>(g.height)} * scale_};
        const auto w = static_cast<float>(font_.data().atlas.width);
        const auto h = static_cast<float>(font_.data().atlas.height);
        out.uv = {{static_cast<float>(g.x) / w, static_cast<float>(g.y) / h},
                  {static_cast<float>(g.x + g.width) / w, static_cast<float>(g.y + g.height) / h}};
        return true;
    }
    return false;
}
} // namespace engine
