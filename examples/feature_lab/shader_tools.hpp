#pragma once
#include "engine/input.hpp"
#include "engine/renderer.hpp"
#include <iostream>

namespace feature_lab {
// Explicit development commands only; no file polling or compilation on normal frames.
class ShaderTools {
  public:
    const engine::ShaderReloadResult& result() const { return result_; }
    void reload(engine::Renderer& renderer, const engine::AssetRoot& assets) {
        // Read both bounded sources before asking the renderer to build a candidate.
        try {
            const auto vertex =
                assets.text("shaders/sprite.vert", engine::Renderer::max_shader_source_bytes);
            const auto fragment =
                assets.text("shaders/sprite.frag", engine::Renderer::max_shader_source_bytes);
            result_ = renderer.reload_shaders(vertex, fragment);
        } catch (const std::runtime_error& error) {
            result_ = {false, renderer.shader_revision(), error.what()};
        }
        report(assets.directory().string());
    }
    void fail_demo(engine::Renderer& renderer) {
        result_ =
            renderer.reload_shaders(engine::Renderer::default_vertex_shader(),
                                    "#version 460 core\n#error deliberate Feature Lab failure\n");
        report("deliberate failure demo");
    }
    void update(engine::Renderer& renderer, const engine::AssetRoot& assets,
                const engine::InputFrame& input) {
        if (engine::button(input, engine::Key::reload_shaders).pressed)
            reload(renderer, assets); // A real reload takes precedence over the failure demo.
        else if (engine::button(input, engine::Key::shader_error).pressed)
            fail_demo(renderer);
    }
    std::string status() const {
        return std::string(result_.applied ? "SHADER READY R" : "SHADER FAILED - KEPT R") +
               std::to_string(result_.revision) + " / F5 RELOAD / F6 TEST ERROR";
    }

  private:
    void report(const std::string& source) const {
        std::cerr << "[shader " << (result_.applied ? "applied" : "retained")
                  << "] revision=" << result_.revision << " source=" << source << '\n';
        if (!result_.diagnostics.empty())
            std::cerr << result_.diagnostics << '\n';
    }
    engine::ShaderReloadResult result_{true, 1, {}};
};
} // namespace feature_lab
