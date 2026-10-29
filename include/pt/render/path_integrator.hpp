#pragma once
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/render/integrator.hpp"
#include <cassert>
#include <span>

namespace pt {

class ImportanceTargets;
class Sampler;
struct HitRecord;

class PathIntegrator final : public Integrator {
public:
    PathIntegrator(const TopLevelBvh& world, std::span<const ConstantMedium> media,
                   const ImportanceTargets& targets, const Color& background, int max_depth)
        : world_(world), media_(media), targets_(targets), background_(background), max_depth_(max_depth) {
        assert(max_depth > 0);
    }

    [[nodiscard]] Color radiance(const Ray& r, Sampler& sampler) const override;

    // Set between passes only: render_pass() reads this while tracing.
    void set_max_depth(int depth) noexcept {
        assert(depth > 0);
        max_depth_ = depth;
    }

private:
    const TopLevelBvh& world_;
    std::span<const ConstantMedium> media_;
    const ImportanceTargets& targets_;
    Color background_;
    int max_depth_; // Segments a path may have, the camera's included: 1 sees emitters and nothing they light.

    [[nodiscard]] Color trace(const Ray& r, int depth, Sampler& sampler) const;

    // The next point the path meets: the closest surface, or a scattering event sampled
    // inside a medium in front of it. False if the ray escapes; rec is then unspecified.
    [[nodiscard]] bool next_interaction(const Ray& r, Sampler& sampler, HitRecord& rec) const;
};

} // namespace pt
