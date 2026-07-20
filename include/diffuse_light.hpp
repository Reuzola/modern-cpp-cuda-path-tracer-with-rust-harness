#pragma once
#include "material.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include <optional>

class diffuse_light : public material {
    public:
        explicit diffuse_light(const texture* tex) : tex(tex) {}

        [[nodiscard]] std::optional<scatter_record> scatter(const ray&, const hit_record&) const override {
            return std::nullopt;
        }

        [[nodiscard]] color emitted(double u, double v, const point3& p) const override {
            return tex->value(u, v, p);
        }
    private:
        const texture* tex;
};