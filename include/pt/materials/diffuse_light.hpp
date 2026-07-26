#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <optional>

namespace pt {

class diffuse_light : public material {
public:
    explicit diffuse_light(const texture* tex) : tex(tex) {}

    [[nodiscard]] std::optional<scatter_record> scatter(const ray&, const hit_record&) const override {
        return std::nullopt;
    }

    [[nodiscard]] color emitted(const ray&, const hit_record& rec) const override {
        if (!rec.front_face) return color(0.0, 0.0, 0.0);
        return tex->value(rec.u, rec.v, rec.p);
    }

private:
    const texture* tex;
};

} // namespace pt
