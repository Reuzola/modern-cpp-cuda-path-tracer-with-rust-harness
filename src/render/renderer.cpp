#include "pt/render/renderer.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/accumulator.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/integrator.hpp"
#include "pt/render/progress.hpp"
#include "pt/render/tile.hpp"
#include "pt/scene/scene.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>

namespace pt {

namespace {

[[nodiscard]] Vec3 sample_square_stratified(int s_i, int s_j, Float recip_sqrt_spp, Sampler& sampler) {
    const Float px = ((static_cast<Float>(s_i) + sampler.next_scalar()) * recip_sqrt_spp) - static_cast<Float>(0.5_f);
    const Float py = ((static_cast<Float>(s_j) + sampler.next_scalar()) * recip_sqrt_spp) - static_cast<Float>(0.5_f);
    return Vec3(px, py, static_cast<Float>(0.0_f));
}

} // namespace

Renderer::Renderer(const Camera& camera, const Integrator& integrator, const RenderSettings& settings, int tile_size)
    : camera_(camera),
      integrator_(integrator),
      image_width_(settings.image_width),
      image_height_(settings.image_height),
      sqrt_spp_(static_cast<int>(std::sqrt(static_cast<Float>(settings.samples_per_pixel)))),
      recip_sqrt_spp_(1.0_f / static_cast<Float>(sqrt_spp_)),
      tiles_(make_tiles(image_width_, image_height_, tile_size)),
      seed_(settings.seed) {
    assert(image_width_ > 0 && image_height_ > 0);
    assert(sqrt_spp_ > 0);
    assert(tile_size > 0);
}

Film Renderer::render(const ProgressCallback& progress) const {
    Accumulator acc(image_width_, image_height_);

    const int total = samples_per_pixel();
    if (progress) progress(RenderProgress{0, total});

    for (int pass = 0; pass < total; ++pass) {
        render_pass(acc, pass);
        if (progress) progress(RenderProgress{pass + 1, total});
    }
    return acc.resolve();
}

void Renderer::render_pass(Accumulator& acc, int pass_index) const {
    for (const Tile& tile : tiles_) {
        render_tile(acc, tile, pass_index);
    }
    acc.end_pass();
}

Color Renderer::render_sample(int x, int y, int pass_index) const {
    assert(pass_index >= 0 && pass_index < samples_per_pixel());
    const std::uint64_t pixel_index = static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(image_width_) + static_cast<std::uint64_t>(x);

    const int s_i = pass_index % sqrt_spp_;
    const int s_j = pass_index / sqrt_spp_;

    Sampler sampler(sampler_seed(seed_, pixel_index, static_cast<std::uint64_t>(pass_index)));

    const Vec3 offset = sample_square_stratified(s_i, s_j, recip_sqrt_spp_, sampler);
    const Ray ray = camera_.generate_ray(x, y, offset, sampler);

    return integrator_.radiance(ray, sampler);
}

void Renderer::render_tile(Accumulator& acc, const Tile& tile, int pass_index) const {
    for (int y = tile.y0; y < tile.y1; y++) {
        for (int x = tile.x0; x < tile.x1; x++) {
            acc.add_sample(x, y, render_sample(x, y, pass_index));
        }
    }
}

} // namespace pt
