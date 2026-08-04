#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/film.hpp"
#include "pt/render/progress.hpp"
#include <cstdint>

namespace pt {

class Camera;

class Integrator;

struct RenderSettings;

struct Tile;

// Lifetime contract: camera and integrator must outlive Renderer
// Reference members implicitly delete copy assignment.
class Renderer final {
public:
    static constexpr int default_tile_size = 16;

    Renderer(const Camera& camera, const Integrator& integrator, const RenderSettings& settings, int tile_size = default_tile_size);

    [[nodiscard]] Film render(const ProgressCallback& progress = {}) const;

private:
    const Camera& camera_;
    const Integrator& integrator_;
    int image_width_{};
    int image_height_{};
    int sqrt_spp_{};
    Float recip_sqrt_spp_{};
    Float pixel_samples_scale_{};
    int tile_size_{};
    std::uint64_t seed_{};

    [[nodiscard]] Color render_pixel(int x, int y) const;

    void render_tile(Film& film, const Tile& tile) const;
};

} // namespace pt
