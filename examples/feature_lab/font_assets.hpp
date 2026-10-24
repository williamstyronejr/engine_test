#pragma once
#include "engine/renderer.hpp"
#include "engine/staging_memory.hpp"
#include "hud_text.hpp"
#include <iostream>
#include <limits>

namespace feature_lab {
// Owns reload status for the renderer's current UI font. The renderer owns the
// immutable metrics and atlas; no second live CPU/GPU copy is retained here.
class FontAssets {
  public:
    static constexpr std::string_view key = "fonts/ui.efnt";
    // Between passes, on the owning render thread. Use strict loading on reload:
    // a bad candidate must never replace a good font with the embedded fallback.
    bool reload(engine::Renderer& renderer, const engine::AssetRoot& assets) {
        try {
            if (revision_ == std::numeric_limits<std::uint64_t>::max())
                throw std::runtime_error("Font revision exhausted");
            const auto candidate = engine::load_font(assets, key);
            const engine::StagingBytes candidate_memory(candidate.data().atlas.rgba.capacity() +
                                                        candidate.data().glyphs.capacity() *
                                                            sizeof(engine::FontGlyph));
            renderer.set_font(candidate);
            ++revision_;
            embedded_ = false;
            applied_ = true;
            diagnostic_.clear();
        } catch (const std::runtime_error& error) {
            applied_ = false;
            diagnostic_ = error.what();
        }
        std::cerr << "[font " << (applied_ ? "applied" : "retained") << "] revision=" << revision_
                  << " embedded=" << embedded_ << " atlas_bytes=" << renderer.font_bytes()
                  << " source=" << key << '\n';
        if (!diagnostic_.empty())
            std::cerr << diagnostic_ << '\n';
        return applied_;
    }
    std::uint64_t revision() const { return revision_; }
    bool applied() const { return applied_; }
    bool embedded() const { return embedded_; }
    const std::string& diagnostic() const { return diagnostic_; }
    std::string_view status(std::string& output) const {
        return format_text(output, applied_ ? "FONT READY R" : "FONT FAILED - KEPT R", revision_,
                           embedded_ ? " / EMBEDDED" : " / FILE");
    }
    std::string status() const {
        std::string output;
        status(output);
        return output;
    }

  private:
    std::uint64_t revision_{1}; // Renderer starts with its embedded font.
    bool applied_{true}, embedded_{true};
    std::string diagnostic_;
};
} // namespace feature_lab
