#include "bvh.hpp"
#include "camera.hpp"
#include "checker_texture.hpp"
#include "hittable_list.hpp"
#include "image_texture.hpp"
#include "noise_texture.hpp"
#include "lambertian.hpp"
#include "material.hpp"
#include "metal.hpp"
#include "dielectric.hpp"
#include "quad.hpp"
#include "random.hpp"
#include "solid_color.hpp"
#include "sphere.hpp"
#include "texture.hpp"
#include "vec3.hpp"
#include <chrono>
#include <cmath>
#include <iostream>
#include <memory>
#include <vector>

void bouncing_spheres();
void checkered_spheres();
void earth();
void perlin_spheres();
void quads();

int main() {
    switch (5) {
        case 1: bouncing_spheres(); break;
        case 2: checkered_spheres(); break;
        case 3: earth(); break;
        case 4: perlin_spheres(); break;
        case 5: quads(); break;
    }
}

void bouncing_spheres() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<solid_color>(color(0.5, 0.5, 0.5))); // ground
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, -1000, 0.0), 1000.0, materials.back().get()));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            const auto choose_mat = random_double();

            const point3 center(a + 0.9 * random_double(), 0.2, b + 0.9 * random_double());
            if ((center - point3(4.0, 0.2, 0.0)).length() > 0.9) {
                if (choose_mat < 0.8) { // %80 diffuse
                    const auto albedo = color::random() * color::random();
                    textures.push_back(std::make_unique<solid_color>(albedo));
                    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
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

    textures.push_back(std::make_unique<solid_color>(color(0.4, 0.2, 0.1)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
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
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void checkered_spheres() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<checker_texture>(0.32, color(0.2, 0.3, 0.1), color(0.9, 0.9, 0.9)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, -10.0, 0.0), 10.0, materials.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, 10.0, 0.0), 10.0, materials.back().get()));

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
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void earth() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<image_texture>("earthmap.jpg"));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, 0.0, 0.0), 2.0, materials.back().get()));

    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;
    cam.lookfrom = point3(0.0, 0.0, 12.0);
    cam.lookat = point3(0.0, 0.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void perlin_spheres() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<noise_texture>(4.0));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

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
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void quads() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<solid_color>(color(1.0, 0.2, 0.2)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(-3.0, -2.0, 5.0), vec3(0.0, 0.0, -4.0), vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<solid_color>(color(0.2, 1.0, 0.2)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(-2.0, -2.0, 0.0), vec3(4.0, 0.0, 0.0), vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<solid_color>(color(0.2, 0.2, 1.0)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(3.0, -2.0, 1.0), vec3(0.0, 0.0, 4.0), vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<solid_color>(color(1.0, 0.5, 0.0)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(-2.0, 3.0, 1.0), vec3(4.0, 0.0, 0.0), vec3(0.0, 0.0, 4.0), materials.back().get()));

    textures.push_back(std::make_unique<solid_color>(color(0.2, 0.8, 0.8)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(-2.0, -3.0, 5.0), vec3(4.0, 0.0, 0.0), vec3(0.0, 0.0, -4.0), materials.back().get()));

    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 80;
    cam.lookfrom = point3(0.0, 0.0, 9.0);
    cam.lookat = point3(0.0, 0.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}