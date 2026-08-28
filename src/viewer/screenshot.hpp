#pragma once
#include "pt/io/image_format.hpp"

namespace pt {

class Film;

// Writes into out/ with a timestamped name. Failures are logged, not reported.
void save_screenshot(const Film& film, ImageFormat format);

} // namespace pt
