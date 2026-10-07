#include "engine/animation.hpp"
#include "engine/assets.hpp"
#include "engine/binary.hpp"
#include <algorithm>
#include <unordered_set>

namespace engine {
void AnimationSet::validate() const {
    AssetRoot::validate_key(texture);
    validate_atlas(atlas_columns, atlas_rows);
    if (clips.empty() || clips.size() > 64)
        throw std::runtime_error("Animation set needs 1..64 clips");
    std::unordered_set<std::string> names;
    std::size_t total = 0;
    for (const auto& clip : clips) {
        if (clip.name.empty() || clip.name.size() > 64 || !names.insert(clip.name).second)
            throw std::runtime_error("Animation clip names must be unique, 1..64 bytes");
        for (char c : clip.name)
            if (c < 33 || c > 126)
                throw std::runtime_error("Clip names must be printable ASCII without spaces");
        if (clip.frames.empty() || clip.frames.size() > 256)
            throw std::runtime_error("Clip needs 1..256 frames");
        total += clip.frames.size();
        if (total > 4096)
            throw std::runtime_error("Animation set exceeds 4096 frames");
        for (auto frame : clip.frames)
            if (frame.atlas_cell >= atlas_columns * atlas_rows || !frame.ticks ||
                frame.ticks > 36000)
                throw std::runtime_error("Invalid animation frame cell/duration");
    }
}
AnimationSet decode_animations(std::span<const std::uint8_t> bytes, std::string_view source) {
    try {
        if (bytes.size() > 1024 * 1024)
            throw std::runtime_error("Animation file exceeds 1 MiB");
        binary::Reader in(bytes, "EANI");
        AnimationSet set;
        set.atlas_columns = in.u32();
        set.atlas_rows = in.u32();
        set.texture = in.string(240);
        const auto clips = in.u32();
        if (!clips || clips > 64)
            in.fail("Invalid clip count");
        std::size_t total = 0;
        for (std::uint32_t i = 0; i < clips; ++i) {
            AnimationClip clip;
            clip.name = in.string(64);
            const auto loop = in.u32(), frames = in.u32();
            if (loop > 1 || !frames || frames > 256)
                in.fail("Invalid animation flags/frame count");
            total += frames;
            if (total > 4096)
                in.fail("Too many animation frames");
            in.require(static_cast<std::size_t>(frames) * 8);
            clip.loop = loop != 0;
            for (std::uint32_t j = 0; j < frames; ++j)
                clip.frames.push_back({in.u32(), in.u32()});
            set.clips.push_back(std::move(clip));
        }
        in.finish();
        set.validate();
        return set;
    } catch (const std::exception& e) {
        throw std::runtime_error(std::string(source) + ": " + e.what());
    }
}
std::vector<std::uint8_t> encode_animations(const AnimationSet& set) {
    set.validate();
    binary::Writer out("EANI");
    out.u32(set.atlas_columns);
    out.u32(set.atlas_rows);
    out.string(set.texture);
    out.u32(static_cast<std::uint32_t>(set.clips.size()));
    for (const auto& clip : set.clips) {
        out.string(clip.name);
        out.u32(clip.loop ? 1U : 0U);
        out.u32(static_cast<std::uint32_t>(clip.frames.size()));
        for (auto frame : clip.frames) {
            out.u32(frame.atlas_cell);
            out.u32(frame.ticks);
        }
    }
    return out.take();
}
AnimationPlayer::AnimationPlayer(std::shared_ptr<const AnimationSet> animations)
    : animations_(std::move(animations)) {
    if (!animations_)
        throw std::invalid_argument("Animation player needs a snapshot");
    animations_->validate();
    play(animations_->clips.front().name);
}
void AnimationPlayer::play(std::string_view name, bool restart) {
    const auto found = std::find_if(animations_->clips.begin(), animations_->clips.end(),
                                    [&](const auto& clip) { return clip.name == name; });
    if (found == animations_->clips.end())
        throw std::invalid_argument("Unknown animation clip: " + std::string(name));
    const auto index = static_cast<std::size_t>(found - animations_->clips.begin());
    if (!restart && duration_ && index == clip_)
        return;
    clip_ = index;
    position_ = 0;
    duration_ = 0;
    paused_ = false;
    for (std::size_t i = 0; i < found->frames.size(); ++i) {
        duration_ += found->frames[i].ticks;
        frame_ends_[i] = duration_;
    }
}
bool AnimationPlayer::finished() const {
    return !animations_->clips[clip_].loop && position_ == duration_;
}
std::size_t AnimationPlayer::frame_index() const {
    const auto last =
        frame_ends_.begin() + static_cast<std::ptrdiff_t>(animations_->clips[clip_].frames.size());
    return static_cast<std::size_t>(
        std::upper_bound(frame_ends_.begin(), last, std::min(position_, duration_ - 1)) -
        frame_ends_.begin());
}
Rect AnimationPlayer::uv() const {
    return atlas_uv(animations_->clips[clip_].frames[frame_index()].atlas_cell,
                    animations_->atlas_columns, animations_->atlas_rows);
}
std::string_view AnimationPlayer::clip_name() const {
    return animations_->clips[clip_].name;
}
AnimationState AnimationPlayer::snapshot() const {
    return {std::string(clip_name()), position_, paused_};
}
void AnimationPlayer::restore(const AnimationState& state) {
    auto candidate = *this;
    candidate.play(state.clip);
    const bool loop = candidate.animations_->clips[candidate.clip_].loop;
    if (state.position > candidate.duration_ || (loop && state.position == candidate.duration_))
        throw std::invalid_argument("Animation position outside clip");
    candidate.position_ = state.position;
    candidate.paused_ = state.paused;
    *this = std::move(candidate);
}
AnimationStep AnimationPlayer::advance(std::uint64_t ticks) {
    if (paused_ || !ticks || finished())
        return {};
    if (animations_->clips[clip_].loop) {
        auto loops = ticks / duration_;
        position_ += ticks % duration_;
        if (position_ >= duration_) {
            position_ -= duration_;
            ++loops;
        }
        return {false, loops};
    }
    position_ += std::min(ticks, duration_ - position_);
    return {finished(), 0};
}
} // namespace engine
