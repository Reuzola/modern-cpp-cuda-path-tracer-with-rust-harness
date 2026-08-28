#pragma once
#include "pt/geometry/constant_medium.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/render/integrator.hpp"
#include <cassert>
#include <span>

namespace pt {

class Hittable;
class ImportanceTargets;
class Sampler;

class PathIntegrator final : public Integrator {
public:
    PathIntegrator(const Hittable& world, std::span<const ConstantMedium> media,
        const ImportanceTargets& targets, const Color& background, int max_depth)
        : world_(world), media_(media), targets_(targets), background_(background), max_depth_(max_depth) {}

    [[nodiscard]] Color radiance(const Ray& r, Sampler& sampler) const override;

    // Set between passes only: render_pass() reads this while tracing.
    void set_max_depth(int depth) noexcept {
        assert(depth > 0);
        max_depth_ = depth;
    }

private:
    const Hittable& world_;
    std::span<const ConstantMedium> media_;
    const ImportanceTargets& targets_;
    Color background_;
    int max_depth_;

    [[nodiscard]] Color trace(const Ray& r, int depth, Sampler& sampler) const;
};

} // namespace pt
