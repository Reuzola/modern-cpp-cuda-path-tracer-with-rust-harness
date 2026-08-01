// Single translation unit responsible for emitting the stb libraries implementation.
// This file is intentionally isolated and contains no other includes, code, or namespaces.

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h" // IWYU pragma: keep

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h" // IWYU pragma: keep
