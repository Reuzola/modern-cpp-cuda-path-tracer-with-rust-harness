#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include <optional>

namespace pt {

class texture;

class diffuse_light final : public material {
public:
    explicit diffuse_light(const texture* tex) : tex(tex) {}

    [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override;

    [[nodiscard]] color emitted(const ray& r_in, const hit_record& rec) const override;

private:
    const texture* tex;
};

} // namespace pt
