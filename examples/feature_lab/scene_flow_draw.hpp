#pragma once
#include "engine/renderer.hpp"
#include "scene_flow.hpp"

namespace feature_lab {
inline void SceneFlow::draw(engine::Renderer& renderer, int width, int height) const {
    if (!title_)
        return;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    renderer.set_camera({{w * 0.5F, -h * 0.5F}, h});
    renderer.quad({w * 0.5F, -h * 0.5F}, {w, h}, {0.008F, 0.018F, 0.03F, 1});
    renderer.quad({w * 0.5F, -h * 0.5F}, panel_.max - panel_.min, {0.025F, 0.05F, 0.075F, 1});
    const auto label = [&](float y, float size, std::string_view text, engine::Color color) {
        renderer.text({panel_.min.x + 32 * scale_, -panel_.min.y - y * scale_}, size * scale_, text,
                      color);
    };
    label(28, 4, "FEATURE LAB", {0.04F, 0.85F, 0.7F, 1});
    label(72, 1.7F, "EXPLORE / COLLECT / ESCAPE", {0.8F, 0.9F, 0.94F, 1});
    label(108, 1.3F,
          notice.empty() ? "CONTINUE READS THE SELECTED SAVE SLOT" : std::string_view(notice),
          notice.empty() ? engine::Color{0.55F, 0.7F, 0.75F, 1} : engine::Color{1, 0.4F, 0.15F, 1});
    engine::draw_ui(renderer, ui_);
    label(522, 1.25F, "TAB / ENTER OR CLICK     ESC QUIT", {0.55F, 0.7F, 0.75F, 1});
}
} // namespace feature_lab
