#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/rotate_y.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/translate.hpp"
#include "pt/materials/dielectric.hpp"
#include "pt/materials/diffuse_light.hpp"
#include "pt/materials/isotropic.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/material.hpp"
#include "pt/materials/metal.hpp"
#include "pt/math/color.hpp"
#include "pt/math/random.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/camera.hpp"
#include "pt/textures/checker_texture.hpp"
#include "pt/textures/image_texture.hpp"
#include "pt/textures/noise_texture.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/textures/texture.hpp"
#include <chrono>
#include <format>
#include <iostream>
#include <memory>
#include <vector>

void bouncing_spheres();
void checkered_spheres();
void earth();
void perlin_spheres();
void quads();
void simple_light();
void cornell_box();
void cornell_smoke();
void final_scene(int image_width, int samples_per_pixel, int max_depth);

int main() {
    switch (7) {
    case 1:
        bouncing_spheres();
        break;
    case 2:
        checkered_spheres();
        break;
    case 3:
        earth();
        break;
    case 4:
        perlin_spheres();
        break;
    case 5:
        quads();
        break;
    case 6:
        simple_light();
        break;
    case 7:
        cornell_box();
        break;
    case 8:
        cornell_smoke();
        break;
    case 9:
        final_scene(400, 250, 4);
        break;
    }
}

void bouncing_spheres() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.5, 0.5, 0.5))); // ground
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, -1000, 0.0), 1000.0, materials.back().get()));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            const auto choose_mat = pt::random_double();

            const pt::point3 center(a + 0.9 * pt::random_double(), 0.2, b + 0.9 * pt::random_double());
            if ((center - pt::point3(4.0, 0.2, 0.0)).length() > 0.9) {
                if (choose_mat < 0.8) { // %80 diffuse
                    const auto albedo = pt::color::random() * pt::color::random();
                    textures.push_back(std::make_unique<pt::solid_color>(albedo));
                    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
                    const auto center2 = center + pt::vec3(0.0, pt::random_double(0.0, 0.5), 0.0);
                    world.add(std::make_shared<pt::sphere>(center, center2, 0.2, materials.back().get()));
                } else if (choose_mat < 0.95) { // %15 pt::metal
                    const auto albedo = pt::color::random(0.5, 1.0);
                    const auto fuzz = pt::random_double(0.0, 0.5);
                    materials.push_back(std::make_unique<pt::metal>(albedo, fuzz));
                    world.add(std::make_shared<pt::sphere>(center, 0.2, materials.back().get()));
                } else { // %5 glass
                    materials.push_back(std::make_unique<pt::dielectric>(1.5));
                    world.add(std::make_shared<pt::sphere>(center, 0.2, materials.back().get()));
                }
            }
        }
    }

    materials.push_back(std::make_unique<pt::dielectric>(1.5));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 1.0, 0.0), 1.0, materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.4, 0.2, 0.1)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(-4.0, 1.0, 0.0), 1.0, materials.back().get()));

    materials.push_back(std::make_unique<pt::metal>(pt::color(0.7, 0.6, 0.5), 0.0));
    world.add(std::make_shared<pt::sphere>(pt::point3(4.0, 1.0, 0.0), 1.0, materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::point3(13.0, 2.0, 3.0);
    cam.lookat = pt::point3(0.0, 0.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void checkered_spheres() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::checker_texture>(0.32, pt::color(0.2, 0.3, 0.1), pt::color(0.9, 0.9, 0.9)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, -10.0, 0.0), 10.0, materials.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 10.0, 0.0), 10.0, materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::point3(13.0, 2.0, 3.0);
    cam.lookat = pt::point3(0.0, 0.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void earth() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::image_texture>("earthmap.jpg"));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 0.0, 0.0), 2.0, materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::point3(0.0, 0.0, 12.0);
    cam.lookat = pt::point3(0.0, 0.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void perlin_spheres() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::noise_texture>(4.0));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::point3(13.0, 2.0, 3.0);
    cam.lookat = pt::point3(0.0, 0.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void quads() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(1.0, 0.2, 0.2)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(-3.0, -2.0, 5.0), pt::vec3(0.0, 0.0, -4.0), pt::vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.2, 1.0, 0.2)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(-2.0, -2.0, 0.0), pt::vec3(4.0, 0.0, 0.0), pt::vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.2, 0.2, 1.0)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(3.0, -2.0, 1.0), pt::vec3(0.0, 0.0, 4.0), pt::vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(1.0, 0.5, 0.0)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(-2.0, 3.0, 1.0), pt::vec3(4.0, 0.0, 0.0), pt::vec3(0.0, 0.0, 4.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.2, 0.8, 0.8)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(-2.0, -3.0, 5.0), pt::vec3(4.0, 0.0, 0.0), pt::vec3(0.0, 0.0, -4.0), materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.7, 0.8, 1.0);

    cam.vfov = 80;
    cam.lookfrom = pt::point3(0.0, 0.0, 9.0);
    cam.lookat = pt::point3(0.0, 0.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void simple_light() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::noise_texture>(4.0));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(4.0, 4.0, 4.0)));
    materials.push_back(std::make_unique<pt::diffuse_light>(textures.back().get()));
    world.add(std::make_shared<pt::quad>(pt::point3(3.0, 1.0, -2.0), pt::vec3(2.0, 0.0, 0.0), pt::vec3(0.0, 2.0, 0.0), materials.back().get()));
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 7.0, 0.0), 2, materials.back().get()));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::color(0.0, 0.0, 0.0);

    cam.vfov = 20;
    cam.lookfrom = pt::point3(26.0, 3.0, 6.0);
    cam.lookat = pt::point3(0.0, 2.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_box() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* red = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* white = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* green = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(15.0, 15.0, 15.0)));
    materials.push_back(std::make_unique<pt::diffuse_light>(textures.back().get()));
    const pt::material* light = materials.back().get();

    materials.push_back(std::make_unique<pt::dielectric>(1.5));
    const pt::material* glass = materials.back().get();

    world.add(std::make_shared<pt::quad>(pt::point3(555.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), pt::vec3(0.0, 0.0, 555.0), green));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), pt::vec3(0.0, 0.0, 555.0), red));
    world.add(std::make_shared<pt::quad>(pt::point3(343.0, 554.0, 332.0), pt::vec3(-130.0, 0.0, 0.0), pt::vec3(0.0, 0.0, -105.0), light));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 0.0), pt::vec3(555.0, 0.0, 0.0), pt::vec3(0.0, 0.0, 555.0), white));
    world.add(std::make_shared<pt::quad>(pt::point3(555.0, 555.0, 555.0), pt::vec3(-555.0, 0.0, 0.0), pt::vec3(0.0, 0.0, -555.0), white));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 555.0), pt::vec3(555.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), white));

    std::shared_ptr<pt::hittable> box1 = pt::box(pt::point3(0.0, 0.0, 0.0), pt::point3(165.0, 330.0, 165.0), white);
    box1 = std::make_shared<pt::rotate_y>(box1, 15.0);
    box1 = std::make_shared<pt::translate>(box1, pt::vec3(265.0, 0.0, 295.0));
    world.add(box1);

    world.add(std::make_shared<pt::sphere>(pt::point3(190.0, 90.0, 190.0), 90.0, glass));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::hittable_list lights;
    lights.add(std::make_shared<pt::quad>(pt::point3(343.0, 554.0, 332.0), pt::vec3(-130.0, 0.0, 0.0), pt::vec3(0.0, 0.0, -105.0), nullptr));
    lights.add(std::make_shared<pt::sphere>(pt::point3(190.0, 90.0, 190.0), 90.0, nullptr));

    pt::camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = pt::color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::point3(278.0, 278.0, -800.0);
    cam.lookat = pt::point3(278.0, 278.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world, &lights);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_smoke() {
    pt::hittable_list world;

    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* red = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* white = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* green = materials.back().get();

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<pt::diffuse_light>(textures.back().get()));
    const pt::material* light = materials.back().get();

    world.add(std::make_shared<pt::quad>(pt::point3(555.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), pt::vec3(0.0, 0.0, 555.0), green));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), pt::vec3(0.0, 0.0, 555.0), red));
    world.add(std::make_shared<pt::quad>(pt::point3(113.0, 554.0, 127.0), pt::vec3(330.0, 0.0, 0.0), pt::vec3(0.0, 0.0, 305.0), light));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 0.0), pt::vec3(555.0, 0.0, 0.0), pt::vec3(0.0, 0.0, 555.0), white));
    world.add(std::make_shared<pt::quad>(pt::point3(555.0, 555.0, 555.0), pt::vec3(-555.0, 0.0, 0.0), pt::vec3(0.0, 0.0, -555.0), white));
    world.add(std::make_shared<pt::quad>(pt::point3(0.0, 0.0, 555.0), pt::vec3(555.0, 0.0, 0.0), pt::vec3(0.0, 555.0, 0.0), white));

    std::shared_ptr<pt::hittable> box1 = pt::box(pt::point3(0.0, 0.0, 0.0), pt::point3(165.0, 330.0, 165.0), white);
    box1 = std::make_shared<pt::rotate_y>(box1, 15.0);
    box1 = std::make_shared<pt::translate>(box1, pt::vec3(265.0, 0.0, 295.0));

    std::shared_ptr<pt::hittable> box2 = pt::box(pt::point3(0.0, 0.0, 0.0), pt::point3(165.0, 165.0, 165.0), white);
    box2 = std::make_shared<pt::rotate_y>(box2, -18.0);
    box2 = std::make_shared<pt::translate>(box2, pt::vec3(130.0, 0.0, 65.0));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.0, 0.0, 0.0)));
    materials.push_back(std::make_unique<pt::isotropic>(textures.back().get()));
    const pt::material* smoke = materials.back().get();
    world.add(std::make_shared<pt::constant_medium>(box1, 0.01, smoke));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<pt::isotropic>(textures.back().get()));
    const pt::material* fog = materials.back().get();
    world.add(std::make_shared<pt::constant_medium>(box2, 0.01, fog));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = pt::color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::point3(278.0, 278.0, -800.0);
    cam.lookat = pt::point3(278.0, 278.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    std::vector<std::unique_ptr<pt::texture>> textures;
    std::vector<std::unique_ptr<pt::material>> materials;

    pt::hittable_list boxes1;

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.48, 0.83, 0.53)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* ground = materials.back().get();

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            const pt::Float w = 100.0;
            const pt::Float x0 = -1000.0 + i * w;
            const pt::Float z0 = -1000.0 + j * w;
            const pt::Float y0 = 0.0;
            const pt::Float x1 = x0 + w;
            const pt::Float z1 = z0 + w;
            const pt::Float y1 = pt::random_double(1, 101);
            boxes1.add(pt::box(pt::point3(x0, y0, z0), pt::point3(x1, y1, z1), ground));
        }
    }

    pt::hittable_list world;
    world.add(std::make_shared<pt::bvh_node>(boxes1));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<pt::diffuse_light>(textures.back().get()));
    const pt::material* light = materials.back().get();
    world.add(std::make_shared<pt::quad>(pt::point3(123.0, 554.0, 147.0), pt::vec3(300.0, 0.0, 0.0), pt::vec3(0.0, 0.0, 265.0), light));

    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.7, 0.3, 0.1)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* orange = materials.back().get();
    const pt::point3 center1 = pt::point3(400.0, 400.0, 200.0);
    const pt::point3 center2 = center1 + pt::vec3(30.0, 0.0, 0.0);
    world.add(std::make_shared<pt::sphere>(center1, center2, 50.0, orange));

    materials.push_back(std::make_unique<pt::dielectric>(1.5));
    const pt::material* glass = materials.back().get();
    world.add(std::make_shared<pt::sphere>(pt::point3(260.0, 150.0, 45.0), 50.0, glass));

    materials.push_back(std::make_unique<pt::metal>(pt::color(0.8, 0.8, 0.9), 1.0));
    const pt::material* metal_mat = materials.back().get();
    world.add(std::make_shared<pt::sphere>(pt::point3(0.0, 150.0, 145.0), 50.0, metal_mat));

    const auto boundary = std::make_shared<pt::sphere>(pt::point3(360.0, 150.0, 145.0), 70.0, glass);
    world.add(boundary);
    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.2, 0.4, 0.9)));
    materials.push_back(std::make_unique<pt::isotropic>(textures.back().get()));
    const pt::material* blue = materials.back().get();
    world.add(std::make_shared<pt::constant_medium>(boundary, 0.2, blue));

    const auto mist_boundary = std::make_shared<pt::sphere>(pt::point3(0.0, 0.0, 0.0), 5000.0, glass);
    textures.push_back(std::make_unique<pt::solid_color>(pt::color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<pt::isotropic>(textures.back().get()));
    const pt::material* mist = materials.back().get();
    world.add(std::make_shared<pt::constant_medium>(mist_boundary, 0.0001, mist));

    textures.push_back(std::make_unique<pt::image_texture>("earthmap.jpg"));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* earth = materials.back().get();
    world.add(std::make_shared<pt::sphere>(pt::point3(400.0, 200.0, 400.0), 100.0, earth));

    textures.push_back(std::make_unique<pt::noise_texture>(0.2));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* perlin_mat = materials.back().get();
    world.add(std::make_shared<pt::sphere>(pt::point3(220.0, 280.0, 300.0), 80.0, perlin_mat));

    pt::hittable_list boxes2;
    textures.push_back(std::make_unique<pt::solid_color>(pt::color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::lambertian>(textures.back().get()));
    const pt::material* white = materials.back().get();
    for (int j = 0; j < 1000; j++) {
        boxes2.add(std::make_shared<pt::sphere>(pt::point3::random(0, 165), 10.0, white));
    }
    world.add(std::make_shared<pt::translate>(std::make_shared<pt::rotate_y>(std::make_shared<pt::bvh_node>(boxes2), 15.0), pt::vec3(-100.0, 270.0, 395.0)));

    world = pt::hittable_list(std::make_shared<pt::bvh_node>(world));

    pt::camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = pt::color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::point3(478.0, 278.0, -600.0);
    cam.lookat = pt::point3(278.0, 278.0, 0.0);
    cam.vup = pt::vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}
