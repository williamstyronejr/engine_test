#pragma once
#include "engine/scene.hpp"
#include <numbers>

namespace feature_lab {
inline bool decoration_motion(std::string_view tag) {
    return tag == "rotor" || tag == "pulse";
}
// Presentation is derived from bounded tick phase, never accumulated or serialized.
// Sampling before composition preserves inherited shear and mirrored local axes.
inline engine::LocalTransform decoration_pose(const engine::SceneNode& node, float phase) {
    auto pose = node.local;
    constexpr float tau = 2 * std::numbers::pi_v<float>;
    if (node.tag == "rotor")
        pose.rotation += tau * phase / 240;
    else if (node.tag == "pulse") {
        const float wave = std::sin(tau * phase / 240);
        pose.scale.x *= 1 + 0.25F * wave;
        pose.scale.y *= 1 - 0.2F * wave;
    }
    return pose;
}
inline void validate_decorations(const engine::Scene& scene) {
    scene.each([&](const engine::SceneNode& node) {
        for (auto p = node.entity; p; p = scene.get(p).parent)
            if (decoration_motion(scene.get(p).tag)) {
                if (node.collider || node.tag == "player" || node.tag == "core" ||
                    node.tag == "exit" || node.tag == "machine")
                    throw std::runtime_error("Animated decoration subtree must be visual only");
                break;
            }
    });
}
} // namespace feature_lab
