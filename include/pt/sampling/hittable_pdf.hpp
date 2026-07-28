#pragma once
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

namespace pt {

class hittable;

class hittable_pdf final : public pdf {
public:
    hittable_pdf(const hittable& objects, const point3& origin) : objects(objects), origin(origin) {}

    [[nodiscard]] double value(const vec3& direction) const override;

    [[nodiscard]] vec3 generate() const override;

private:
    const hittable& objects;
    point3 origin;
};

} // namespace pt
