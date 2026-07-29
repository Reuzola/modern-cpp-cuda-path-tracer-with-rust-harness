#pragma once
#include "pt/io/image_loader.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <string>

namespace pt {

class ImageTexture final : public Texture {
public:
    explicit ImageTexture(const std::string& filename) : image_(filename) {}

    [[nodiscard]] Color value(Float u, Float v, const Point3& p) const override;

private:
    ImageLoader image_;
};

} // namespace pt
