#include "pt/post/tonemap.hpp"
#include "pt/math/color.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/scalar.hpp"
#include "pt/render/film.hpp"
#include <cmath>

namespace pt {

namespace {

[[nodiscard]] Float sanitize(Float x) noexcept {
    return std::isnan(x) ? 0.0_f : x;
}

[[nodiscard]] Float reinhard(Float x) noexcept {
    return x / (1.0_f + x);
}

[[nodiscard]] Float aces(Float x) noexcept {
    static constexpr Float aces_a = 2.51_f;
    static constexpr Float aces_b = 0.03_f;
    static constexpr Float aces_c = 2.43_f;
    static constexpr Float aces_d = 0.59_f;
    static constexpr Float aces_e = 0.14_f;

    return (x * (aces_a * x + aces_b)) / (x * (aces_c * x + aces_d) + aces_e);
}

[[nodiscard]] Float apply_operator(Float x, ToneMapOperator op) noexcept {
    switch (op) {
    case ToneMapOperator::none:
        return x;
    case ToneMapOperator::reinhard:
        return reinhard(x);
    case ToneMapOperator::aces:
        return aces(x);
    }
    return x;
}

// sRGB OETF (IEC 61966-2-1). The linear segment near black bounds the slope where
// the power function's derivative is unbounded, which would otherwise band in 8 bits.
[[nodiscard]] Float encode_srgb(Float x) noexcept {
    static constexpr Float linear_cutoff = 0.0031308_f;
    static constexpr Float linear_slope = 12.92_f;
    static constexpr Float alpha = 1.055_f;
    static constexpr Float offset = 0.055_f;
    static constexpr Float gamma_exponent = 1.0_f / 2.4_f;

    if (x <= 0) return 0.0_f;
    if (x < linear_cutoff) return linear_slope * x;

    return alpha * std::pow(x, gamma_exponent) - offset;
}

[[nodiscard]] Float clamp_display(Float x) noexcept {
    static constexpr Interval intensity(0.000_f, 0.999_f);
    return intensity.clamp(x);
}

[[nodiscard]] Float map_channel(Float x, const ToneMapSettings& s) noexcept {
    x = sanitize(x);
    x = x * s.exposure;
    x = apply_operator(x, s.op);
    x = encode_srgb(x);
    x = clamp_display(x);
    return x;
}

[[nodiscard]] Color map_pixel(const Color& c, const ToneMapSettings& s) noexcept {
    return Color(map_channel(c.r(), s), map_channel(c.g(), s), map_channel(c.b(), s));
}

} // namespace

Film tone_map(const Film& film, const ToneMapSettings& settings) {
    Film result(film.width(), film.height());

    for (int y = 0; y < film.height(); y++) {
        for (int x = 0; x < film.width(); x++) {
            result.set_pixel(x, y, map_pixel(film.pixel(x, y), settings));
        }
    }
    return result;
}

} // namespace pt
