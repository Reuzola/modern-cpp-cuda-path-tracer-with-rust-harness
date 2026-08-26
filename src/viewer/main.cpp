#include "pt/io/color.hpp"
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
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <exception>
#include <variant>
#include <vector>

namespace {

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

        const pt::Camera camera(scene.camera, img_w, img_h);
        const pt::PathIntegrator integrator(scene.world(), scene.media(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
        const pt::Renderer renderer(camera, integrator, scene.render);

        pt::Accumulator acc(img_w, img_h);
        const int target_spp = renderer.samples_per_pixel();
        std::vector<std::uint8_t> pixels;

        const auto start = std::chrono::steady_clock::now();
        while (!window.should_close()) {
            window.poll_events();

            if (acc.sample_count() < target_spp) {
                renderer.render_pass(acc, acc.sample_count());
                film_to_bytes(acc.resolve(), scene.render.tone_map, pixels);
                display.upload(pixels);

                if (acc.sample_count() == target_spp) {
                    const auto end = std::chrono::steady_clock::now();
                    const std::chrono::duration<double> elapsed = end - start;
                    pt::log_info("Elapsed time: {:.2f}s", elapsed.count());
                    pt::log_info("Reached spp: {}", acc.sample_count());
                }
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
