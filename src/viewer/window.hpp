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

class Window final {
public:
    Window(int width, int height, std::string_view title);

    [[nodiscard]] bool should_close() const noexcept;

    void poll_events() noexcept;

    void swap_buffers() noexcept;

    [[nodiscard]] std::pair<int, int> framebuffer_size() const noexcept;

private:
    GlfwLibrary library_;
    std::unique_ptr<GLFWwindow, WindowDeleter> handle_;
};

} // namespace pt
