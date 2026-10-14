#pragma once
#include "engine/assets.hpp"
#include "engine/math.hpp"
#include <array>

namespace engine {
// Atlas rectangles use bottom-left pixels. Offsets use top-left text coordinates
// (x right, y down); advances and line height are unscaled font pixels.
struct FontGlyph {
    std::uint32_t code{}, x{}, y{}, width{}, height{};
    std::int32_t offset_x{}, offset_y{};
    std::uint32_t advance{};
};
struct FontData {
    TextureData atlas;
    std::uint32_t line_height{8}, fallback{'?'};
    std::vector<FontGlyph> glyphs;
    void validate() const;
};
FontData decode_font(std::span<const std::uint8_t> bytes, std::string_view source = "<font>");
std::vector<std::uint8_t> encode_font(const FontData& font);
FontData builtin_font(); // In-house uppercase/digit/punctuation subset, no filesystem dependency.

// Validated immutable snapshot, O(1) byte lookup, no allocations during layout.
class BitmapFont {
  public:
    static constexpr std::size_t max_file_bytes = 4 * 1024 * 1024 + 4096;
    static constexpr std::size_t max_text_bytes = 4096;
    explicit BitmapFont(FontData data);
    const FontData& data() const { return data_; }
    const FontGlyph& glyph(unsigned char byte) const;
    // Logical advance width (including spaces) and line-box height; empty text is zero.
    Vec2 measure(std::string_view text, float scale = 1) const;
    static void validate_text(std::string_view text, float scale);

  private:
    FontData data_;
    std::array<std::uint8_t, 128> lookup_{};
};
BitmapFont load_font(const AssetRoot& root, std::string_view key);
BitmapFont load_font_or_fallback(const AssetRoot& root, std::string_view key, std::string& error);

struct PositionedGlyph {
    Rect bounds; // Relative to the top-left origin, y down. Includes offsets.
    Rect uv;
};
// LF starts a new line; every other unsupported byte produces one fallback glyph.
// Input string and immutable font must outlive this cursor.
class TextCursor {
  public:
    TextCursor(const BitmapFont& font, std::string_view text, float scale = 1);
    bool next(PositionedGlyph& glyph);

  private:
    const BitmapFont& font_;
    std::string_view text_;
    float scale_;
    Vec2 pen_{};
};
} // namespace engine
