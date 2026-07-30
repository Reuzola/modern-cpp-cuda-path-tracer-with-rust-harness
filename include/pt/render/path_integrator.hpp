#pragma once
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/render/integrator.hpp"

namespace pt {

class Hittable;

class ImportanceTargets;

class PathIntegrator final : public Integrator {
public:
    PathIntegrator(const Hittable& world, const ImportanceTargets& targets, const Color& background, int max_depth)
        : world_(world), targets_(targets), background_(background), max_depth_(max_depth) {}

    [[nodiscard]] Color radiance(const Ray& r) const override;

private:
    const Hittable& world_;
    const ImportanceTargets& targets_;
    Color background_;
    int max_depth_;

    [[nodiscard]] Color trace(const Ray& r, int depth) const;
};

} // namespace pt
