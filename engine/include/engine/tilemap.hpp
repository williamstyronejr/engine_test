#pragma once
#include "engine/atlas.hpp"
#include <algorithm>
#include <cstdint>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {
struct TileDefinition {
    std::uint32_t atlas_cell{};
    bool solid{};
};
struct TileLayer {
    std::int32_t order{};
    std::vector<std::uint16_t> cells;
};
struct TileMapData {
    std::uint32_t width{}, height{};
    Vec2 origin{};
    float tile_size{1};
    std::string texture;
    std::uint32_t atlas_columns{1}, atlas_rows{1};
    std::vector<TileDefinition> palette; // ID zero is always empty/non-solid.
    std::vector<TileLayer> layers;       // Strictly increasing order.
    void validate() const;
};
TileMapData decode_tilemap(std::span<const std::uint8_t> bytes,
                           std::string_view source = "<tilemap>");
std::vector<std::uint8_t> encode_tilemap(const TileMapData& map);
struct TileVisitStats {
    std::size_t chunks{}, cells{}, tiles{};
};
struct TileInstance {
    Vec2 center;
    Rect uv;
    std::uint16_t id{};
};
// Immutable after validation. Rendering traverses only visible, occupied 16x16 chunks.
class TileMap {
  public:
    static constexpr std::uint32_t chunk_side = 16;
    explicit TileMap(TileMapData data);
    const TileMapData& data() const { return data_; }
    Rect bounds() const;
    std::size_t solid_cells() const { return solid_count_; }
    std::size_t chunk_count() const { return chunks_.size(); }
    bool solid(std::uint32_t x, std::uint32_t y) const;
    template <class F> TileVisitStats visit_visible(std::size_t layer, Rect view, F&& draw) const {
        if (layer >= data_.layers.size())
            throw std::out_of_range("Invalid tile layer");
        const auto region = range(view);
        TileVisitStats stats;
        if (region.x0 == region.x1 || region.y0 == region.y1)
            return stats;
        const auto& cells = data_.layers[layer].cells;
        for (std::uint32_t cy = region.y0 / chunk_side; cy <= (region.y1 - 1) / chunk_side; ++cy) {
            for (std::uint32_t cx = region.x0 / chunk_side; cx <= (region.x1 - 1) / chunk_side;
                 ++cx) {
                ++stats.chunks;
                if (!chunks_[layer * chunk_columns_ * chunk_rows_ + cy * chunk_columns_ + cx])
                    continue;
                const auto x0 = std::max(region.x0, cx * chunk_side),
                           x1 = std::min(region.x1, (cx + 1) * chunk_side);
                const auto y0 = std::max(region.y0, cy * chunk_side),
                           y1 = std::min(region.y1, (cy + 1) * chunk_side);
                for (auto y = y0; y < y1; ++y)
                    for (auto x = x0; x < x1; ++x) {
                        ++stats.cells;
                        const auto id = cells[static_cast<std::size_t>(y) * data_.width + x];
                        if (!id)
                            continue;
                        ++stats.tiles;
                        draw(TileInstance{
                            {static_cast<float>(
                                 (static_cast<double>(x_edges_[x]) + x_edges_[x + 1]) * 0.5),
                             static_cast<float>(
                                 (static_cast<double>(y_edges_[y]) + y_edges_[y + 1]) * 0.5)},
                            atlas_uv(data_.palette[id].atlas_cell, data_.atlas_columns,
                                     data_.atlas_rows),
                            id});
                    }
            }
        }
        return stats;
    }
    // Appends de-duplicated solid cells overlapping the query; caller reuses/reserves storage.
    void append_colliders(Rect query, std::vector<Rect>& output) const;

  private:
    struct Range {
        std::uint32_t x0{}, y0{}, x1{}, y1{};
    };
    Range range(Rect view) const;
    TileMapData data_;
    std::uint32_t chunk_columns_{}, chunk_rows_{};
    std::vector<std::uint8_t> chunks_, solids_;
    std::vector<float> x_edges_, y_edges_;
    std::size_t solid_count_{};
};
} // namespace engine
