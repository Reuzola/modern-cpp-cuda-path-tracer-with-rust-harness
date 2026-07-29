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
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.5, 0.5, 0.5))); // ground
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, -1000, 0.0), 1000.0, materials.back().get()));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            const auto choose_mat = pt::random_scalar();

            const pt::Point3 center(a + 0.9 * pt::random_scalar(), 0.2, b + 0.9 * pt::random_scalar());
            if ((center - pt::Point3(4.0, 0.2, 0.0)).length() > 0.9) {
                if (choose_mat < 0.8) { // %80 Diffuse
                    const auto albedo = pt::Color::random() * pt::Color::random();
                    textures.push_back(std::make_unique<pt::SolidColor>(albedo));
                    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
                    const auto center2 = center + pt::Vec3(0.0, pt::random_scalar(0.0, 0.5), 0.0);
                    world.add(std::make_shared<pt::Sphere>(center, center2, 0.2, materials.back().get()));
                } else if (choose_mat < 0.95) { // %15 Metal
                    const auto albedo = pt::Color::random(0.5, 1.0);
                    const auto fuzz = pt::random_scalar(0.0, 0.5);
                    materials.push_back(std::make_unique<pt::Metal>(albedo, fuzz));
                    world.add(std::make_shared<pt::Sphere>(center, 0.2, materials.back().get()));
                } else { // %5 Glass
                    materials.push_back(std::make_unique<pt::Dielectric>(1.5));
                    world.add(std::make_shared<pt::Sphere>(center, 0.2, materials.back().get()));
                }
            }
        }
    }

    materials.push_back(std::make_unique<pt::Dielectric>(1.5));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 1.0, 0.0), 1.0, materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.4, 0.2, 0.1)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(-4.0, 1.0, 0.0), 1.0, materials.back().get()));

    materials.push_back(std::make_unique<pt::Metal>(pt::Color(0.7, 0.6, 0.5), 0.0));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(4.0, 1.0, 0.0), 1.0, materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    cam.lookat = pt::Point3(0.0, 0.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.6;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void checkered_spheres() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::CheckerTexture>(0.32, pt::Color(0.2, 0.3, 0.1), pt::Color(0.9, 0.9, 0.9)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, -10.0, 0.0), 10.0, materials.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 10.0, 0.0), 10.0, materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    cam.lookat = pt::Point3(0.0, 0.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void earth() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::ImageTexture>("earthmap.jpg"));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 0.0, 0.0), 2.0, materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::Point3(0.0, 0.0, 12.0);
    cam.lookat = pt::Point3(0.0, 0.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void perlin_spheres() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::NoiseTexture>(4.0));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.7, 0.8, 1.0);

    cam.vfov = 20;
    cam.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    cam.lookat = pt::Point3(0.0, 0.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void quads() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(1.0, 0.2, 0.2)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(-3.0, -2.0, 5.0), pt::Vec3(0.0, 0.0, -4.0), pt::Vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.2, 1.0, 0.2)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(-2.0, -2.0, 0.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.2, 0.2, 1.0)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(3.0, -2.0, 1.0), pt::Vec3(0.0, 0.0, 4.0), pt::Vec3(0.0, 4.0, 0.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(1.0, 0.5, 0.0)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(-2.0, 3.0, 1.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 4.0), materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.2, 0.8, 0.8)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(-2.0, -3.0, 5.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -4.0), materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.7, 0.8, 1.0);

    cam.vfov = 80;
    cam.lookfrom = pt::Point3(0.0, 0.0, 9.0);
    cam.lookat = pt::Point3(0.0, 0.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void simple_light() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::NoiseTexture>(4.0));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(4.0, 4.0, 4.0)));
    materials.push_back(std::make_unique<pt::DiffuseLight>(textures.back().get()));
    world.add(std::make_shared<pt::Quad>(pt::Point3(3.0, 1.0, -2.0), pt::Vec3(2.0, 0.0, 0.0), pt::Vec3(0.0, 2.0, 0.0), materials.back().get()));
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 7.0, 0.0), 2, materials.back().get()));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = pt::Color(0.0, 0.0, 0.0);

    cam.vfov = 20;
    cam.lookfrom = pt::Point3(26.0, 3.0, 6.0);
    cam.lookat = pt::Point3(0.0, 2.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_box() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* red = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* white = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* green = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(15.0, 15.0, 15.0)));
    materials.push_back(std::make_unique<pt::DiffuseLight>(textures.back().get()));
    const pt::Material* light = materials.back().get();

    materials.push_back(std::make_unique<pt::Dielectric>(1.5));
    const pt::Material* glass = materials.back().get();

    world.add(std::make_shared<pt::Quad>(pt::Point3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), green));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), red));
    world.add(std::make_shared<pt::Quad>(pt::Point3(343.0, 554.0, 332.0), pt::Vec3(-130.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -105.0), light));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), white));
    world.add(std::make_shared<pt::Quad>(pt::Point3(555.0, 555.0, 555.0), pt::Vec3(-555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -555.0), white));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 555.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), white));

    std::shared_ptr<pt::Hittable> box1 = pt::box(pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 330.0, 165.0), white);
    box1 = std::make_shared<pt::RotateY>(box1, 15.0);
    box1 = std::make_shared<pt::Translate>(box1, pt::Vec3(265.0, 0.0, 295.0));
    world.add(box1);

    world.add(std::make_shared<pt::Sphere>(pt::Point3(190.0, 90.0, 190.0), 90.0, glass));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::HittableList lights;
    lights.add(std::make_shared<pt::Quad>(pt::Point3(343.0, 554.0, 332.0), pt::Vec3(-130.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -105.0), nullptr));
    lights.add(std::make_shared<pt::Sphere>(pt::Point3(190.0, 90.0, 190.0), 90.0, nullptr));

    pt::Camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = pt::Color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::Point3(278.0, 278.0, -800.0);
    cam.lookat = pt::Point3(278.0, 278.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world, &lights);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_smoke() {
    pt::HittableList world;

    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* red = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* white = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* green = materials.back().get();

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<pt::DiffuseLight>(textures.back().get()));
    const pt::Material* light = materials.back().get();

    world.add(std::make_shared<pt::Quad>(pt::Point3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), green));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), red));
    world.add(std::make_shared<pt::Quad>(pt::Point3(113.0, 554.0, 127.0), pt::Vec3(330.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 305.0), light));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), white));
    world.add(std::make_shared<pt::Quad>(pt::Point3(555.0, 555.0, 555.0), pt::Vec3(-555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -555.0), white));
    world.add(std::make_shared<pt::Quad>(pt::Point3(0.0, 0.0, 555.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), white));

    std::shared_ptr<pt::Hittable> box1 = pt::box(pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 330.0, 165.0), white);
    box1 = std::make_shared<pt::RotateY>(box1, 15.0);
    box1 = std::make_shared<pt::Translate>(box1, pt::Vec3(265.0, 0.0, 295.0));

    std::shared_ptr<pt::Hittable> box2 = pt::box(pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 165.0, 165.0), white);
    box2 = std::make_shared<pt::RotateY>(box2, -18.0);
    box2 = std::make_shared<pt::Translate>(box2, pt::Vec3(130.0, 0.0, 65.0));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.0, 0.0, 0.0)));
    materials.push_back(std::make_unique<pt::Isotropic>(textures.back().get()));
    const pt::Material* smoke = materials.back().get();
    world.add(std::make_shared<pt::ConstantMedium>(box1, 0.01, smoke));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<pt::Isotropic>(textures.back().get()));
    const pt::Material* fog = materials.back().get();
    world.add(std::make_shared<pt::ConstantMedium>(box2, 0.01, fog));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = pt::Color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::Point3(278.0, 278.0, -800.0);
    cam.lookat = pt::Point3(278.0, 278.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    std::vector<std::unique_ptr<pt::Texture>> textures;
    std::vector<std::unique_ptr<pt::Material>> materials;

    pt::HittableList boxes1;

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.48, 0.83, 0.53)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* ground = materials.back().get();

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            const pt::Float w = 100.0;
            const pt::Float x0 = -1000.0 + i * w;
            const pt::Float z0 = -1000.0 + j * w;
            const pt::Float y0 = 0.0;
            const pt::Float x1 = x0 + w;
            const pt::Float z1 = z0 + w;
            const pt::Float y1 = pt::random_scalar(1, 101);
            boxes1.add(pt::box(pt::Point3(x0, y0, z0), pt::Point3(x1, y1, z1), ground));
        }
    }

    pt::HittableList world;
    world.add(std::make_shared<pt::BvhNode>(boxes1));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<pt::DiffuseLight>(textures.back().get()));
    const pt::Material* light = materials.back().get();
    world.add(std::make_shared<pt::Quad>(pt::Point3(123.0, 554.0, 147.0), pt::Vec3(300.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 265.0), light));

    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.7, 0.3, 0.1)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* orange = materials.back().get();
    const pt::Point3 center1 = pt::Point3(400.0, 400.0, 200.0);
    const pt::Point3 center2 = center1 + pt::Vec3(30.0, 0.0, 0.0);
    world.add(std::make_shared<pt::Sphere>(center1, center2, 50.0, orange));

    materials.push_back(std::make_unique<pt::Dielectric>(1.5));
    const pt::Material* glass = materials.back().get();
    world.add(std::make_shared<pt::Sphere>(pt::Point3(260.0, 150.0, 45.0), 50.0, glass));

    materials.push_back(std::make_unique<pt::Metal>(pt::Color(0.8, 0.8, 0.9), 1.0));
    const pt::Material* metal_mat = materials.back().get();
    world.add(std::make_shared<pt::Sphere>(pt::Point3(0.0, 150.0, 145.0), 50.0, metal_mat));

    const auto boundary = std::make_shared<pt::Sphere>(pt::Point3(360.0, 150.0, 145.0), 70.0, glass);
    world.add(boundary);
    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.2, 0.4, 0.9)));
    materials.push_back(std::make_unique<pt::Isotropic>(textures.back().get()));
    const pt::Material* blue = materials.back().get();
    world.add(std::make_shared<pt::ConstantMedium>(boundary, 0.2, blue));

    const auto mist_boundary = std::make_shared<pt::Sphere>(pt::Point3(0.0, 0.0, 0.0), 5000.0, glass);
    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<pt::Isotropic>(textures.back().get()));
    const pt::Material* mist = materials.back().get();
    world.add(std::make_shared<pt::ConstantMedium>(mist_boundary, 0.0001, mist));

    textures.push_back(std::make_unique<pt::ImageTexture>("earthmap.jpg"));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* earth = materials.back().get();
    world.add(std::make_shared<pt::Sphere>(pt::Point3(400.0, 200.0, 400.0), 100.0, earth));

    textures.push_back(std::make_unique<pt::NoiseTexture>(0.2));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* perlin_mat = materials.back().get();
    world.add(std::make_shared<pt::Sphere>(pt::Point3(220.0, 280.0, 300.0), 80.0, perlin_mat));

    pt::HittableList boxes2;
    textures.push_back(std::make_unique<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<pt::Lambertian>(textures.back().get()));
    const pt::Material* white = materials.back().get();
    for (int j = 0; j < 1000; j++) {
        boxes2.add(std::make_shared<pt::Sphere>(pt::Point3::random(0, 165), 10.0, white));
    }
    world.add(std::make_shared<pt::Translate>(std::make_shared<pt::RotateY>(std::make_shared<pt::BvhNode>(boxes2), 15.0), pt::Vec3(-100.0, 270.0, 395.0)));

    world = pt::HittableList(std::make_shared<pt::BvhNode>(world));

    pt::Camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = pt::Color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = pt::Point3(478.0, 278.0, -600.0);
    cam.lookat = pt::Point3(278.0, 278.0, 0.0);
    cam.vup = pt::Vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}
