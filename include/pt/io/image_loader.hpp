#pragma once
#include "pt/math/color.hpp"
#include <memory>
#include <string>

namespace pt {

struct StbiDeleter {
    void operator()(float* ptr) const;
};

class ImageLoader {
public:
    explicit ImageLoader(const std::string& filename);

    [[nodiscard]] int width() const {
        return fdata ? image_width : 0;
    }

    [[nodiscard]] int height() const {
        return fdata ? image_height : 0;
    }

    [[nodiscard]] Color pixel_data(int x, int y) const;

private:
    std::unique_ptr<float, StbiDeleter> fdata;
    int image_width{};
    int image_height{};

    bool load(const std::string& filename);
};

} // namespace pt
