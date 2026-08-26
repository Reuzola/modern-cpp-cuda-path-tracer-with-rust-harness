#pragma once
#include <cstdint>
#include <span>

namespace pt {

// Display must be destroyed before Window (requires an active OpenGL context).
class Display final {
public:
    Display(int width, int height);
    ~Display();

    Display(const Display&) = delete;
    Display& operator=(const Display&) = delete;
    Display(Display&&) = delete;
    Display& operator=(Display&&) = delete;

    void upload(std::span<const std::uint8_t> pixels);
    void draw(int framebuffer_width, int framebuffer_height) const;

    [[nodiscard]] int width() const noexcept { return width_; }
    [[nodiscard]] int height() const noexcept { return height_; }

private:
    unsigned int texture_{};
    unsigned int program_{};
    unsigned int vao_{};
    int width_{};
    int height_{};
};

} // namespace pt
