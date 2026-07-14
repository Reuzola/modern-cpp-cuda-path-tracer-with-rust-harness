#pragma once
#include "hit_record.hpp"
#include "hittable.hpp"
#include "interval.hpp"
#include "constants.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include "color.hpp"
#include "random.hpp"
#include "material.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>

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

        void render(const hittable& world) {
            initialize();

            std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

            for (int j = 0; j < image_height; j++) {
                std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;

                for (int i = 0; i < image_width; i++) {
                    color pixel_color(0, 0, 0);

                    for(int s = 0; s < samples_per_pixel; s++) {
                        pixel_color += ray_color(get_ray(i, j), max_depth, world);
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
        vec3 u, v, w;

        void initialize() {
            image_height = std::max(static_cast<int>(image_width / aspect_ratio), 1);
            center = lookfrom;

            pixel_samples_scale = 1.0 / samples_per_pixel;

            const auto focal_length = (lookfrom - lookat).length();

            w = unit_vector(lookfrom - lookat);
            u = unit_vector(cross(vup, w));
            v = cross(w, u);

            const auto theta = degrees_to_radians(vfov);
            const auto h = std::tan(theta / 2.0);

            const auto viewport_height = 2.0 * h * focal_length;
            const auto viewport_width = viewport_height * (static_cast<double>(image_width) / image_height);

            const auto viewport_u = viewport_width * u;
            const auto viewport_v = viewport_height * (-v);

            pixel_delta_u = viewport_u / image_width;
            pixel_delta_v = viewport_v / image_height;

            const auto viewport_upper_left = center - (focal_length * w) - viewport_u / 2.0 - viewport_v / 2.0;

            pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
        }

        [[nodiscard]] color ray_color(const ray& r, int depth, const hittable& world) const {
            if (depth <= 0) return color(0, 0, 0);

            hit_record rec;

            if (world.hit(r, interval(0.001, infinity), rec)) {
                if (auto sr = rec.mat->scatter(r, rec)) {
                    return sr->attenuation * ray_color(sr->scattered, depth - 1, world);
                }

                return color(0.0, 0.0, 0.0);
            }
            
            const vec3 unit_direction = unit_vector(r.direction());
            const auto a = 0.5 * (unit_direction.y() + 1.0);
            return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
        }

        [[nodiscard]] vec3 sample_square() const {
            return vec3(random_double() - 0.5, random_double() - 0.5, 0.0);
        }

        [[nodiscard]] ray get_ray(int i, int j) const {
            const vec3 offset = sample_square();

            point3 sample_point = pixel00_loc + (i + offset.x()) * pixel_delta_u + (j + offset.y()) * pixel_delta_v;

            return ray(center, sample_point - center);
        }
};