#pragma once
#include "vec3.hpp"

class pdf {
    public:
        [[nodiscard]] virtual double value(const vec3& direction) const = 0;

        [[nodiscard]] virtual vec3 generate() const = 0;
    protected:
        ~pdf() = default;
};