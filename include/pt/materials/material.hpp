#pragma once
#include "pt/math/color.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include <optional>
#include <variant>

namespace pt {

class Sampler;

struct HitRecord;

struct SpecularBounce {
    Ray scattered;
};

struct DiffuseBounce {
    PdfVariant sampling_pdf;
};

struct ScatterRecord {
    Color attenuation;
    std::variant<SpecularBounce, DiffuseBounce> bounce;
};

class Material {
public:
    virtual ~Material();

    [[nodiscard]] virtual std::optional<ScatterRecord> scatter(const Ray& r_in, const HitRecord& rec, Sampler& sampler) const = 0;

    [[nodiscard]] virtual Color emitted(const Ray& r_in, const HitRecord& rec) const;

    [[nodiscard]] virtual Float scattering_pdf(const Ray& r_in, const HitRecord& rec, const Ray& scattered) const;
};

} // namespace pt
