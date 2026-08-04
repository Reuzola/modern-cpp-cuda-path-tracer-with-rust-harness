#include "pt/render/renderer.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/integrator.hpp"
#include "pt/render/progress.hpp"
#include "pt/render/tile.hpp"
#include "pt/scene/scene.hpp"
#include <cassert>
#include <cmath>
#include <cstdint>
#include <vector>

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
      pixel_samples_scale_(1.0_f / static_cast<Float>(sqrt_spp_ * sqrt_spp_)),
      tile_size_(tile_size),
      seed_(settings.seed) {
    assert(image_width_ > 0 && image_height_ > 0);
    assert(sqrt_spp_ > 0);
    assert(tile_size_ > 0);
}

Color Renderer::render_pixel(int x, int y) const {
    Color pixel_color(0, 0, 0);

    const auto pixel_index = static_cast<std::uint64_t>(y) * static_cast<std::uint64_t>(image_width_) + static_cast<std::uint64_t>(x);

    for (int s_j = 0; s_j < sqrt_spp_; s_j++) {
        for (int s_i = 0; s_i < sqrt_spp_; s_i++) {
            const auto sample_index = static_cast<std::uint64_t>(s_j) * static_cast<std::uint64_t>(sqrt_spp_) + static_cast<std::uint64_t>(s_i);

            Sampler sampler(sampler_seed(seed_, pixel_index, sample_index));

            const Vec3 offset = sample_square_stratified(s_i, s_j, recip_sqrt_spp_, sampler);
            const Ray ray = camera_.generate_ray(x, y, offset, sampler);
            pixel_color += integrator_.radiance(ray, sampler);
        }
    }

    return pixel_samples_scale_ * pixel_color;
}

Film Renderer::render(const ProgressCallback& progress) const {
    Film film(image_width_, image_height_);

    const std::vector<Tile> tiles = make_tiles(image_width_, image_height_, tile_size_);

    const int total = static_cast<int>(tiles.size());
    if (progress) progress(RenderProgress{0, total});

    int completed = 0;
    for (const Tile& tile : tiles) {
        render_tile(film, tile);
        completed++;
        if (progress) progress(RenderProgress{completed, total});
    }
    return film;
}

void Renderer::render_tile(Film& film, const Tile& tile) const {
    for (int y = tile.y0; y < tile.y1; y++) {
        for (int x = tile.x0; x < tile.x1; x++) {
            film.set_pixel(x, y, render_pixel(x, y));
        }
    }
}

} // namespace pt
