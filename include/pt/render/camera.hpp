#pragma once
#include "pt/core/hittable.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/sampling/importance_targets.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

namespace pt {

class Camera {
public:
    Float aspect_ratio{1.0_f};
    int image_width{100};
    int samples_per_pixel{10};
    int max_depth{10};
    Float vfov{90.0_f};
    Point3 lookfrom{0, 0, 0};
    Point3 lookat{0, 0, -1};
    Vec3 vup{0, 1, 0};
    Float defocus_angle{0};
    Float focus_dist{10.0_f};
    Color background{};

    [[nodiscard]] Film render(const Hittable& world, const ImportanceTargets& targets) {
        initialize();

        Film film(image_width, image_height_);
        const PathIntegrator integrator(world, targets, background, max_depth);

        for (int j = 0; j < image_height_; j++) {
            std::clog << "\rScanlines remaining: " << (image_height_ - j) << ' ' << std::flush;

            for (int i = 0; i < image_width; i++) {
                Color pixel_color(0, 0, 0);

                for (int s_j = 0; s_j < sqrt_spp_; s_j++) {
                    for (int s_i = 0; s_i < sqrt_spp_; s_i++) {
                        pixel_color += integrator.radiance(get_ray(i, j, s_i, s_j));
                    }
                }

                film.set_pixel(i, j, pixel_samples_scale_ * pixel_color);
            }
        }

        std::clog << "\rDone.                 \n";
        return film;
    }

private:
    int image_height_{};
    Point3 center_;
    Point3 pixel00_loc_;
    Vec3 pixel_delta_u_;
    Vec3 pixel_delta_v_;
    Float pixel_samples_scale_{};
    int sqrt_spp_{};
    Float recip_sqrt_spp_{};
    Vec3 u_, v_, w_;
    Vec3 defocus_disk_u_;
    Vec3 defocus_disk_v_;

    void initialize() {
        image_height_ = std::max(static_cast<int>(static_cast<Float>(image_width) / aspect_ratio), 1);
        center_ = lookfrom;

        sqrt_spp_ = static_cast<int>(std::sqrt(static_cast<Float>(samples_per_pixel)));
        recip_sqrt_spp_ = 1.0_f / static_cast<Float>(sqrt_spp_);

        pixel_samples_scale_ = 1.0_f / static_cast<Float>(sqrt_spp_ * sqrt_spp_);

        w_ = unit_vector(lookfrom - lookat);
        u_ = unit_vector(cross(vup, w_));
        v_ = cross(w_, u_);

        const Float theta = degrees_to_radians(vfov);
        const Float h = std::tan(theta / 2.0_f);

        const Float viewport_height = 2.0_f * h * focus_dist;
        const Float viewport_width = viewport_height * (static_cast<Float>(image_width) / static_cast<Float>(image_height_));

        const Vec3 viewport_u = viewport_width * u_;
        const Vec3 viewport_v = viewport_height * (-v_);

        pixel_delta_u_ = viewport_u / static_cast<Float>(image_width);
        pixel_delta_v_ = viewport_v / static_cast<Float>(image_height_);

        const Vec3 viewport_upper_left = center_ - (focus_dist * w_) - viewport_u / 2.0_f - viewport_v / 2.0_f;

        pixel00_loc_ = viewport_upper_left + 0.5_f * (pixel_delta_u_ + pixel_delta_v_);

        const Float defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.0_f));

        defocus_disk_u_ = u_ * defocus_radius;
        defocus_disk_v_ = v_ * defocus_radius;
    }

    [[nodiscard]] Vec3 sample_square_stratified(int s_i, int s_j) const {
        const Float px = ((static_cast<Float>(s_i) + random_scalar()) * recip_sqrt_spp_) - 0.5_f;
        const Float py = ((static_cast<Float>(s_j) + random_scalar()) * recip_sqrt_spp_) - 0.5_f;
        return Vec3(px, py, 0.0_f);
    }

    [[nodiscard]] Ray get_ray(int i, int j, int s_i, int s_j) const {
        const Vec3 offset = sample_square_stratified(s_i, s_j);

        const Point3 sample_point = pixel00_loc_ + (static_cast<Float>(i) + offset.x()) * pixel_delta_u_ + (static_cast<Float>(j) + offset.y()) * pixel_delta_v_;
        const Point3 origin = (defocus_angle <= 0) ? center_ : defocus_disk_sample();

        const Float ray_time = random_scalar();
        return Ray(origin, sample_point - origin, ray_time);
    }

    [[nodiscard]] Point3 defocus_disk_sample() const {
        const auto p = random_in_unit_disk();
        return center_ + p.x() * defocus_disk_u_ + p.y() * defocus_disk_v_;
    }
};

} // namespace pt
