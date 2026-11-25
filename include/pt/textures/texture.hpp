#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

class Texture {
public:
    virtual ~Texture();

    [[nodiscard]] virtual Color value(Float u, Float v, const Point3& p) const = 0;

    // Whether a lookup can depend on u and v. Fixed when the texture is built, so a fill can ask before computing
    // coordinates no lookup would read.
    [[nodiscard]] bool reads_uv() const noexcept { return reads_uv_; }

protected:
    // A texture that does not say is taken to read uv: a wrong yes costs a fill its coordinates, a wrong no the image.
    Texture() noexcept = default;
    explicit Texture(bool reads_uv) noexcept : reads_uv_(reads_uv) {}

private:
    bool reads_uv_ = true;
};

// The same question about a texture that may be missing, which is answered the safe way.
[[nodiscard]] inline bool texture_reads_uv(const Texture* texture) noexcept { return texture == nullptr || texture->reads_uv(); }

} // namespace pt
