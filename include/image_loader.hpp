#pragma once
#include <algorithm>
#include <iostream>
#include <memory>
#include <string>
#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include "vec3.hpp"

struct stbi_deleter {
    void operator()(float* ptr) const { stbi_image_free(ptr); }
};

class image_loader {
    public:
        explicit image_loader(const std::string& filename) {
            const bool success = load(filename);
            if (!success) std::cerr << "ERROR: Could not load image file '" << filename << "'.\n";
        }

        [[nodiscard]] int width() const {
            return fdata ? image_width : 0;
        }

        [[nodiscard]] int height() const {
            return fdata ? image_height : 0;
        }

        [[nodiscard]] color pixel_data(int x, int y) const {
            x = std::clamp(x, 0, image_width - 1);
            y = std::clamp(y, 0, image_height - 1);

            const int index = (y * image_width + x) * 3;
            const float r = fdata.get()[index];
            const float g = fdata.get()[index + 1];
            const float b = fdata.get()[index + 2];

            return color(r, g, b);
        }
    private:
        std::unique_ptr<float, stbi_deleter> fdata;
        int image_width{};
        int image_height{};

        bool load(const std::string& filename) {
            constexpr int desired_channels = 3;
            int dummy{};

            float* raw_ptr = stbi_loadf(filename.c_str(), &image_width, &image_height, &dummy, desired_channels);
            if (raw_ptr == nullptr) return false;
            fdata.reset(raw_ptr);
            return true;
        }
};