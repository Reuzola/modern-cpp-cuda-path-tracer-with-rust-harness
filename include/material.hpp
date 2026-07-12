#pragma once
#include "vec3.hpp"
#include "ray.hpp"
#include <optional>

struct hit_record;

struct scatter_record {
    color attenuation;
    ray scattered;
};

class material {
    public:
        virtual ~material() = default;

        [[nodiscard]] virtual std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const = 0;
};