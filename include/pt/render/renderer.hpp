#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/film.hpp"
#include "pt/render/progress.hpp"
#include "pt/render/tile.hpp"
#include <cstdint>
#include <stop_token>
#include <vector>

namespace pt {

class Accumulator;
class Camera;
class Integrator;
class ThreadPool;
struct RenderSettings;

// Lifetime contract: camera, integrator and pool must outlive Renderer.
// render_pass() returns only once every tile of the pass is done, so camera, integrator and
// settings may be changed between passes from the calling thread, never during one.
class Renderer final {
public:
    static constexpr int default_tile_size = 16;

    Renderer(const Camera& camera,
             const Integrator& integrator,
             const RenderSettings& settings,
             ThreadPool& pool,
             int tile_size = default_tile_size);

    [[nodiscard]] Film render(const ProgressCallback& progress = {}) const;

    // Blocks until the pass is complete. Tiles are handed out one at a time to whichever thread is free.
    void render_pass(Accumulator& acc, int pass_index) const;

    // Cancellable at tile granularity: once stop is requested, tiles not yet started are
    // skipped and running ones finish. Returns false if stop was seen before the pass was
    // counted; acc then holds a partial pass and must be reset() before any further pass.
    [[nodiscard]] bool render_pass(Accumulator& acc, int pass_index, const std::stop_token& stop) const;

    [[nodiscard]] int samples_per_pixel() const noexcept { return sqrt_spp_ * sqrt_spp_; }

    // The pool's workers plus the calling thread, which runs tasks while it waits.
    [[nodiscard]] int thread_count() const noexcept;

    // Changes the stratification grid, so any samples already accumulated become inconsistent.
    void set_samples_per_pixel(int spp);

private:
    const Camera& camera_;
    const Integrator& integrator_;
    ThreadPool& pool_;
    int image_width_{};
    int image_height_{};
    int sqrt_spp_{};
    Float recip_sqrt_spp_{};
    std::vector<Tile> tiles_;
    std::uint64_t seed_{};

    [[nodiscard]] Color render_sample(int x, int y, int pass_index) const;

    void render_tile(Accumulator& acc, const Tile& tile, int pass_index) const;

    [[nodiscard]] static int sqrt_spp_from(int samples_per_pixel) noexcept;
};

} // namespace pt
