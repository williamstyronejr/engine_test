#include "engine/ui.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace engine {
namespace {
void validate(Rect r) {
    for (float f : {r.min.x, r.min.y, r.max.x, r.max.y})
        if (!std::isfinite(f) || std::abs(f) > 1000000)
            throw std::invalid_argument("Invalid UI rectangle");
    if (r.max.x <= r.min.x || r.max.y <= r.min.y)
        throw std::invalid_argument("Empty UI rectangle");
}
bool contains(Rect r, Vec2 p) {
    return p.x >= r.min.x && p.x < r.max.x && p.y >= r.min.y && p.y < r.max.y;
}
void validate_value(UiKind kind, float value) {
    if (!std::isfinite(value) || value < 0 || value > 1 ||
        (kind == UiKind::checkbox && value != 0 && value != 1))
        throw std::invalid_argument("Invalid widget value");
}
} // namespace
void Ui::layout(Rect clip, std::span<const UiWidget> widgets) {
    validate(clip);
    if (widgets.size() > 64)
        throw std::invalid_argument("UI widget limit is 64");
    for (std::size_t i = 0; i < widgets.size(); ++i) {
        const auto& w = widgets[i];
        validate(w.bounds);
        validate_value(w.kind, w.value);
        if (!w.id || w.kind < UiKind::button || w.kind > UiKind::slider || w.label.size() > 80)
            throw std::invalid_argument("Invalid widget ID/type/label");
        for (char c : w.label)
            if (c < 32 || c > 126)
                throw std::invalid_argument("UI labels must be printable ASCII");
        for (std::size_t j = 0; j < i; ++j)
            if (widgets[j].id == w.id)
                throw std::invalid_argument("Duplicate UI ID");
    }
    std::vector<UiWidget> candidate(widgets.begin(), widgets.end());
    widgets_.swap(candidate);
    clip_ = clip;
    capture_ = hover_ = 0;
    const auto* focused = find(focus_);
    if (!focused || !focused->enabled || !overlaps(focused->bounds, clip_)) {
        focus_ = 0;
        navigate(1);
    }
}
UiWidget* Ui::find(UiId id) {
    for (auto& w : widgets_)
        if (w.id == id)
            return &w;
    return nullptr;
}
UiId Ui::hit(Vec2 point) const {
    if (!contains(clip_, point))
        return 0;
    for (auto i = widgets_.rbegin(); i != widgets_.rend(); ++i)
        if (contains(i->bounds, point))
            return i->id;
    return 0;
}
void Ui::navigate(int direction) {
    if (widgets_.empty()) {
        focus_ = 0;
        return;
    }
    int index = direction > 0 ? -1 : 0;
    for (std::size_t i = 0; i < widgets_.size(); ++i)
        if (widgets_[i].id == focus_)
            index = static_cast<int>(i);
    const auto size = static_cast<int>(widgets_.size());
    for (int n = 0; n < size; ++n) {
        index = (index + direction + size) % size;
        const auto& w = widgets_[static_cast<std::size_t>(index)];
        if (w.enabled && overlaps(w.bounds, clip_)) {
            focus_ = w.id;
            return;
        }
    }
    focus_ = 0;
}
void Ui::set_value(UiId id, float value) {
    auto* w = find(id);
    if (!w)
        throw std::invalid_argument("Unknown widget ID");
    validate_value(w->kind, value);
    w->value = value;
}
void Ui::reset_interaction() {
    focus_ = hover_ = capture_ = 0;
    navigate(1);
}
UiResult Ui::update(const InputFrame& input, bool modal) {
    for (auto p :
         {input.pointer.position, input.pointer.press_position, input.pointer.release_position})
        if (!std::isfinite(p.x) || !std::isfinite(p.y))
            throw std::invalid_argument("Nonfinite UI pointer");
    UiResult result{};
    result.keyboard = modal;
    result.pointer = modal;
    if (input.canceled) {
        capture_ = hover_ = 0;
        return result;
    }
    const auto emit = [&](UiWidget& w, float value) {
        w.value = value;
        for (std::size_t i = 0; i < result.count; ++i)
            if (result.actions[i].id == w.id) {
                result.actions[i].value = value;
                return;
            }
        result.actions[result.count++] = {w.id, value};
    };
    const auto activate = [&](UiWidget& w) {
        emit(w, w.kind == UiKind::checkbox ? 1 - w.value : w.value);
    };
    const auto drag = [&](UiWidget& w, Vec2 p) {
        const float width = w.bounds.max.x - w.bounds.min.x;
        const float value = std::clamp(
            (p.x - (w.bounds.min.x + w.bounds.max.x) * 0.5F) / (width * 0.9F) + 0.5F, 0.0F, 1.0F);
        if (value != w.value)
            emit(w, value);
    };
    hover_ = input.pointer.inside ? hit(input.pointer.position) : 0;
    if (button(input, Key::tab).pressed) {
        capture_ = 0;
        navigate(button(input, Key::shift).held ? -1 : 1);
        result.keyboard = true;
    } else if (button(input, Key::down).pressed || button(input, Key::up).pressed) {
        capture_ = 0;
        navigate(button(input, Key::down).pressed ? 1 : -1);
        result.keyboard = true;
    }
    if (input.pointer.primary.pressed) {
        capture_ = 0;
        auto* w = find(hit(input.pointer.press_position));
        if (w)
            result.pointer = true;
        if (w && w->enabled) {
            capture_ = focus_ = w->id;
            if (w->kind == UiKind::slider)
                drag(*w, input.pointer.press_position);
        }
    }
    if (auto* w = find(capture_)) {
        result.pointer = true;
        if (w->kind == UiKind::slider &&
            (input.pointer.primary.held || input.pointer.primary.released))
            drag(*w, input.pointer.primary.released ? input.pointer.release_position
                                                    : input.pointer.position);
        if (input.pointer.primary.released) {
            if (w->kind != UiKind::slider && hit(input.pointer.release_position) == w->id)
                activate(*w);
            capture_ = 0;
        } else if (!input.pointer.primary.held)
            capture_ = 0;
    }
    if (!input.pointer.primary.pressed && !input.pointer.primary.released && !capture_) {
        if (auto* w = find(focus_); w && w->enabled) {
            if (w->kind != UiKind::slider &&
                (button(input, Key::accept).pressed || button(input, Key::space).pressed)) {
                activate(*w);
                result.keyboard = true;
            }
            if (w->kind == UiKind::slider &&
                (button(input, Key::left).pressed || button(input, Key::right).pressed)) {
                const float value = std::clamp(
                    w->value + (button(input, Key::right).pressed ? 0.05F : -0.05F), 0.0F, 1.0F);
                if (value != w->value)
                    emit(*w, value);
                result.keyboard = true;
            }
        }
        if (auto* w = find(hover_);
            w && w->enabled && w->kind == UiKind::slider && input.pointer.wheel) {
            const float value =
                std::clamp(w->value + static_cast<float>(input.pointer.wheel) * 0.05F, 0.0F, 1.0F);
            if (value != w->value)
                emit(*w, value);
            result.pointer = true;
        }
    }
    return result;
}
UiColumn::UiColumn(Rect bounds, float gap) : bounds_(bounds), cursor_(bounds.min.y), gap_(gap) {
    validate(bounds);
    if (!std::isfinite(gap) || gap < 0)
        throw std::invalid_argument("Invalid UI gap");
}
Rect UiColumn::next(float height) {
    if (!std::isfinite(height) || height <= 0 || cursor_ + height > bounds_.max.y)
        throw std::invalid_argument("UI column overflow/height");
    Rect result{{bounds_.min.x, cursor_}, {bounds_.max.x, cursor_ + height}};
    cursor_ += height + gap_;
    return result;
}
} // namespace engine
