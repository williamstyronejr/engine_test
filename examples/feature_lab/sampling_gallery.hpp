#pragma once
#include "engine/renderer.hpp"

namespace feature_lab {
// Four tiny independent images make all combinations visible at once.
// Renderer owns GPU lifetimes, as for CollisionVisuals. It must outlive this object.
// No uploads or state changes during draw.
class SamplingGallery {
  public:
    explicit SamplingGallery(engine::Renderer& renderer) : renderer_(renderer) {
        const engine::TextureData image{
            2, 2, {0, 220, 210, 255, 255, 120, 20, 255, 255, 120, 20, 255, 0, 220, 210, 255}};
        try {
            for (std::size_t i = 0; i < textures_.size(); ++i) {
                const auto filter =
                    i % 2 ? engine::TextureFilter::linear : engine::TextureFilter::nearest;
                const auto wrap = i / 2 ? engine::TextureWrap::repeat : engine::TextureWrap::clamp;
                textures_[i] = renderer_.upload(image, {filter, filter, wrap, wrap});
            }
        } catch (...) {
            release();
            throw;
        }
    }
    SamplingGallery(const SamplingGallery&) = delete;
    SamplingGallery& operator=(const SamplingGallery&) = delete;
    void draw() const {
        using namespace engine;
        constexpr std::array<std::string_view, 4> labels{"NEAREST / CLAMP", "LINEAR / CLAMP",
                                                         "NEAREST / REPEAT", "LINEAR / REPEAT"};
        constexpr std::array<Vec2, 4> centers{
            {{-22, -3}, {-19.6F, -3}, {-22, -5.2F}, {-19.6F, -5.2F}}};
        for (std::size_t i = 0; i < textures_.size(); ++i)
            renderer_.sprite(textures_[i], Transform::from(centers[i], 0, {1.8F, 1.5F}), {},
                             {{-0.5F, -0.5F}, {1.5F, 1.5F}});
        // All labels share the built-in atlas and can occupy one batch.
        for (std::size_t i = 0; i < textures_.size(); ++i)
            renderer_.text(centers[i] + Vec2{-0.95F, 1.1F}, 0.02F, labels[i],
                           {0.1F, 0.9F, 0.8F, 1});
    }

  private:
    engine::Renderer& renderer_;
    std::array<engine::TextureHandle, 4> textures_{};
    void release() {
        for (auto texture : textures_)
            if (texture.serial)
                renderer_.release(texture);
    }
};
} // namespace feature_lab
