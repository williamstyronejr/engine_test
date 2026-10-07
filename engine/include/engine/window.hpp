#pragma once
#include "engine/bindings.hpp"
#include <memory>
#include <string_view>

namespace engine {
// Single window/context per process; all methods and destruction on its main thread.
class Window {
  public:
    Window(int width, int height, std::string_view title, bool visible = true);
    ~Window();
    Window(const Window&) = delete;
    Window& operator=(const Window&) = delete;
    bool poll(Input& input);
    // Main thread; validates before publication and suppresses held keys until release.
    void set_bindings(const KeyBindings& bindings, Input& input);
    void present();
    bool set_vsync(bool enabled);
    void toggle_fullscreen();
    void resize(int width, int height);
    int width() const;
    int height() const;
    bool drawable() const;
    void title(std::string_view text);

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace engine
