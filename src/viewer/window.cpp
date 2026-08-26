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

} // namespace pt
