#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include <optional>
#include <variant>

namespace pt {

class Sampler;
struct HitRecord;

// A single direction, with no density to divide by. Only the direction: which side of the surface the bounce
// leaves from depends on it and is the integrator's to decide, and the path's time is the path's.
struct SpecularBounce {
    Vec3 direction;
};

struct DiffuseBounce {
    PdfVariant sampling_pdf;
};

struct ScatterRecord {
    Color attenuation;
    std::variant<SpecularBounce, DiffuseBounce> bounce;
};

// What a surface or a medium does to light, judged from directions alone: `incoming` is the direction of the
// ray that arrived, pointing at the point and of any length. Rays - where a bounce starts, when it happens -
// belong to the integrator.
class Material {
public:
    virtual ~Material();

    [[nodiscard]] virtual std::optional<ScatterRecord> scatter(const Vec3& incoming, const HitRecord& rec, Sampler& sampler) const = 0;

    [[nodiscard]] virtual Color emitted(const Vec3& incoming, const HitRecord& rec) const;

    [[nodiscard]] virtual Float scattering_pdf(const Vec3& incoming, const HitRecord& rec, const Vec3& scattered) const;

    // Whether shading this material can read the hit's u and v. Fixed at construction from its textures, so a fill
    // can skip coordinates nothing would look up.
    [[nodiscard]] bool reads_uv() const noexcept { return reads_uv_; }

protected:
    // A material that does not say is taken to read uv, for the reason a texture is.
    Material() noexcept = default;
    explicit Material(bool reads_uv) noexcept : reads_uv_(reads_uv) {}

private:
    bool reads_uv_ = true;
};

} // namespace pt
