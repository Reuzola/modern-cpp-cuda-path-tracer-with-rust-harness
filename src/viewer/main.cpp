#include "pt/io/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/accumulator.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_error.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/log.hpp"
#include "viewer/cli.hpp"
#include "viewer/display.hpp"
#include "viewer/window.hpp"
#include "viewer/camera_controller.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <variant>
#include <vector>

namespace {

// Clamp so one held key can't jump the camera across the scene between two visible frames.
constexpr double max_frame_time = 0.1;

void film_to_bytes(const pt::Film& film, const pt::ToneMapSettings& settings, std::vector<std::uint8_t>& out) {
    const pt::Film display_film = pt::tone_map(film, settings);
    const std::size_t width = static_cast<std::size_t>(display_film.width());
    const std::size_t height = static_cast<std::size_t>(display_film.height());
    out.resize(width * height * 3);

    for (std::size_t y = 0; y < height; ++y) {
        for (std::size_t x = 0; x < width; ++x) {
            const auto bytes = pt::to_ldr_bytes(display_film.pixel(static_cast<int>(x), static_cast<int>(y)));

            const std::size_t index = (y * width + x) * 3;
            out[index] = bytes[0];
            out[index + 1] = bytes[1];
            out[index + 2] = bytes[2];
        }
    }
}

// Key bindings live here, not in Window: the device layer stays free of semantics.
[[nodiscard]] pt::CameraInput read_camera_input(pt::Window& window, bool looking) noexcept {
    pt::CameraInput input{};
    auto axis = [&window](pt::Key pos, pt::Key neg) -> pt::Float {
        const pt::Float pos_val = static_cast<pt::Float>(window.is_key_down(pos));
        const pt::Float neg_val = static_cast<pt::Float>(window.is_key_down(neg));
        return pos_val - neg_val;
    };

    const pt::Float x = axis(pt::Key::d, pt::Key::a);
    const pt::Float y = axis(pt::Key::e, pt::Key::q);
    const pt::Float z = axis(pt::Key::w, pt::Key::s);
    input.move = pt::Vec3(x, y, z);

    if (looking) {
        const auto [dx, dy] = window.cursor_delta();
        input.look_dx = static_cast<pt::Float>(dx);
        input.look_dy = static_cast<pt::Float>(dy);
    }

    input.fast = window.is_key_down(pt::Key::left_shift);
    input.slow = window.is_key_down(pt::Key::left_control);
    return input;
}

} // namespace

int main(int argc, char** argv) {
    const auto parsed = pt::parse_viewer_command_line(argc, argv);
    if (const int* exit_code = std::get_if<int>(&parsed)) return *exit_code;

    const auto& opts = std::get<pt::ViewerOptions>(parsed);
    pt::set_log_level(opts.log_level);

    try {
        pt::Scene scene(pt::load_scene(opts.scene));
        pt::apply_overrides(scene, opts);

        const int img_w = scene.render.image_width;
        const int img_h = scene.render.image_height;
        pt::Window window(img_w, img_h, "pathtracer viewer");
        pt::Display display(img_w, img_h);

        pt::CameraController controller(scene.camera);
        pt::Camera camera(controller.settings(), img_w, img_h);
        const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
        const pt::Renderer renderer(camera, integrator, scene.render);

        pt::Accumulator acc(img_w, img_h);
        const int target_spp = renderer.samples_per_pixel();
        std::vector<std::uint8_t> pixels;

        auto last_time = std::chrono::steady_clock::now();
        bool looking{};
        while (!window.should_close()) {
            window.poll_events();

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<double> frame_time = now - last_time;
            last_time = now;
            const auto dt = static_cast<pt::Float>(std::min(frame_time.count(), max_frame_time));

            const bool is_rmb_down = window.is_mouse_button_down(pt::MouseButton::right);
            if (is_rmb_down != looking) {
                looking = is_rmb_down;
                window.set_cursor_mode(looking ? pt::CursorMode::hidden : pt::CursorMode::normal);
            }

            const pt::CameraInput input = read_camera_input(window, looking);
            if (controller.update(input, dt)) {
                camera = pt::Camera(controller.settings(), img_w, img_h);
                acc.reset();
            }

            if (acc.sample_count() < target_spp) {
                renderer.render_pass(acc, acc.sample_count());
                film_to_bytes(acc.resolve(), scene.render.tone_map, pixels);
                display.upload(pixels);
            }

            const auto [fb_w, fb_h] = window.framebuffer_size();
            display.draw(fb_w, fb_h);

            window.swap_buffers();
        }

        return EXIT_SUCCESS;
    } catch (const pt::SceneError& e) {
        pt::log_error("{}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        pt::log_error("Viewer error: {}", e.what());
        return EXIT_FAILURE;
    }
}
