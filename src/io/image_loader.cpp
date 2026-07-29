#include "pt/io/image_loader.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "stb_image.h"
#include <algorithm>
#include <iostream>
#include <string>

namespace pt {

void stbi_deleter::operator()(float* ptr) const { stbi_image_free(ptr); }

image_loader::image_loader(const std::string& filename) {
    const bool success = load(filename);
    if (!success) std::cerr << "ERROR: Could not load image file '" << filename << "'.\n";
}

color image_loader::pixel_data(int x, int y) const {
    x = std::clamp(x, 0, image_width - 1);
    y = std::clamp(y, 0, image_height - 1);

    const int index = (y * image_width + x) * 3;
    const Float r = static_cast<Float>(fdata.get()[index]);
    const Float g = static_cast<Float>(fdata.get()[index + 1]);
    const Float b = static_cast<Float>(fdata.get()[index + 2]);

    return color(r, g, b);
}

bool image_loader::load(const std::string& filename) {
    constexpr int desired_channels = 3;
    int dummy{};

    float* raw_ptr = stbi_loadf(filename.c_str(), &image_width, &image_height, &dummy, desired_channels);
    if (raw_ptr == nullptr) return false;
    fdata.reset(raw_ptr);
    return true;
}

} // namespace pt
