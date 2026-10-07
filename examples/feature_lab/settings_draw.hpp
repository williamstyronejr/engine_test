#pragma once
#include "engine/renderer.hpp"
#include "settings.hpp"

namespace feature_lab {
inline void Settings::draw(engine::Renderer& renderer, int width, int height) const {
    if (!open_)
        return;
    const float w = static_cast<float>(width), h = static_cast<float>(height);
    renderer.set_camera({{w * 0.5F, -h * 0.5F}, h});
    renderer.quad({w * 0.5F, -h * 0.5F}, {w, h}, {0, 0, 0, 0.65F});
    renderer.quad({(panel_.min.x + panel_.max.x) * 0.5F, -(panel_.min.y + panel_.max.y) * 0.5F},
                  panel_.max - panel_.min, {0.025F, 0.05F, 0.075F, 1});
    renderer.text({panel_.min.x + 20 * scale_, -panel_.min.y - 16 * scale_}, 3 * scale_,
                  controls_ ? "KEYBOARD CONTROLS" : "SETTINGS", {0.05F, 0.9F, 0.75F, 1});
    renderer.text({panel_.min.x + 20 * scale_, -panel_.min.y - 48 * scale_}, 1.2F * scale_,
                  !notice.empty()     ? std::string_view(notice)
                  : !vsync_available  ? "VSYNC CONTROL UNAVAILABLE"
                  : !audio_available_ ? "AUDIO UNAVAILABLE"
                                      : "SETTINGS SAVED ON CLOSE",
                  {0.65F, 0.75F, 0.8F, 1});
    engine::draw_ui(renderer, ui_);
    renderer.text({panel_.min.x + 20 * scale_, -panel_.max.y + 26 * scale_}, 1.2F * scale_,
                  controls_ ? "ARROWS / SPACE / FUNCTION KEYS STAY FIXED"
                            : "TAB FOCUS / ARROWS ADJUST / ENTER / ESC CLOSE",
                  {0.65F, 0.75F, 0.8F, 1});
}
} // namespace feature_lab
