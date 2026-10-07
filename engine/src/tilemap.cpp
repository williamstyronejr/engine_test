#include "engine/tilemap.hpp"
#include "engine/assets.hpp"
#include "engine/binary.hpp"
#include <cmath>
#include <limits>

namespace engine {
namespace {
std::size_t cell_count(std::uint32_t width, std::uint32_t height, std::size_t layers) {
    if (!width || !height || width > 1024 || height > 1024 || !layers || layers > 8)
        throw std::runtime_error("Tilemap dimensions/layers out of bounds");
    const auto count = static_cast<std::size_t>(width) * height;
    if (count * layers > 1024 * 1024)
        throw std::runtime_error("Tilemap exceeds 1048576 total cells");
    return count;
}
} // namespace
void TileMapData::validate() const {
    const auto count = cell_count(width, height, layers.size());
    AssetRoot::validate_key(texture);
    validate_atlas(atlas_columns, atlas_rows);
    if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || std::abs(origin.x) > 1e6F ||
        std::abs(origin.y) > 1e6F || !std::isfinite(tile_size) || tile_size < 0.01F ||
        tile_size > 1000)
        throw std::runtime_error("Invalid tilemap origin/size");
    const double largest = std::max(
        {1.0, std::abs(static_cast<double>(origin.x)), std::abs(static_cast<double>(origin.y)),
         std::abs(static_cast<double>(origin.x) + width * static_cast<double>(tile_size)),
         std::abs(static_cast<double>(origin.y) + height * static_cast<double>(tile_size))});
    if (tile_size < 8 * std::numeric_limits<float>::epsilon() * largest)
        throw std::runtime_error("Tile size too small for world-coordinate precision");
    if (palette.empty() || palette.size() > 256 || palette[0].solid || palette[0].atlas_cell != 0)
        throw std::runtime_error("Palette needs 1..256 entries with empty entry zero");
    for (const auto& tile : palette)
        if (tile.atlas_cell >= atlas_columns * atlas_rows)
            throw std::runtime_error("Tile atlas cell outside grid");
    for (std::size_t i = 0; i < layers.size(); ++i) {
        const auto& layer = layers[i];
        if (layer.order < -100000 || layer.order > 100000 ||
            (i && layers[i - 1].order >= layer.order))
            throw std::runtime_error(
                "Tile layers need unique increasing order in [-100000,100000]");
        if (layer.cells.size() != count)
            throw std::runtime_error("Tilemap layer cell count mismatch");
        for (auto id : layer.cells)
            if (id >= palette.size())
                throw std::runtime_error("Unknown tile palette ID");
    }
}
TileMapData decode_tilemap(std::span<const std::uint8_t> bytes, std::string_view source) {
    try {
        if (bytes.size() > 4 * 1024 * 1024)
            throw std::runtime_error("Tilemap exceeds 4 MiB");
        binary::Reader in(bytes, "ETMP");
        TileMapData map;
        map.width = in.u32();
        map.height = in.u32();
        const auto layers = in.u32();
        const auto count = cell_count(map.width, map.height, layers);
        map.origin = {in.f32(), in.f32()};
        map.tile_size = in.f32();
        map.atlas_columns = in.u32();
        map.atlas_rows = in.u32();
        map.texture = in.string(240);
        const auto palette_count = in.u32();
        if (!palette_count || palette_count > 256)
            in.fail("Invalid palette count");
        for (std::uint32_t i = 0; i < palette_count; ++i) {
            const auto cell = in.u32(), flags = in.u32();
            if (flags > 1)
                in.fail("Unknown tile flags");
            map.palette.push_back({cell, flags != 0});
        }
        in.require(static_cast<std::size_t>(layers) * (4 + count * 2));
        for (std::uint32_t i = 0; i < layers; ++i) {
            TileLayer layer;
            layer.order = in.i32();
            layer.cells.reserve(count);
            for (std::size_t j = 0; j < count; ++j)
                layer.cells.push_back(in.u16());
            map.layers.push_back(std::move(layer));
        }
        in.finish();
        map.validate();
        return map;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(source) + ": " + e.what());
    }
}
std::vector<std::uint8_t> encode_tilemap(const TileMapData& map) {
    map.validate();
    binary::Writer out("ETMP");
    out.u32(map.width);
    out.u32(map.height);
    out.u32(static_cast<std::uint32_t>(map.layers.size()));
    out.f32(map.origin.x);
    out.f32(map.origin.y);
    out.f32(map.tile_size);
    out.u32(map.atlas_columns);
    out.u32(map.atlas_rows);
    out.string(map.texture);
    out.u32(static_cast<std::uint32_t>(map.palette.size()));
    for (auto tile : map.palette) {
        out.u32(tile.atlas_cell);
        out.u32(tile.solid ? 1U : 0U);
    }
    for (const auto& layer : map.layers) {
        out.i32(layer.order);
        for (auto id : layer.cells)
            out.u16(id);
    }
    return out.take();
}
TileMap::TileMap(TileMapData data) : data_(std::move(data)) {
    data_.validate();
    x_edges_.reserve(data_.width + 1);
    y_edges_.reserve(data_.height + 1);
    for (std::uint32_t x = 0; x <= data_.width; ++x)
        x_edges_.push_back(static_cast<float>(static_cast<double>(data_.origin.x) +
                                              x * static_cast<double>(data_.tile_size)));
    for (std::uint32_t y = 0; y <= data_.height; ++y)
        y_edges_.push_back(static_cast<float>(static_cast<double>(data_.origin.y) +
                                              y * static_cast<double>(data_.tile_size)));
    chunk_columns_ = (data_.width + chunk_side - 1) / chunk_side;
    chunk_rows_ = (data_.height + chunk_side - 1) / chunk_side;
    chunks_.resize(data_.layers.size() * chunk_columns_ * chunk_rows_);
    solids_.resize(static_cast<std::size_t>(data_.width) * data_.height);
    for (std::size_t layer = 0; layer < data_.layers.size(); ++layer)
        for (std::uint32_t y = 0; y < data_.height; ++y)
            for (std::uint32_t x = 0; x < data_.width; ++x) {
                const auto index = static_cast<std::size_t>(y) * data_.width + x;
                const auto id = data_.layers[layer].cells[index];
                if (!id)
                    continue;
                chunks_[layer * chunk_columns_ * chunk_rows_ + (y / chunk_side) * chunk_columns_ +
                        x / chunk_side] = 1;
                if (data_.palette[id].solid)
                    solids_[index] = 1;
            }
    solid_count_ = static_cast<std::size_t>(std::count(solids_.begin(), solids_.end(), 1));
}
Rect TileMap::bounds() const {
    return {{x_edges_.front(), y_edges_.front()}, {x_edges_.back(), y_edges_.back()}};
}
TileMap::Range TileMap::range(Rect view) const {
    for (float v : {view.min.x, view.min.y, view.max.x, view.max.y})
        if (!std::isfinite(v))
            throw std::invalid_argument("Nonfinite tile query");
    if (view.min.x > view.max.x || view.min.y > view.max.y)
        throw std::invalid_argument("Inverted tile query");
    if (view.min.x == view.max.x || view.min.y == view.max.y || !overlaps(view, bounds()))
        return {};
    // Search the same rounded boundaries used by collision geometry. Dividing by
    // tile_size alone can skip cells at decimal-size boundaries due to float rounding.
    const auto lower = [](const std::vector<float>& edges, float p) {
        const auto index = std::upper_bound(edges.begin(), edges.end(), p) - edges.begin() - 1;
        return static_cast<std::uint32_t>(
            std::clamp(index, std::ptrdiff_t{0}, static_cast<std::ptrdiff_t>(edges.size() - 1)));
    };
    const auto upper = [](const std::vector<float>& edges, float p) {
        return static_cast<std::uint32_t>(
            std::min(std::lower_bound(edges.begin(), edges.end(), p) - edges.begin(),
                     static_cast<std::ptrdiff_t>(edges.size() - 1)));
    };
    return {lower(x_edges_, view.min.x), lower(y_edges_, view.min.y), upper(x_edges_, view.max.x),
            upper(y_edges_, view.max.y)};
}
bool TileMap::solid(std::uint32_t x, std::uint32_t y) const {
    if (x >= data_.width || y >= data_.height)
        throw std::out_of_range("Tile coordinates outside map");
    return solids_[static_cast<std::size_t>(y) * data_.width + x] != 0;
}
void TileMap::append_colliders(Rect query, std::vector<Rect>& output) const {
    const auto region = range(query);
    for (auto y = region.y0; y < region.y1; ++y)
        for (auto x = region.x0; x < region.x1; ++x)
            if (solid(x, y)) {
                output.push_back({{x_edges_[x], y_edges_[y]}, {x_edges_[x + 1], y_edges_[y + 1]}});
            }
}
} // namespace engine
