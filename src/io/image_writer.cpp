#include "pt/io/image_writer.hpp"
#include "pt/io/exr_writer.hpp"
#include "pt/io/image_format.hpp"
#include "pt/io/png_writer.hpp"
#include "pt/io/ppm_writer.hpp"
#include <memory>

namespace pt {

std::unique_ptr<ImageWriter> make_image_writer(ImageFormat format) {
    switch (format) {
    case ImageFormat::ppm:
        return std::make_unique<PpmWriter>();
    case ImageFormat::png:
        return std::make_unique<PngWriter>();
    case ImageFormat::exr:
        return std::make_unique<ExrWriter>();
    }
    // No 'default' enables compiler warnings for missing enum cases; invalid casted ints reach here.
    return std::make_unique<PpmWriter>();
}

} // namespace pt
