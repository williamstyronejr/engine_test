#pragma once
#include "engine/input.hpp"
#include <span>
#include <string>
#include <vector>

namespace engine {
using UiId = std::uint32_t;
enum class UiKind { button, checkbox, slider };
struct UiWidget {
    UiId id{};
    UiKind kind{};
    Rect bounds; // Top-left pixels, same coordinates as pointer input.
    std::string label;
    float value{}; // Checkbox 0/1; slider [0,1].
    bool enabled{true};
};
struct UiAction {
    UiId id{};
    float value{};
};
struct UiResult {
    std::array<UiAction, 64> actions{};
    std::size_t count{};
    bool keyboard{}, pointer{};
    std::span<const UiAction> events() const { return {actions.data(), count}; }
};
// Retained, bounded widget state. No renderer/window dependency or update allocations.
class Ui {
  public:
    void layout(Rect clip,
                std::span<const UiWidget> widgets); // Validated replacement; cancels drags.
    void set_value(UiId id, float value);
    void reset_interaction();
    UiResult update(const InputFrame& input, bool modal = true);
    std::span<const UiWidget> widgets() const { return widgets_; }
    Rect clip() const { return clip_; }
    UiId focused() const { return focus_; }
    UiId hovered() const { return hover_; }
    UiId captured() const { return capture_; }

  private:
    UiWidget* find(UiId id);
    UiId hit(Vec2 point) const;
    void navigate(int direction);
    Rect clip_{};
    std::vector<UiWidget> widgets_;
    UiId focus_{}, hover_{}, capture_{};
};
// Simple vertical layout with explicit bounds; overflow is an authoring error.
class UiColumn {
  public:
    UiColumn(Rect bounds, float gap);
    Rect next(float height);

  private:
    Rect bounds_;
    float cursor_{}, gap_{};
};
class Renderer;
// Caller uses a top-left pixel camera (x right, negative world y down).
void draw_ui(Renderer& renderer, const Ui& ui);
} // namespace engine
