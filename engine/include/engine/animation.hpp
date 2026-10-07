#pragma once
#include "engine/atlas.hpp"
#include <array>
#include <cstdint>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace engine {
struct AnimationFrame {
    std::uint32_t atlas_cell{}, ticks{1};
};
struct AnimationClip {
    std::string name;
    bool loop{};
    std::vector<AnimationFrame> frames;
};
struct AnimationSet {
    std::string texture;
    std::uint32_t atlas_columns{1}, atlas_rows{1};
    std::vector<AnimationClip> clips;
    void validate() const;
};
AnimationSet decode_animations(std::span<const std::uint8_t> bytes,
                               std::string_view source = "<animations>");
std::vector<std::uint8_t> encode_animations(const AnimationSet& set);
struct AnimationStep {
    bool completed{};
    std::uint64_t loops{};
};
struct AnimationState {
    std::string clip;
    std::uint64_t position{};
    bool paused{};
    bool operator==(const AnimationState&) const = default;
};
// Owns an immutable clip snapshot. Advance consumes simulation ticks, not render time.
class AnimationPlayer {
  public:
    explicit AnimationPlayer(std::shared_ptr<const AnimationSet> animations);
    void play(std::string_view name, bool restart = true);
    void pause(bool paused) { paused_ = paused; }
    bool paused() const { return paused_; }
    bool finished() const;
    std::size_t frame_index() const;
    Rect uv() const;
    std::string_view clip_name() const;
    AnimationStep advance(std::uint64_t ticks);
    AnimationState snapshot() const;
    void restore(const AnimationState& state); // Validate before replacement; no completion event.

  private:
    std::shared_ptr<const AnimationSet> animations_;
    std::size_t clip_{};
    std::array<std::uint64_t, 256> frame_ends_{};
    std::uint64_t position_{}, duration_{};
    bool paused_{};
};
} // namespace engine
