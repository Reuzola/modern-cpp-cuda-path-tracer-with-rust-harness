#pragma once
#include "pt/math/vec3.hpp"
#include <memory>
#include <string>

namespace pt {

struct stbi_deleter {
    void operator()(float* ptr) const;
};

class image_loader {
public:
    explicit image_loader(const std::string& filename);

    [[nodiscard]] int width() const {
        return fdata ? image_width : 0;
    }

    [[nodiscard]] int height() const {
        return fdata ? image_height : 0;
    }

    [[nodiscard]] color pixel_data(int x, int y) const;

private:
    std::unique_ptr<float, stbi_deleter> fdata;
    int image_width{};
    int image_height{};

    bool load(const std::string& filename);
};

} // namespace pt
