#pragma once
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include <optional>
#include <variant>

namespace pt {

struct hit_record;

struct specular_bounce {
    ray scattered;
};

struct diffuse_bounce {
    pdf_variant sampling_pdf;
};

struct scatter_record {
    color attenuation;
    std::variant<specular_bounce, diffuse_bounce> bounce;
};

class material {
public:
    virtual ~material();

    [[nodiscard]] virtual std::optional<scatter_record> scatter(const ray& r_in, const hit_record& rec) const = 0;

    [[nodiscard]] virtual color emitted(const ray& r_in, const hit_record& rec) const;

    [[nodiscard]] virtual Float scattering_pdf(const ray& r_in, const hit_record& rec, const ray& scattered) const;
};

} // namespace pt
