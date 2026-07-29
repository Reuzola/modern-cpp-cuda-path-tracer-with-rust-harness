#pragma once
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class Hittable;

class HittablePdf final : public Pdf {
public:
    HittablePdf(const Hittable& objects, const Point3& origin) : objects(objects), origin(origin) {}

    [[nodiscard]] Float value(const Vec3& direction) const override;

    [[nodiscard]] Vec3 generate() const override;

private:
    const Hittable& objects;
    Point3 origin;
};

} // namespace pt
