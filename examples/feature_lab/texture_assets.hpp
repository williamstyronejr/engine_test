#pragma once
#include "engine/assets.hpp"
#include "engine/atlas.hpp"
#include "engine/input.hpp"
#include "engine/renderer.hpp"
#include <algorithm>
#include <iostream>
#include <limits>
#include <memory>
#include <numeric>
#include <string>
#include <unordered_map>
#include <vector>

namespace feature_lab {
struct ResidentTexture {
    std::shared_ptr<const engine::TextureData> data;
    engine::TextureHandle gpu;
    std::uint32_t columns{1}, rows{1};
    bool fallback{};
};
// Renderer owns GPU lifetimes. This registry coordinates its CPU snapshots and cache
// with a single GPU transaction; all callers of a shared handle see the new image.
class TextureAssets {
  public:
    explicit TextureAssets(engine::AssetRoot assets)
        : assets_(std::move(assets)), cache_(std::make_unique<engine::TextureCache>(assets_)) {}
    const ResidentTexture& at(const std::string& key) const { return entries_.at(key); }
    std::uint64_t revision() const { return revision_; }
    bool applied() const { return applied_; }
    const std::string& diagnostic() const { return diagnostic_; }
    std::size_t size() const { return entries_.size(); }
    void load(engine::Renderer& renderer, const std::string& key, std::uint32_t columns,
              std::uint32_t rows, bool force_missing = false) {
        using namespace engine;
        validate_atlas(columns, rows);
        if (const auto found = entries_.find(key); found != entries_.end()) {
            auto& entry = found->second;
            const auto c = std::lcm(entry.columns, columns), r = std::lcm(entry.rows, rows);
            if (c > 4096 || r > 4096)
                throw std::invalid_argument("Incompatible shared atlas grids");
            if (!entry.fallback)
                validate_image(*entry.data, c, r);
            entry.columns = c;
            entry.rows = r;
            return;
        }
        std::string error;
        auto data = cache_->load_or_fallback(force_missing ? "textures/missing.etex" : key, error);
        if (error.empty())
            validate_atlas_image(data->width, data->height, columns, rows);
        else
            std::cerr << "[asset fallback] " << error << '\n';
        const auto handle = renderer.upload(*data);
        try {
            entries_.emplace(
                key, ResidentTexture{std::move(data), handle, columns, rows, !error.empty()});
        } catch (...) {
            renderer.release(handle);
            throw;
        }
    }
    bool reload(engine::Renderer& renderer, bool fail_demo = false) {
        using namespace engine;
        struct Staged {
            ResidentTexture* resident;
            std::shared_ptr<const TextureData> data;
        };
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Texture revision exhausted");
            auto candidate_cache = std::make_unique<TextureCache>(assets_);
            std::vector<std::string> keys;
            keys.reserve(entries_.size());
            for (const auto& [key, entry] : entries_) {
                (void)entry;
                keys.push_back(key);
            }
            std::sort(keys.begin(), keys.end());
            std::vector<Staged> staged;
            std::vector<TextureReplacement> replacements;
            staged.reserve(keys.size());
            replacements.reserve(keys.size());
            std::size_t bytes = 0;
            for (const auto& key : keys) {
                auto& resident = entries_.at(key);
                auto data = candidate_cache->load(key);
                try {
                    validate_image(*data, resident.columns, resident.rows);
                    if (data->rgba.size() > Renderer::max_texture_reload_bytes - bytes)
                        throw std::runtime_error("Texture reload exceeds 64 MiB staging budget");
                } catch (const std::exception& error) {
                    throw std::runtime_error(key + ": " + error.what());
                }
                bytes += data->rgba.size();
                replacements.push_back({resident.gpu, *data});
                staged.push_back({&resident, std::move(data)});
            }
            const TextureData invalid{1, 1, {}};
            if (fail_demo) {
                if (replacements.empty())
                    throw std::runtime_error("No resident textures for failure demo");
                const auto handle = replacements.back().handle;
                replacements.pop_back();
                replacements.push_back({handle, invalid});
            }
            renderer.replace_textures(replacements);
            // Every throwing operation is complete. Publish CPU/cache state with noexcept swaps.
            for (auto& item : staged) {
                item.resident->data.swap(item.data);
                item.resident->fallback = false;
            }
            cache_.swap(candidate_cache);
            ++revision_;
            applied_ = true;
            diagnostic_.clear();
        } catch (const std::exception& error) {
            applied_ = false;
            diagnostic_ = error.what();
        }
        std::cerr << "[textures " << (applied_ ? "applied" : "retained")
                  << "] revision=" << revision_ << " count=" << entries_.size() << '\n';
        if (!diagnostic_.empty())
            std::cerr << diagnostic_ << '\n';
        return applied_;
    }
    void update(engine::Renderer& renderer, const engine::InputFrame& input) {
        if (engine::button(input, engine::Key::reload_textures).pressed)
            reload(renderer);
        else if (engine::button(input, engine::Key::texture_error).pressed)
            reload(renderer, true);
    }
    std::string status() const {
        return std::string(applied_ ? "TEXTURES READY R" : "TEXTURES FAILED - KEPT R") +
               std::to_string(revision_) + " / F7 RELOAD / F8 TEST ERROR";
    }

  private:
    static void validate_image(const engine::TextureData& data, std::uint32_t columns,
                               std::uint32_t rows) {
        // Combined grids may exceed the per-consumer limit of 256 cells per axis.
        if (!data.width || !data.height || data.width % columns || data.height % rows)
            throw std::invalid_argument("Texture dimensions must fit every registered atlas grid");
    }
    engine::AssetRoot assets_;
    std::unique_ptr<engine::TextureCache> cache_;
    std::unordered_map<std::string, ResidentTexture> entries_;
    std::uint64_t revision_{1};
    bool applied_{true};
    std::string diagnostic_;
};
} // namespace feature_lab
