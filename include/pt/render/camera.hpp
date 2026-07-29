#pragma once
#include "pt/core/hit_record.hpp"
#include "pt/core/hittable.hpp"
#include "pt/io/color.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/constants.hpp"
#include "pt/math/interval.hpp"
#include "pt/math/random.hpp"
#include "pt/math/ray.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/sampling/hittable_pdf.hpp"
#include "pt/sampling/mixture_pdf.hpp"
#include "pt/sampling/pdf.hpp"
#include "pt/sampling/pdf_variant.hpp"
#include "pt/util/overloaded.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <variant>

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

    void render(const Hittable& world, const Hittable* lights = nullptr) {
        initialize();

        std::cout << "P3\n"
                  << image_width << ' ' << image_height << "\n255\n";

        for (int j = 0; j < image_height; j++) {
            std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;

            for (int i = 0; i < image_width; i++) {
                Color pixel_color(0, 0, 0);

                for (int s_j = 0; s_j < sqrt_spp; s_j++) {
                    for (int s_i = 0; s_i < sqrt_spp; s_i++) {
                        pixel_color += ray_color(get_ray(i, j, s_i, s_j), max_depth, world, lights);
                    }
                }

                write_color(std::cout, pixel_samples_scale * pixel_color);
            }
        }

        std::clog << "\rDone.                 \n";
    }

private:
    int image_height{};
    Point3 center;
    Point3 pixel00_loc;
    Vec3 pixel_delta_u;
    Vec3 pixel_delta_v;
    Float pixel_samples_scale{};
    int sqrt_spp{};
    Float recip_sqrt_spp{};
    Vec3 u, v, w;
    Vec3 defocus_disk_u;
    Vec3 defocus_disk_v;

    void initialize() {
        image_height = std::max(static_cast<int>(static_cast<Float>(image_width) / aspect_ratio), 1);
        center = lookfrom;

        sqrt_spp = static_cast<int>(std::sqrt(static_cast<Float>(samples_per_pixel)));
        recip_sqrt_spp = 1.0_f / static_cast<Float>(sqrt_spp);

        pixel_samples_scale = 1.0_f / static_cast<Float>(sqrt_spp * sqrt_spp);

        w = unit_vector(lookfrom - lookat);
        u = unit_vector(cross(vup, w));
        v = cross(w, u);

        const Float theta = degrees_to_radians(vfov);
        const Float h = std::tan(theta / 2.0_f);

        const Float viewport_height = 2.0_f * h * focus_dist;
        const Float viewport_width = viewport_height * (static_cast<Float>(image_width) / static_cast<Float>(image_height));

        const Vec3 viewport_u = viewport_width * u;
        const Vec3 viewport_v = viewport_height * (-v);

        pixel_delta_u = viewport_u / static_cast<Float>(image_width);
        pixel_delta_v = viewport_v / static_cast<Float>(image_height);

        const Vec3 viewport_upper_left = center - (focus_dist * w) - viewport_u / 2.0_f - viewport_v / 2.0_f;

        pixel00_loc = viewport_upper_left + 0.5_f * (pixel_delta_u + pixel_delta_v);

        const Float defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.0_f));

        defocus_disk_u = u * defocus_radius;
        defocus_disk_v = v * defocus_radius;
    }

    [[nodiscard]] Color ray_color(const Ray& r, int depth, const Hittable& world, const Hittable* lights) const {
        if (depth <= 0) return Color(0, 0, 0);

        HitRecord rec;

        if (!world.hit(r, Interval(0.001_f, infinity), rec)) return background;

        const Color color_from_emission = rec.mat->emitted(r, rec);

        if (const auto sr = rec.mat->scatter(r, rec)) {
            const auto shade = [&](const Pdf& p) -> Color {
                const Ray scattered(rec.p, p.generate(), r.time());
                const Float pdf_value = p.value(scattered.direction());

                const Float scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
                const Color sample_color = ray_color(scattered, depth - 1, world, lights);

                return (sr->attenuation * scattering_pdf * sample_color) / pdf_value;
            };

            // clang-format off
            return color_from_emission + std::visit(Overloaded{
                [&](const SpecularBounce& sb) -> Color {
                    return sr->attenuation * ray_color(sb.scattered, depth - 1, world, lights);
                },
                [&](const DiffuseBounce& db) -> Color {
                    const auto& surface_pdf = as_pdf(db.sampling_pdf);

                    if (lights == nullptr) return shade(surface_pdf);

                    const HittablePdf light_pdf(*lights, rec.p);
                    const MixturePdf mixed_pdf(surface_pdf, light_pdf);
                    return shade(mixed_pdf);
                }
            }, sr->bounce);
            // clang-format on
        }
        return color_from_emission;
    }

    [[nodiscard]] Vec3 sample_square_stratified(int s_i, int s_j) const {
        const Float px = ((static_cast<Float>(s_i) + random_scalar()) * recip_sqrt_spp) - 0.5_f;
        const Float py = ((static_cast<Float>(s_j) + random_scalar()) * recip_sqrt_spp) - 0.5_f;
        return Vec3(px, py, 0.0_f);
    }

    [[nodiscard]] Ray get_ray(int i, int j, int s_i, int s_j) const {
        const Vec3 offset = sample_square_stratified(s_i, s_j);

        const Point3 sample_point = pixel00_loc + (static_cast<Float>(i) + offset.x()) * pixel_delta_u + (static_cast<Float>(j) + offset.y()) * pixel_delta_v;
        const Point3 origin = (defocus_angle <= 0) ? center : defocus_disk_sample();

        const Float ray_time = random_scalar();
        return Ray(origin, sample_point - origin, ray_time);
    }

    [[nodiscard]] Point3 defocus_disk_sample() const {
        const auto p = random_in_unit_disk();
        return center + p.x() * defocus_disk_u + p.y() * defocus_disk_v;
    }
};

} // namespace pt
