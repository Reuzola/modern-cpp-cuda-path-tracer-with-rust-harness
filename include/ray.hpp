#pragma once
#include "vec3.hpp"

class ray {
    public:
        ray() = default;
        ray(const point3& origin, const vec3& direction) : ray(origin, direction, 0.0) {}
        ray(const point3& origin, const vec3& direction, double tm) : orig(origin), dir(direction), tm(tm) {}

        [[nodiscard]] const point3& origin() const { return orig; }
        [[nodiscard]] const vec3& direction() const { return dir; }
        [[nodiscard]] double time() const { return tm; }

        point3 at(double t) const {
            return orig + t * dir;
        }

    private:
        point3 orig;
        vec3 dir;
        double tm{};
};