#pragma once
#include "pt/math/ray.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"

namespace pt {

struct CameraSettings;

class Camera {
public:
    Camera(const CameraSettings& settings, int image_width, int image_height);

    [[nodiscard]] Ray generate_ray(int i, int j, const Vec3& pixel_offset, Sampler& sampler) const { // pixel_offset -> [-0.5, 0.5]²
        const Float sample_u = static_cast<Float>(i) + pixel_offset.x();
        const Float sample_v = static_cast<Float>(j) + pixel_offset.y();

        const Point3 sample_point = pixel00_loc_ + sample_u * pixel_delta_u_ + sample_v * pixel_delta_v_;
        const Point3 origin = defocus_enabled_ ? defocus_disk_sample(sampler) : center_;

        const Float ray_time = sampler.next_scalar();
        return Ray(origin, sample_point - origin, ray_time);
    }

private:
    Point3 center_;
    Point3 pixel00_loc_;
    Vec3 pixel_delta_u_;
    Vec3 pixel_delta_v_;
    Vec3 defocus_disk_u_;
    Vec3 defocus_disk_v_;
    bool defocus_enabled_{};

    [[nodiscard]] Point3 defocus_disk_sample(Sampler& sampler) const {
        const Vec3 p = random_in_unit_disk(sampler);
        return center_ + p.x() * defocus_disk_u_ + p.y() * defocus_disk_v_;
    }
};

} // namespace pt
