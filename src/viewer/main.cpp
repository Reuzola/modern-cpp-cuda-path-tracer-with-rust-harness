#include "pt/io/color.hpp"
#include "pt/io/image_format.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/render_session.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/scene/scene_error.hpp"
#include "pt/scene/scene_loader.hpp"
#include "pt/util/log.hpp"
#include "pt/util/thread_pool.hpp"
#include "viewer/camera_controller.hpp"
#include "viewer/cli.hpp"
#include "viewer/controls.hpp"
#include "viewer/display.hpp"
#include "viewer/edit_pacer.hpp"
#include "viewer/frame_limiter.hpp"
#include "viewer/frame_measurement.hpp"
#include "viewer/gui.hpp"
#include "viewer/screenshot.hpp"
#include "viewer/window.hpp"
#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <iostream>
#include <optional>
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
    try {
        const std::variant<pt::ViewerOptions, int> parsed = pt::parse_viewer_command_line(argc, argv);
        if (const int* exit_code = std::get_if<int>(&parsed)) return *exit_code;

        const pt::ViewerOptions& opts = std::get<pt::ViewerOptions>(parsed);
        pt::set_log_level(opts.log_level);

        const int threads = pt::resolve_render_threads(opts.threads);
        pt::ThreadPool pool(static_cast<unsigned>(threads - 1));
        pt::log_info("Threads: {}", threads);

        pt::Scene scene(pt::load_scene(opts.scene));
        pt::apply_overrides(scene, opts);

        const int img_w = scene.render.image_width;
        const int img_h = scene.render.image_height;
        pt::Window window(img_w, img_h, "pathtracer viewer");
        pt::Display display(img_w, img_h);

        // Platform scale is the default; XWayland reports 1.0 regardless of DPI, hence the override.
        pt::Gui gui(window, opts.ui_scale.value_or(window.content_scale()));

        pt::CameraController controller(scene.camera);

        pt::ViewerControls controls{
            .tone_map = scene.render.tone_map,
            .max_depth = scene.render.max_depth,
            .target_spp = scene.render.samples_per_pixel,
        };

        std::vector<std::uint8_t> pixels;

        pt::Film resolved(img_w, img_h);
        bool display_dirty{true};

        const auto current_parameters = [&controller, &controls] {
            return pt::SessionParameters{.camera = controller.settings(), .max_depth = controls.max_depth, .samples_per_pixel = controls.target_spp};
        };

        // After the pool and the scene it uses: destroyed, and its thread joined, before either.
        pt::RenderSession session(scene, pool, current_parameters());
        pt::FrameInfo shown{}; // The image on screen; zero until the first arrives.
        pt::EditPacer pacer;
        std::uint64_t latest_generation{};
        bool edit_pending{};

        std::optional<pt::FrameMeasurement> measurement;
        if (opts.measure_images) measurement.emplace(*opts.measure_images);

        const int refresh = window.refresh_rate();

        // 60 when the platform reports no rate: a guess, but a busy loop is the worse default.
        const int max_fps = opts.max_fps.value_or(refresh > 0 ? refresh : 60);
        pt::FrameLimiter limiter(max_fps);

        if (max_fps > 0)
            pt::log_info("Frame cap: {} fps", max_fps);
        else
            pt::log_info("Frame rate: uncapped");

        auto last_time = std::chrono::steady_clock::now();
        bool looking{};
        while (!window.should_close()) {
            const auto frame_start = pt::FrameClock::now();
            pt::FrameTimes times{};

            window.poll_events();
            gui.begin_frame();

            const pt::ControlChange change = gui.draw_controls(controls);
            if (change.accumulation) edit_pending = true;
            if (change.display) display_dirty = true;

            if (!gui.wants_keyboard()) {
                if (gui.key_pressed(pt::ViewerKey::r)) {
                    controller.reset();
                    edit_pending = true;
                }
                if (gui.key_pressed(pt::ViewerKey::f2)) pt::save_screenshot(pt::tone_map(resolved, controls.tone_map), pt::ImageFormat::png);
                if (gui.key_pressed(pt::ViewerKey::f3)) pt::save_screenshot(resolved, pt::ImageFormat::exr);
            }

            const auto now = std::chrono::steady_clock::now();
            const std::chrono::duration<double> frame_time = now - last_time;
            last_time = now;
            const pt::Float dt = static_cast<pt::Float>(std::min(frame_time.count(), max_frame_time));

            const bool is_rmb_down = window.is_mouse_button_down(pt::MouseButton::right);
            const bool desired_looking = looking ? is_rmb_down : (is_rmb_down && !gui.wants_mouse());
            if (desired_looking != looking) {
                looking = desired_looking;
                window.set_cursor_mode(looking ? pt::CursorMode::hidden : pt::CursorMode::normal);
            }

            const bool moving = !gui.wants_keyboard();
            const pt::CameraInput input = measurement ? measurement->scripted_input(pacer.ready()) : read_camera_input(window, looking, moving);
            if (controller.update(input, dt)) edit_pending = true;

            if (edit_pending && pacer.ready()) {
                latest_generation = session.update(current_parameters());
                pacer.posted(latest_generation);
                edit_pending = false;
            }

            bool new_image{};
            if (const std::optional<pt::FrameInfo> info = session.take_frame(resolved)) {
                shown = *info;
                new_image = true;
                display_dirty = true;
                // Mirror the square count the renderer adopted, but only for the latest posted
                // edit: otherwise a stale value would overwrite one not yet posted.
                if (shown.generation == latest_generation && !edit_pending) controls.target_spp = shown.target_spp;
            }

            if (display_dirty) {
                const auto display_start = pt::FrameClock::now();
                film_to_bytes(resolved, controls.tone_map, pixels);
                display.upload(pixels);
                times.display_ms += pt::ms_since(display_start);

                display_dirty = false;
            }

            const auto [fb_w, fb_h] = window.framebuffer_size();
            gui.draw_hud({
                .sample_count = shown.sample_count,
                .target_spp = controls.target_spp,
                .accumulated_seconds = shown.elapsed_seconds,
                .camera_position = controller.settings().lookfrom,
            });

            // Safe to draw after the UI trashed GL state last frame: draw() rebinds everything it needs.
            const auto present_start = pt::FrameClock::now();
            display.draw(fb_w, fb_h);
            gui.end_frame();
            window.swap_buffers();
            times.present_ms = pt::ms_since(present_start);
            times.frame_ms = pt::ms_since(frame_start);
            times.new_image = new_image;
            if (new_image) times.latency_ms = pacer.displayed(shown.generation);

            if (measurement) {
                const bool converged = pacer.ready() && !edit_pending && shown.target_spp > 0 && shown.sample_count >= shown.target_spp;
                measurement->record(times, converged);
                if (measurement->done()) {
                    // stdout carries the record and nothing else; logs go to stderr, so runs can be appended to one file.
                    // clang-format off
                    pt::write_measurement({
                        .scene = opts.scene.string(),
                        .width = img_w,
                        .height = img_h,
                        .threads = threads,
                        .moving = measurement->moving(),
                        .still = measurement->still(),
                        .passes_per_second = shown.elapsed_seconds > 0.0 ? static_cast<double>(shown.sample_count) / shown.elapsed_seconds : 0.0,
                    }, std::cout);
                    // clang-format on
                    break;
                }
            }

            // Outside the measured span: frame_ms is what a frame costs; the cap only spaces frames out.
            limiter.wait();
        }

        return EXIT_SUCCESS;
    } catch (const pt::SceneError& e) {
        pt::log_error("{}", e.what());
        return EXIT_FAILURE;
    } catch (const std::exception& e) {
        pt::log_error("Viewer error: {}", e.what());
        return EXIT_FAILURE;
    } catch (...) {
        pt::log_error("Unknown viewer error");
        return EXIT_FAILURE;
    }
}
