#pragma once
#include "hittable.hpp"
#include "pdf.hpp"
#include "vec3.hpp"

class hittable_pdf final : public pdf {
    public:
        hittable_pdf(const hittable& objects, const point3& origin) : objects(objects), origin(origin) {}

        [[nodiscard]] double value(const vec3& direction) const override {
            return objects.pdf_value(origin, direction);
        }

        [[nodiscard]] vec3 generate() const override {
            return objects.random(origin);
        }
    private:
        const hittable& objects;
        point3 origin;
};