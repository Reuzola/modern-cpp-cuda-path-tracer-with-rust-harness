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
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include <catch2/catch_test_macros.hpp>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace {

using pt::Accumulator;
using pt::Camera;
using pt::CameraSettings;
using pt::Color;
using pt::Film;
using pt::Float;
using pt::Integrator;
using pt::Ray;
using pt::Renderer;
using pt::RenderProgress;
using pt::RenderSettings;
using pt::Sampler;
using pt::Vec3;
using pt::operator""_f;

constexpr CameraSettings straight_ahead{
    .vfov = 90.0_f,
    .lookfrom = pt::Point3{0, 0, 0},
    .lookat = pt::Point3{0, 0, -1},
    .vup = Vec3{0, 1, 0},
    .defocus_angle = 0.0_f,
    .focus_dist = 1.0_f,
};

/// Turns the sampler's next draw into a colour and counts its invocations.
/// Standing in for the path integrator on purpose: what the renderer owes its
/// caller is a sampling schedule, not a light transport result, and a real
/// integrator would make every expectation here depend on a scene as well.
class StubIntegrator final : public Integrator {
public:
    [[nodiscard]] Color radiance(const Ray& r, Sampler& sampler) const override {
        ++calls_;

        // Reads the stream, so two samples that were seeded identically produce
        // identical pixels - which is exactly what must not happen between
        // neighbouring pixels.
        const Float value = sampler.next_scalar();
        return Color(value, r.direction().x(), r.time());
    }

    [[nodiscard]] int calls() const noexcept { return calls_; }

private:
    mutable int calls_ = 0;
};

[[nodiscard]] RenderSettings settings_for(int width, int height, int spp, std::uint64_t seed) {
    RenderSettings settings;
    settings.image_width = width;
    settings.image_height = height;
    settings.samples_per_pixel = spp;
    settings.seed = seed;
    return settings;
}

/// Every channel of every pixel, compared exactly.
[[nodiscard]] bool identical(const Film& a, const Film& b) {
    if (a.width() != b.width() || a.height() != b.height()) return false;

    for (int y = 0; y < a.height(); ++y) {
        for (int x = 0; x < a.width(); ++x) {
            const Color lhs = a.pixel(x, y);
            const Color rhs = b.pixel(x, y);
            if (lhs.r() != rhs.r() || lhs.g() != rhs.g() || lhs.b() != rhs.b()) return false;
        }
    }
    return true;
}

} // namespace

TEST_CASE("the sample count is rounded down to a square", "[render][renderer]") {
    const Camera camera(straight_ahead, 4, 4);
    const StubIntegrator integrator;

    // Samples are stratified on a sqrt(n) by sqrt(n) grid, so the requested count
    // is floored to the nearest square. Reporting the requested number instead
    // would make the accumulator's divisor disagree with the number of samples
    // actually added, and darken the whole image by a few percent.
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 16, 1)).samples_per_pixel() == 16);
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 20, 1)).samples_per_pixel() == 16);
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 24, 1)).samples_per_pixel() == 16);
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 25, 1)).samples_per_pixel() == 25);
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 1, 1)).samples_per_pixel() == 1);
    REQUIRE(Renderer(camera, integrator, settings_for(4, 4, 3, 1)).samples_per_pixel() == 1);
}

TEST_CASE("every pixel gets every sample", "[render][renderer]") {
    const Camera camera(straight_ahead, 5, 3);
    const StubIntegrator integrator;
    const Renderer renderer(camera, integrator, settings_for(5, 3, 9, 1));

    const Film film = renderer.render();

    REQUIRE(film.width() == 5);
    REQUIRE(film.height() == 3);

    // Width times height times the stratification grid, exactly once each. A tile
    // that overlapped its neighbour would show up here as a count that is too
    // high, and a gap as one too low.
    REQUIRE(integrator.calls() == 5 * 3 * 9);
}

TEST_CASE("the same seed gives the same image", "[render][renderer]") {
    const Camera camera(straight_ahead, 8, 6);
    const StubIntegrator first;
    const StubIntegrator second;
    const StubIntegrator third;

    const Film a = Renderer(camera, first, settings_for(8, 6, 4, 12345)).render();
    const Film b = Renderer(camera, second, settings_for(8, 6, 4, 12345)).render();

    // Bit for bit, not within a tolerance: the sampler is seeded per pixel and
    // per pass from the scene's seed alone, with nothing accumulated between
    // samples. This is what makes a golden image possible at all, and it is the
    // property the parallel renderer will have to preserve.
    REQUIRE(identical(a, b));

    const Film c = Renderer(camera, third, settings_for(8, 6, 4, 999)).render();
    REQUIRE_FALSE(identical(a, c));
}

TEST_CASE("neighbouring pixels do not share a sample sequence", "[render][renderer]") {
    const Camera camera(straight_ahead, 4, 4);
    const StubIntegrator integrator;
    const Film film = Renderer(camera, integrator, settings_for(4, 4, 1, 7)).render();

    // The seed mixes in the pixel's linear index, so the first channel - which is
    // a raw draw from the stream - differs between pixels. If it did not, the
    // same noise pattern would be stamped over every pixel and no amount of
    // averaging would ever remove it.
    REQUIRE(film.pixel(0, 0).r() != film.pixel(1, 0).r());
    REQUIRE(film.pixel(0, 0).r() != film.pixel(0, 1).r());

    // Including across a row boundary, where a weaker mix would collide.
    REQUIRE(film.pixel(3, 0).r() != film.pixel(0, 1).r());
}

TEST_CASE("the tile size is invisible in the result", "[render][renderer]") {
    const Camera camera(straight_ahead, 10, 7);
    const StubIntegrator by_default;
    const StubIntegrator by_pixel;
    const StubIntegrator in_one_go;

    const Film standard = Renderer(camera, by_default, settings_for(10, 7, 4, 3)).render();
    const Film per_pixel = Renderer(camera, by_pixel, settings_for(10, 7, 4, 3), 1).render();
    const Film whole_image = Renderer(camera, in_one_go, settings_for(10, 7, 4, 3), 1000).render();

    // Tiles decide the order pixels are visited and nothing else: each sample is
    // seeded from its own coordinates, so no state crosses a tile boundary. This
    // is the precondition for handing the tile loop to a thread pool - and the
    // test that will catch the first accidental shared accumulator when that
    // happens.
    REQUIRE(identical(standard, per_pixel));
    REQUIRE(identical(standard, whole_image));
}

TEST_CASE("render is the pass loop written out", "[render][renderer]") {
    const Camera camera(straight_ahead, 6, 4);
    const StubIntegrator batch;
    const StubIntegrator progressive;

    const Renderer batch_renderer(camera, batch, settings_for(6, 4, 9, 21));
    const Film all_at_once = batch_renderer.render();

    const Renderer progressive_renderer(camera, progressive, settings_for(6, 4, 9, 21));
    Accumulator acc(6, 4);
    for (int pass = 0; pass < progressive_renderer.samples_per_pixel(); ++pass) {
        progressive_renderer.render_pass(acc, pass);
    }

    // The viewer refines an image pass by pass and the command line renders it in
    // one call; both go through the same accumulator in the same order. If they
    // diverged, the picture on screen would not be the picture that gets saved.
    REQUIRE(identical(all_at_once, acc.resolve()));
    REQUIRE(acc.sample_count() == 9);
}

TEST_CASE("a partial render is an unbiased estimate", "[render][renderer]") {
    const Camera camera(straight_ahead, 4, 4);
    const StubIntegrator integrator;
    const Renderer renderer(camera, integrator, settings_for(4, 4, 16, 5));

    Accumulator acc(4, 4);
    renderer.render_pass(acc, 0);
    const Film after_one = acc.resolve();

    renderer.render_pass(acc, 1);
    const Film after_two = acc.resolve();

    // Resolving divides by the passes completed so far, not by the passes
    // requested. A viewer can therefore show a usable image after the first pass
    // instead of a nearly black one that brightens as it converges.
    REQUIRE(acc.sample_count() == 2);
    REQUIRE(after_one.pixel(0, 0).r() > 0.0_f);
    REQUIRE(after_two.pixel(0, 0).r() > 0.0_f);
}

TEST_CASE("changing the sample count restratifies the grid", "[render][renderer]") {
    const Camera camera(straight_ahead, 4, 4);
    const StubIntegrator integrator;
    Renderer renderer(camera, integrator, settings_for(4, 4, 4, 8));

    REQUIRE(renderer.samples_per_pixel() == 4);

    renderer.set_samples_per_pixel(16);
    REQUIRE(renderer.samples_per_pixel() == 16);

    // The stratification grid is part of where a sample lands inside its pixel,
    // so samples taken before the change belong to a different pattern. The
    // caller is expected to reset the accumulator - the renderer holds no
    // accumulated state of its own to invalidate.
    const Film film = renderer.render();
    REQUIRE(integrator.calls() == 4 * 4 * 16);
    REQUIRE(film.width() == 4);
}

TEST_CASE("progress is reported once before and once per pass", "[render][renderer]") {
    const Camera camera(straight_ahead, 3, 3);
    const StubIntegrator integrator;
    const Renderer renderer(camera, integrator, settings_for(3, 3, 9, 11));

    std::vector<RenderProgress> reports;
    static_cast<void>(renderer.render([&reports](RenderProgress progress) { reports.push_back(progress); }));

    // The opening call with nothing completed is what lets a caller draw an empty
    // progress bar before the first pass arrives - which on a heavy scene is the
    // difference between a silent minute and a visible one.
    REQUIRE(reports.size() == 10);
    REQUIRE(reports.front().completed == 0);
    REQUIRE(reports.back().completed == 9);

    for (std::size_t i = 0; i < reports.size(); ++i) {
        REQUIRE(reports[i].completed == static_cast<int>(i));
        REQUIRE(reports[i].total == 9);
    }
}

TEST_CASE("rendering without a callback is the same render", "[render][renderer]") {
    const Camera camera(straight_ahead, 4, 4);
    const StubIntegrator silent;
    const StubIntegrator watched;

    const Film without = Renderer(camera, silent, settings_for(4, 4, 4, 13)).render();
    const Film with = Renderer(camera, watched, settings_for(4, 4, 4, 13)).render([](RenderProgress) {});

    // An empty std::function is a valid argument, checked rather than called. The
    // reporting path must not perturb the result in any way.
    REQUIRE(identical(without, with));
}
