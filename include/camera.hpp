#pragma once
#include "hit_record.hpp"
#include "hittable.hpp"
#include "interval.hpp"
#include "constants.hpp"
#include "ray.hpp"
#include "vec3.hpp"
#include "color.hpp"
#include <algorithm>
#include <iostream>

class camera {
    public:
        double aspect_ratio{1.0};
        int image_width{100};

        void render(const hittable& world) {
            initialize();

            std::cout << "P3\n" << image_width << ' ' << image_height << "\n255\n";

            for (int j = 0; j < image_height; j++) {
                std::clog << "\rScanlines remaining: " << (image_height - j) << ' ' << std::flush;

                for (int i = 0; i < image_width; i++) {
                    auto pixel_center = pixel00_loc + (i * pixel_delta_u) + (j * pixel_delta_v);
                    auto ray_direction = pixel_center - center;
                    ray r(center, ray_direction);

                    color pixel_color = ray_color(r, world);
                    write_color(std::cout, pixel_color);
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

        void initialize() {
            image_height = std::max(static_cast<int>(image_width / aspect_ratio), 1);
            center = point3(0, 0, 0);

            const auto viewport_height = 2.0;
            const auto viewport_width = viewport_height * (static_cast<double>(image_width) / image_height);

            const auto focal_length = 1.0;

            const auto viewport_u = vec3(viewport_width, 0, 0);
            const auto viewport_v = vec3(0, -viewport_height, 0);

            pixel_delta_u = viewport_u / image_width;
            pixel_delta_v = viewport_v / image_height;

            const auto viewport_upper_left = center - vec3(0, 0, focal_length) - viewport_u / 2 - viewport_v / 2;

            pixel00_loc = viewport_upper_left + 0.5 * (pixel_delta_u + pixel_delta_v);
        }

        [[nodiscard]] color ray_color(const ray& r, const hittable& world) const {
            hit_record rec;

            if (world.hit(r, interval(0, infinity), rec)) {
                return color(0.5 * (rec.normal + color(1, 1, 1)));
            }
            
            const vec3 unit_direction = unit_vector(r.direction());
            const auto a = 0.5 * (unit_direction.y() + 1.0);
            return (1.0 - a) * color(1.0, 1.0, 1.0) + a * color(0.5, 0.7, 1.0);
        }
};