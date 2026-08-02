#pragma once

namespace pt {

enum class ImageFormat { ppm,
                         png,
                         exr };

[[nodiscard]] constexpr bool is_hdr(ImageFormat format) noexcept {
    switch (format) {
    case ImageFormat::ppm:
    case ImageFormat::png:
        return false;
    case ImageFormat::exr:
        return true;
    }
    // No 'default': a new enumerator must break this switch, not silently fall through to "LDR".
    return false;
}

} // namespace pt
