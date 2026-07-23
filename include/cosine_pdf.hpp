#pragma once
#include "constants.hpp"
#include "onb.hpp"
#include "pdf.hpp"
#include "vec3.hpp"
#include <algorithm>

class cosine_pdf final : public pdf {
    public:
        explicit cosine_pdf(const vec3& normal) : uvw(normal) {}

        [[nodiscard]] double value(const vec3& direction) const override {
            const vec3 unit_direction = unit_vector(direction);
            const double cosine_theta = dot(uvw.w(), unit_direction);

            return std::max(0.0, cosine_theta) / pi;
        }

        [[nodiscard]] vec3 generate() const override {
            return uvw.transform(random_cosine_direction());
        }
    private:
        onb uvw;
};