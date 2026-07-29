#include "pt/io/image_loader.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "stb_image.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace pt {

void StbiDeleter::operator()(float* ptr) const { stbi_image_free(ptr); }

ImageLoader::ImageLoader(const std::string& filename) {
    const bool success = load(filename);
    if (!success) std::cerr << "ERROR: Could not load image file '" << filename << "'.\n";
}

Color ImageLoader::pixel_data(int x, int y) const {
    x = std::clamp(x, 0, image_width_ - 1);
    y = std::clamp(y, 0, image_height_ - 1);

    const int index = (y * image_width_ + x) * 3;
    const Float r = static_cast<Float>(fdata_.get()[index]);
    const Float g = static_cast<Float>(fdata_.get()[index + 1]);
    const Float b = static_cast<Float>(fdata_.get()[index + 2]);

    return Color(r, g, b);
}

bool ImageLoader::load(const std::string& filename) {
    constexpr int desired_channels = 3;
    int dummy{};

    float* raw_ptr = stbi_loadf(filename.c_str(), &image_width_, &image_height_, &dummy, desired_channels);
    if (raw_ptr == nullptr) return false;
    fdata_.reset(raw_ptr);
    return true;
}

} // namespace pt
