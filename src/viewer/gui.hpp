#pragma once
#include "viewer/controls.hpp"

namespace pt {

class Window;

// Gui must be destroyed before Window (its backends hold the GLFW window and an active OpenGL context).
class Gui final {
public:
    Gui(Window& window, float ui_scale);
    ~Gui();

    Gui(const Gui&) = delete;
    Gui& operator=(const Gui&) = delete;
    Gui(Gui&&) = delete;
    Gui& operator=(Gui&&) = delete;

    void begin_frame() noexcept;
    void end_frame() noexcept;

    [[nodiscard]] bool wants_keyboard() const noexcept;
    [[nodiscard]] bool wants_mouse() const noexcept;

    [[nodiscard]] ControlChange draw_controls(ViewerControls& controls) noexcept;

private:
    bool controls_visible_{true};
};

} // namespace pt
