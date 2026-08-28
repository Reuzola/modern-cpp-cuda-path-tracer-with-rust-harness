#include "pt/io/color.hpp"
#include "pt/io/image_format.hpp"
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
#include "viewer/camera_controller.hpp"
#include "viewer/cli.hpp"
#include "viewer/controls.hpp"
#include "viewer/display.hpp"
#include "viewer/gui.hpp"
#include "viewer/screenshot.hpp"
#include "viewer/window.hpp"
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
[[nodiscard]] pt::CameraInput read_camera_input(pt::Window& window, bool looking, bool moving) noexcept {
    pt::CameraInput input{};

    if (moving) {
        auto axis = [&window](pt::Key pos, pt::Key neg) -> pt::Float {
            const pt::Float pos_val = static_cast<pt::Float>(window.is_key_down(pos));
            const pt::Float neg_val = static_cast<pt::Float>(window.is_key_down(neg));
            return pos_val - neg_val;
        };

        const pt::Float x = axis(pt::Key::d, pt::Key::a);
        const pt::Float y = axis(pt::Key::e, pt::Key::q);
        const pt::Float z = axis(pt::Key::w, pt::Key::s);
        input.move = pt::Vec3(x, y, z);
        input.fast = window.is_key_down(pt::Key::left_shift);
        input.slow = window.is_key_down(pt::Key::left_control);
    }

    if (looking) {
        const auto [dx, dy] = window.cursor_delta();
        input.look_dx = static_cast<pt::Float>(dx);
        input.look_dy = static_cast<pt::Float>(dy);
    }

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

        // Platform scale is the default; XWayland reports 1.0 regardless of DPI, hence the override.
        pt::Gui gui(window, opts.ui_scale.value_or(window.content_scale()));

        pt::CameraController controller(scene.camera);
        pt::Camera camera(controller.settings(), img_w, img_h);
        pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
        pt::Renderer renderer(camera, integrator, scene.render);
        pt::ViewerControls controls{
            .tone_map = scene.render.tone_map,
            .max_depth = scene.render.max_depth,
            .target_spp = renderer.samples_per_pixel(),
        };

        pt::Accumulator acc(img_w, img_h);
        std::vector<std::uint8_t> pixels;

        pt::Film resolved(img_w, img_h);
        bool display_dirty{true};

        // Every restart must clear the stopwatch too; keep the two in one place.
        double accumulated_seconds{};
        const auto restart_accumulation = [&acc, &accumulated_seconds]() noexcept {
            acc.reset();
            accumulated_seconds = 0.0;
        };

        auto last_time = std::chrono::steady_clock::now();
        bool looking{};
        while (!window.should_close()) {
            window.poll_events();
            gui.begin_frame();

            const pt::ControlChange change = gui.draw_controls(controls);
            if (change.accumulation) {
                integrator.set_max_depth(controls.max_depth);
                renderer.set_samples_per_pixel(controls.target_spp);
                controls.target_spp = renderer.samples_per_pixel(); // mirror whatever the renderer actually adopted
                restart_accumulation();
            }
            if (change.display) display_dirty = true;

            if (!gui.wants_keyboard()) {
                if (gui.key_pressed(pt::ViewerKey::r)) {
                    controller.reset();
                    camera = pt::Camera(controller.settings(), img_w, img_h);
                    restart_accumulation();
                }
                if (gui.key_pressed(pt::ViewerKey::f2)) pt::save_screenshot(pt::tone_map(resolved, controls.tone_map), pt::ImageFormat::png);
                if (gui.key_pressed(pt::ViewerKey::f3)) pt::save_screenshot(resolved, pt::ImageFormat::exr);
            }

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<double> frame_time = now - last_time;
            last_time = now;
            const auto dt = static_cast<pt::Float>(std::min(frame_time.count(), max_frame_time));

            const bool is_rmb_down = window.is_mouse_button_down(pt::MouseButton::right);
            const bool desired_looking = looking ? is_rmb_down : (is_rmb_down && !gui.wants_mouse());
            if (desired_looking != looking) {
                looking = desired_looking;
                window.set_cursor_mode(looking ? pt::CursorMode::hidden : pt::CursorMode::normal);
            }

            const bool moving = !gui.wants_keyboard();
            const pt::CameraInput input = read_camera_input(window, looking, moving);
            if (controller.update(input, dt)) {
                camera = pt::Camera(controller.settings(), img_w, img_h);
                restart_accumulation();
            }

            if (acc.sample_count() < renderer.samples_per_pixel()) {
                const auto pass_start = std::chrono::steady_clock::now();
                renderer.render_pass(acc, acc.sample_count());
                accumulated_seconds += std::chrono::duration<double>(std::chrono::steady_clock::now() - pass_start).count();
                resolved = acc.resolve();
                display_dirty = true;
            }

            if (display_dirty) {
                film_to_bytes(resolved, controls.tone_map, pixels);
                display.upload(pixels);
                display_dirty = false;
            }

            const auto [fb_w, fb_h] = window.framebuffer_size();
            gui.draw_hud({
                .sample_count = acc.sample_count(),
                .target_spp = renderer.samples_per_pixel(),
                .accumulated_seconds = accumulated_seconds,
                .camera_position = controller.settings().lookfrom,
            });

            // Safe to draw after the UI trashed GL state last frame: draw() rebinds everything it needs.
            display.draw(fb_w, fb_h);
            gui.end_frame();
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
