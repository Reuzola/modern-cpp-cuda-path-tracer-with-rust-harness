#pragma once
#include "pt/math/vec3.hpp"
#include "viewer/controls.hpp"

namespace pt {

class Window;

// clang-format off
enum class ViewerKey { f1, f2, f3, r };
// clang-format on

// Read-only snapshot for the overlay; the Gui does not know where these come from.
struct HudStats {
    int sample_count{};
    int target_spp{};
    double accumulated_seconds{};
    Point3 camera_position{};
};

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
    [[nodiscard]] bool key_pressed(ViewerKey key) const noexcept;

    [[nodiscard]] ControlChange draw_controls(ViewerControls& controls) noexcept;
    void draw_hud(const HudStats& stats) const noexcept;

private:
    bool controls_visible_{true};
};

} // namespace pt
