#pragma once
#include <memory>
#include <string_view>
#include <utility>

struct GLFWwindow;

namespace pt {

struct WindowDeleter {
    void operator()(GLFWwindow* w) const noexcept;
};

class GlfwLibrary {
public:
    GlfwLibrary();
    ~GlfwLibrary();

    GlfwLibrary(const GlfwLibrary&) = delete;
    GlfwLibrary& operator=(const GlfwLibrary&) = delete;
    GlfwLibrary(GlfwLibrary&&) = delete;
    GlfwLibrary& operator=(GlfwLibrary&&) = delete;
};

// clang-format off
enum class Key { w, a, s, d, q, e, left_shift, left_control };
enum class MouseButton { right };
enum class CursorMode { normal, hidden };
// clang-format on

class Window final {
public:
    Window(int width, int height, std::string_view title);

    [[nodiscard]] bool should_close() const noexcept;

    void poll_events() noexcept;

    void swap_buffers() noexcept;

    [[nodiscard]] std::pair<int, int> framebuffer_size() const noexcept;

    [[nodiscard]] bool is_key_down(Key key) const noexcept;

    [[nodiscard]] bool is_mouse_button_down(MouseButton button) const noexcept;

    std::pair<double, double> cursor_delta() noexcept;

    void set_cursor_mode(CursorMode mode) noexcept;

private:
    GlfwLibrary library_;
    std::unique_ptr<GLFWwindow, WindowDeleter> handle_;
    double cursor_x_{};
    double cursor_y_{};
    bool cursor_valid_{}; // False means the next cursor_delta() seeds the position and reports no motion.
};

} // namespace pt
