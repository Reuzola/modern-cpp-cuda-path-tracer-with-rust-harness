#pragma once
#include "pt/math/constants.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf.hpp"

class sphere_pdf final : public pdf {
public:
    [[nodiscard]] double value(const vec3&) const override {
        return 1.0 / (4.0 * pi);
    }

    [[nodiscard]] vec3 generate() const override {
        return random_unit_vector();
    }
};
