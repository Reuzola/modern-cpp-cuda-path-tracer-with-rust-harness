#pragma once
#include "hit_record.hpp"
#include "hittable.hpp"
#include "hittable_pdf.hpp"
#include "interval.hpp"
#include "constants.hpp"
#include "mixture_pdf.hpp"
#include "overloaded.hpp"
#include "pdf.hpp"
#include "pdf_variant.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include "color.hpp"
#include "random.hpp"
#include "material.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <variant>

class camera {
    public:
        double aspect_ratio{1.0};
        int image_width{100};
        int samples_per_pixel{10};
        int max_depth{10};
        double vfov{90.0};
        point3 lookfrom{0, 0, 0};
        point3 lookat{0, 0, -1};
        vec3 vup{0, 1 ,0};
        double defocus_angle{0};
        double focus_dist{10};
        color background{};

        void render(const hittable& world, const hittable* lights = nullptr) {
            initialize();

            std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

            for (int j = 0; j < image_height; j++) {
                std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;

                for (int i = 0; i < image_width; i++) {
                    color pixel_color(0, 0, 0);

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
        point3 center;
        point3 pixel00_loc;
        vec3 pixel_delta_u;
        vec3 pixel_delta_v;
        double pixel_samples_scale{};
        int sqrt_spp{};
        double recip_sqrt_spp{};
        vec3 u, v, w;
        vec3 defocus_disk_u;
        vec3 defocus_disk_v;

        void initialize() {
            image_height = std::max(static_cast<int>(image_width / aspect_ratio), 1);
            center = lookfrom;

            sqrt_spp = static_cast<int>(std::sqrt(samples_per_pixel));
            recip_sqrt_spp = 1.0 / sqrt_spp;

            pixel_samples_scale = 1.0 / (sqrt_spp * sqrt_spp);

            w = unit_vector(lookfrom - lookat);
            u = unit_vector(cross(vup, w));
            v = cross(w, u);

            const auto theta = degrees_to_radians(vfov);
            const auto h = std::tan(theta / 2.0);

            const auto viewport_height = 2.0 * h * focus_dist;
            const auto viewport_width = viewport_height * (static_cast<double>(image_width) / image_height);

            const auto viewport_u = viewport_width * u;
            const auto viewport_v = viewport_height * (-v);

            pixel_delta_u = viewport_u / image_width;
            pixel_delta_v = viewport_v / image_height;

            const auto viewport_upper_left = center - (focus_dist * w) - viewport_u / 2.0 - viewport_v / 2.0;

            pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);

            const auto defocus_radius = focus_dist * std::tan(degrees_to_radians(defocus_angle / 2.0));

            defocus_disk_u = u * defocus_radius;
            defocus_disk_v = v * defocus_radius;
        }

        [[nodiscard]] color ray_color(const ray& r, int depth, const hittable& world, const hittable* lights) const {
            if (depth <= 0) return color(0, 0, 0);

            hit_record rec;

            if (!world.hit(r, interval(0.001, infinity), rec)) return background;

            const color color_from_emission = rec.mat->emitted(r, rec);

            if (const auto sr = rec.mat->scatter(r, rec)) {
                const auto shade = [&](const pdf& p) -> color {
                    const ray scattered(rec.p, p.generate(), r.time());
                    const double pdf_value = p.value(scattered.direction());
                    
                    const double scattering_pdf = rec.mat->scattering_pdf(r, rec, scattered);
                    const color sample_color = ray_color(scattered, depth - 1, world, lights);

                    return (sr->attenuation * scattering_pdf * sample_color) / pdf_value;
                };

                return color_from_emission + std::visit(overloaded{
                    [&](const specular_bounce& sb) -> color {
                        return sr->attenuation * ray_color(sb.scattered, depth - 1, world, lights);
                    },
                    [&](const diffuse_bounce& db) -> color {
                        const auto& surface_pdf = as_pdf(db.sampling_pdf);

                        if (lights == nullptr) return shade(surface_pdf);

                        const hittable_pdf light_pdf(*lights, rec.p);
                        const mixture_pdf mixed_pdf(surface_pdf, light_pdf);
                        return shade(mixed_pdf);
                    }
                }, sr->bounce);
            }
            return color_from_emission;
        }

        [[nodiscard]] vec3 sample_square_stratified(int s_i, int s_j) const {
            const double px = ((s_i + random_double()) * recip_sqrt_spp) - 0.5;
            const double py = ((s_j + random_double()) * recip_sqrt_spp) - 0.5;
            return vec3(px, py, 0.0);
        }

        [[nodiscard]] ray get_ray(int i, int j, int s_i, int s_j) const {
            const vec3 offset = sample_square_stratified(s_i, s_j);

            const point3 sample_point = pixel00_loc + (i + offset.x()) * pixel_delta_u + (j + offset.y()) * pixel_delta_v;
            const point3 origin = (defocus_angle <= 0) ? center : defocus_disk_sample();

            const double ray_time = random_double();
            return ray(origin, sample_point - origin, ray_time); 
        }

        [[nodiscard]] point3 defocus_disk_sample() const {
            const auto p = random_in_unit_disk();
            return center + p.x() * defocus_disk_u + p.y() * defocus_disk_v;
        } 
};