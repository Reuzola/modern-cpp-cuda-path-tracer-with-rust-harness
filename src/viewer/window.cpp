// clang-format changes the order of includes but <glad/glad.h> must be included before <GLFW/glfw3.h>.
// clang-format off
#include "viewer/window.hpp"
#include "pt/util/log.hpp"
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
// clang-format on

namespace pt {

namespace {

void glfw_error_callback(int error_code, const char* description) noexcept {
    pt::log_error("GLFW Error ({}): {}", error_code, description);
}

[[nodiscard]] int to_glfw(Key key) noexcept {
    // No default case: adding an enumerator must fail the -Wswitch build.
    switch (key) {
    case Key::w: return GLFW_KEY_W;
    case Key::a: return GLFW_KEY_A;
    case Key::s: return GLFW_KEY_S;
    case Key::d: return GLFW_KEY_D;
    case Key::q: return GLFW_KEY_Q;
    case Key::e: return GLFW_KEY_E;
    case Key::left_shift: return GLFW_KEY_LEFT_SHIFT;
    case Key::left_control: return GLFW_KEY_LEFT_CONTROL;
    }
    return GLFW_KEY_UNKNOWN;
}

[[nodiscard]] int to_glfw(MouseButton button) noexcept {
    // No default case: adding an enumerator must fail the -Wswitch build.
    switch (button) {
    case MouseButton::right: return GLFW_MOUSE_BUTTON_RIGHT;
    }
    return -1;
}

} // namespace

void WindowDeleter::operator()(GLFWwindow* w) const noexcept {
    glfwDestroyWindow(w);
}

GlfwLibrary::GlfwLibrary() {
    glfwSetErrorCallback(glfw_error_callback);
    if (glfwInit() != GLFW_TRUE) throw std::runtime_error("Failed to initialize GLFW");
}

GlfwLibrary::~GlfwLibrary() { glfwTerminate(); }

Window::Window(int width, int height, std::string_view title) {
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    handle_.reset(glfwCreateWindow(width, height, std::string(title).c_str(), nullptr, nullptr));
    if (handle_ == nullptr) throw std::runtime_error("Failed to create window");

    glfwMakeContextCurrent(handle_.get());

    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress)))
        throw std::runtime_error("Failed to load GLAD");

    glfwSwapInterval(1);

    const char* version = reinterpret_cast<const char*>(glGetString(GL_VERSION));
    const char* renderer = reinterpret_cast<const char*>(glGetString(GL_RENDERER));

    pt::log_info("GL Version: {}", version != nullptr ? version : "unknown");
    pt::log_info("GL Renderer: {}", renderer != nullptr ? renderer : "unknown");
}

bool Window::should_close() const noexcept { return glfwWindowShouldClose(handle_.get()); }

void Window::poll_events() noexcept {
    glfwPollEvents();

    // Window lifecycle, not a runtime control
    if (glfwGetKey(handle_.get(), GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(handle_.get(), GLFW_TRUE);
}

void Window::swap_buffers() noexcept { glfwSwapBuffers(handle_.get()); }

std::pair<int, int> Window::framebuffer_size() const noexcept {
    int width{};
    int height{};
    glfwGetFramebufferSize(handle_.get(), &width, &height);
    return {width, height};
}

bool Window::is_key_down(Key key) const noexcept {
    return glfwGetKey(handle_.get(), to_glfw(key)) == GLFW_PRESS;
}

bool Window::is_mouse_button_down(MouseButton button) const noexcept {
    return glfwGetMouseButton(handle_.get(), to_glfw(button)) == GLFW_PRESS;
}

std::pair<double, double> Window::cursor_delta() noexcept {
    double x{};
    double y{};
    glfwGetCursorPos(handle_.get(), &x, &y);

    if (!cursor_valid_) {
        cursor_x_ = x;
        cursor_y_ = y;
        cursor_valid_ = true;
        return {0.0, 0.0};
    }

    const double dx = x - cursor_x_;
    const double dy = y - cursor_y_;
    cursor_x_ = x;
    cursor_y_ = y;
    return {dx, dy};
}

void Window::set_cursor_mode(CursorMode mode) noexcept {
    // Deliberately not GLFW_CURSOR_DISABLED: it warps the pointer to the window
    // centre, which XWayland does not apply, so the same offset is reported every poll.
    glfwSetInputMode(handle_.get(), GLFW_CURSOR, mode == CursorMode::hidden ? GLFW_CURSOR_HIDDEN : GLFW_CURSOR_NORMAL);
    cursor_valid_ = false;
}

} // namespace pt
