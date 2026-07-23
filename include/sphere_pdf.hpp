#pragma once
#include "constants.hpp"
#include "pdf.hpp"
#include "vec3.hpp"

class sphere_pdf final : public pdf {
    public:
        [[nodiscard]] double value(const vec3&) const override {
            return 1.0 / (4.0 * pi);
        }

        [[nodiscard]] vec3 generate() const override {
            return random_unit_vector();
        }
};