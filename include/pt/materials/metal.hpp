#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include <optional>

namespace pt {

class metal final : public material {
public:
    explicit metal(const color& albedo, Float fuzz);

    [[nodiscard]] std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const override;

private:
    color albedo;
    Float fuzz{};
};

} // namespace pt
