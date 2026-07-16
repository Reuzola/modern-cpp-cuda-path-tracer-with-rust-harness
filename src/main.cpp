#include "bvh.hpp"
#include "camera.hpp"
#include "hittable_list.hpp"
#include "lambertian.hpp"
#include "material.hpp"
#include "metal.hpp"
#include "dielectric.hpp"
#include "random.hpp"
#include "sphere.hpp"
#include "vec3.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

int main() {
    hittable_list world;

    std::vector<std::unique_ptr<material>> materials;

    materials.push_back(std::make_unique<lambertian>(color(0.5, 0.5, 0.5))); // ground
    world.add(std::make_shared<sphere>(point3(0.0, -1000, 0.0), 1000.0, materials.back().get()));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            const auto choose_mat = random_double();

            const point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());
            if ((center - point3(4.0, 0.2, 0.0)).length() > 0.9) {
                if (choose_mat < 0.8) { // %80 diffuse
                    const auto albedo = color::random() * color::random();
                    materials.push_back(std::make_unique<lambertian>(albedo));
                    const auto center2 = center + vec3(0.0, random_double(0.0, 0.5), 0.0);
                    world.add(std::make_shared<sphere>(center, center2, 0.2, materials.back().get()));
                } else if (choose_mat < 0.95) { // %15 metal
                    const auto albedo = color::random(0.5, 1.0);
                    const auto fuzz = random_double(0.0, 0.5);
                    materials.push_back(std::make_unique<metal>(albedo, fuzz));
                    world.add(std::make_shared<sphere>(center, 0.2, materials.back().get()));
                } else { // %5 glass
                    materials.push_back(std::make_unique<dielectric>(1.5));
                    world.add(std::make_shared<sphere>(center, 0.2, materials.back().get()));
                }
            }
        }
    }

    materials.push_back(std::make_unique<dielectric>(1.5));
    world.add(std::make_shared<sphere>(point3(0.0, 1.0, 0.0), 1.0, materials.back().get()));

    materials.push_back(std::make_unique<lambertian>(color(0.4, 0.2, 0.1)));
    world.add(std::make_shared<sphere>(point3(-4.0, 1.0, 0.0), 1.0, materials.back().get()));

    materials.push_back(std::make_unique<metal>(color(0.7, 0.6, 0.5), 0.0));
    world.add(std::make_shared<sphere>(point3(4.0, 1.0, 0.0), 1.0, materials.back().get()));
    
    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;
    cam.lookfrom = point3(13.0, 2.0, 3.0);
    cam.lookat = point3(0.0, 0.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.focus_dist = 10.0;
    cam.defocus_angle = 0.6;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
    
    return 0;
}