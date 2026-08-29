#pragma once
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"

namespace pt_test {

/// Reports a fixed colour and remembers the coordinates it was asked about.
/// Lets a caller's forwarding be checked directly, rather than inferred from a
/// texture whose output happens to vary with position.
class ProbeTexture final : public pt::Texture {
public:
    explicit ProbeTexture(const pt::Color& answer) noexcept : answer_(answer) {}

    [[nodiscard]] pt::Color value(pt::Float u, pt::Float v, const pt::Point3& p) const override {
        last_u_ = u;
        last_v_ = v;
        last_p_ = p;
        ++calls_;
        return answer_;
    }

    [[nodiscard]] pt::Float last_u() const noexcept { return last_u_; }
    [[nodiscard]] pt::Float last_v() const noexcept { return last_v_; }
    [[nodiscard]] const pt::Point3& last_p() const noexcept { return last_p_; }
    [[nodiscard]] int calls() const noexcept { return calls_; }

private:
    pt::Color answer_;

    // value() is const on the interface, so the bookkeeping has to be mutable.
    mutable pt::Float last_u_{};
    mutable pt::Float last_v_{};
    mutable pt::Point3 last_p_{};
    mutable int calls_ = 0;
};

} // namespace pt_test
