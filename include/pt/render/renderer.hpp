#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/film.hpp"
#include "pt/render/progress.hpp"
#include "pt/render/tile.hpp"
#include <cstdint>
#include <vector>

namespace pt {

class Accumulator;
class Camera;
class Integrator;
struct RenderSettings;

// Lifetime contract: camera and integrator must outlive Renderer.
// The referenced camera may be reassigned between passes; it is read per sample.
// Reference members implicitly delete copy assignment.
class Renderer final {
public:
    static constexpr int default_tile_size = 16;

    Renderer(const Camera& camera, const Integrator& integrator, const RenderSettings& settings, int tile_size = default_tile_size);

    [[nodiscard]] Film render(const ProgressCallback& progress = {}) const;

    void render_pass(Accumulator& acc, int pass_index) const;

    [[nodiscard]] int samples_per_pixel() const noexcept { return sqrt_spp_ * sqrt_spp_; }

private:
    const Camera& camera_;
    const Integrator& integrator_;
    int image_width_{};
    int image_height_{};
    int sqrt_spp_{};
    Float recip_sqrt_spp_{};
    std::vector<Tile> tiles_;
    std::uint64_t seed_{};

    [[nodiscard]] Color render_sample(int x, int y, int pass_index) const;

    void render_tile(Accumulator& acc, const Tile& tile, int pass_index) const;
};

} // namespace pt
