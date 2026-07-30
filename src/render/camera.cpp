#include "pt/render/camera.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/scene/scene.hpp"
#include <cmath>

namespace pt {

Camera::Camera(const CameraSettings& settings, int image_width, int image_height) {
    center_ = settings.lookfrom;
    defocus_enabled_ = !(settings.defocus_angle <= 0);

    const Vec3 w = unit_vector(settings.lookfrom - settings.lookat);
    const Vec3 u = unit_vector(cross(settings.vup, w));
    const Vec3 v = cross(w, u);

    const Float theta = degrees_to_radians(settings.vfov);
    const Float h = std::tan(theta / 2.0_f);

    const Float viewport_height = 2.0_f * h * settings.focus_dist;
    const Float viewport_width = viewport_height * (static_cast<Float>(image_width) / static_cast<Float>(image_height));

    const Vec3 viewport_u = viewport_width * u;
    const Vec3 viewport_v = viewport_height * (-v);

    pixel_delta_u_ = viewport_u / static_cast<Float>(image_width);
    pixel_delta_v_ = viewport_v / static_cast<Float>(image_height);

    const Point3 viewport_upper_left = center_ - (settings.focus_dist * w) - viewport_u / 2.0_f - viewport_v / 2.0_f;
    pixel00_loc_ = viewport_upper_left + 0.5_f * (pixel_delta_u_ + pixel_delta_v_);

    const Float defocus_radius = settings.focus_dist * std::tan(degrees_to_radians(settings.defocus_angle / 2.0_f));
    defocus_disk_u_ = u * defocus_radius;
    defocus_disk_v_ = v * defocus_radius;
}

} // namespace pt
