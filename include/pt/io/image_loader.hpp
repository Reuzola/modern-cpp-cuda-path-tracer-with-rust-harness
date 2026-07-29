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
        return fdata_ ? image_width_ : 0;
    }

    [[nodiscard]] int height() const {
        return fdata_ ? image_height_ : 0;
    }

    [[nodiscard]] Color pixel_data(int x, int y) const;

private:
    std::unique_ptr<float, StbiDeleter> fdata_;
    int image_width_{};
    int image_height_{};

    bool load(const std::string& filename);
};

} // namespace pt
