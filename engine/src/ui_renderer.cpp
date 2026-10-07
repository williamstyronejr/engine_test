#include "engine/renderer.hpp"
#include "engine/ui.hpp"
#include <cmath>
#include <string>

namespace engine {
void draw_ui(Renderer& r, const Ui& ui) {
    const auto clip = [&](Rect b) {
        const int x = static_cast<int>(std::floor(b.min.x)),
                  y = static_cast<int>(std::floor(b.min.y));
        r.push_clip({x, y, static_cast<int>(std::ceil(b.max.x)) - x,
                     static_cast<int>(std::ceil(b.max.y)) - y});
    };
    const auto box = [&](Rect b, Color c) {
        r.quad({(b.min.x + b.max.x) * 0.5F, -(b.min.y + b.max.y) * 0.5F}, b.max - b.min, c);
    };
    clip(ui.clip());
    for (const auto& w : ui.widgets()) {
        const auto b = w.bounds;
        const auto size = b.max - b.min;
        clip(b);
        const Color accent =
            w.enabled ? Color{0.04F, 0.85F, 0.7F, 1} : Color{0.16F, 0.22F, 0.25F, 1};
        const Color text = w.enabled ? Color{0.85F, 0.93F, 0.96F, 1} : Color{0.3F, 0.38F, 0.41F, 1};
        box(b, ui.focused() == w.id ? accent : Color{0.08F, 0.15F, 0.2F, 1});
        const float inset =
            std::min(std::max(1.0F, size.y * 0.035F), std::min(size.x, size.y) * 0.2F);
        box({b.min + Vec2{inset, inset}, b.max - Vec2{inset, inset}},
            ui.captured() == w.id  ? Color{0.06F, 0.26F, 0.28F, 1}
            : ui.hovered() == w.id ? Color{0.04F, 0.12F, 0.16F, 1}
                                   : Color{0.018F, 0.04F, 0.06F, 1});
        const float font = size.y / (w.kind == UiKind::slider ? 20.0F : 13.0F);
        r.text({b.min.x + size.x * 0.05F, -b.min.y - size.y * 0.2F}, font, w.label, text);
        if (w.kind == UiKind::slider) {
            const float x0 = b.min.x + size.x * 0.05F, x1 = b.max.x - size.x * 0.05F;
            const float y = b.min.y + size.y * 0.77F;
            box({{x0, y - 1}, {x1, y + 1}}, {0.14F, 0.25F, 0.3F, 1});
            const float x = x0 + (x1 - x0) * w.value;
            box({{x - size.y * 0.07F, y - size.y * 0.11F},
                 {x + size.y * 0.07F, y + size.y * 0.11F}},
                accent);
            r.text({b.max.x - size.x * 0.17F, -b.min.y - size.y * 0.2F}, font,
                   std::to_string(static_cast<int>(std::lround(w.value * 100))) + "%", accent);
        } else if (w.kind == UiKind::checkbox) {
            const float side = size.y * 0.5F, x = b.max.x - size.x * 0.05F - side,
                        y = b.min.y + size.y * 0.25F;
            box({{x, y}, {x + side, y + side}}, accent);
            if (w.value == 0)
                box({{x + inset, y + inset}, {x + side - inset, y + side - inset}},
                    {0.018F, 0.04F, 0.06F, 1});
        }
        r.pop_clip();
    }
    r.pop_clip();
}
} // namespace engine
