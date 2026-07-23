#include "box.hpp"
#include "bvh.hpp"
#include "camera.hpp"
#include "checker_texture.hpp"
#include "constant_medium.hpp"
#include "hittable.hpp"
#include "hittable_list.hpp"
#include "image_texture.hpp"
#include "isotropic.hpp"
#include "noise_texture.hpp"
#include "lambertian.hpp"
#include "material.hpp"
#include "metal.hpp"
#include "dielectric.hpp"
#include "quad.hpp"
#include "diffuse_light.hpp"
#include "random.hpp"
#include "rotate_y.hpp"
#include "solid_color.hpp"
#include "sphere.hpp"
#include "texture.hpp"
#include "translate.hpp"
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
void simple_light();
void cornell_box();
void cornell_smoke();
void final_scene(int image_width, int samples_per_pixel, int max_depth);

int main() {
    switch (7) {
        case 1: bouncing_spheres(); break;
        case 2: checkered_spheres(); break;
        case 3: earth(); break;
        case 4: perlin_spheres(); break;
        case 5: quads(); break;
        case 6: simple_light(); break;
        case 7: cornell_box(); break;
        case 8: cornell_smoke(); break;
        case 9: final_scene(400, 250, 4); break;
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
    cam.background = color(0.7, 0.8, 1.0);

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
    cam.background = color(0.7, 0.8, 1.0);

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
    cam.background = color(0.7, 0.8, 1.0);

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
    cam.background = color(0.7, 0.8, 1.0);

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
    cam.background = color(0.7, 0.8, 1.0);

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

void simple_light() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<noise_texture>(4.0));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, -1000.0, 0.0), 1000.0, materials.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, 2.0, 0.0), 2.0, materials.back().get()));

    textures.push_back(std::make_unique<solid_color>(color(4.0, 4.0, 4.0)));
    materials.push_back(std::make_unique<diffuse_light>(textures.back().get()));
    world.add(std::make_shared<quad>(point3(3.0, 1.0, -2.0), vec3(2.0, 0.0, 0.0), vec3(0.0, 2.0, 0.0), materials.back().get()));
    world.add(std::make_shared<sphere>(point3(0.0, 7.0, 0.0), 2, materials.back().get()));

    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;
    cam.background = color(0.0, 0.0, 0.0);

    cam.vfov = 20;
    cam.lookfrom = point3(26.0, 3.0, 6.0);
    cam.lookat = point3(0.0, 2.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_box() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<solid_color>(color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* red = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* white = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* green = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(15.0, 15.0, 15.0)));
    materials.push_back(std::make_unique<diffuse_light>(textures.back().get()));
    const material* light = materials.back().get();

    world.add(std::make_shared<quad>(point3(555.0,0.0,0.0), vec3(0.0,555.0,0.0), vec3(0.0,0.0,555.0), green));
    world.add(std::make_shared<quad>(point3(0.0,0.0,0.0), vec3(0.0,555.0,0.0), vec3(0.0,0.0,555.0), red));
    world.add(std::make_shared<quad>(point3(343.0,554.0,332.0), vec3(-130.0,0.0,0.0), vec3(0.0,0.0,-105.0), light));
    world.add(std::make_shared<quad>(point3(0.0,0.0,0.0), vec3(555.0,0.0,0.0), vec3(0.0,0.0,555.0), white));
    world.add(std::make_shared<quad>(point3(555.0,555.0,555.0), vec3(-555.0,0.0,0.0), vec3(0.0,0.0,-555.0), white));
    world.add(std::make_shared<quad>(point3(0.0,0.0,555.0), vec3(555.0,0.0,0.0), vec3(0.0,555.0,0.0), white));

    materials.push_back(std::make_unique<metal>(color(0.8, 0.85, 0.88), 0.0));
    const material* aluminum = materials.back().get();
    
    std::shared_ptr<hittable> box1 = box(point3(0.0, 0.0, 0.0), point3(165.0, 330.0, 165.0), aluminum);
    box1 = std::make_shared<rotate_y>(box1, 15.0);
    box1 = std::make_shared<translate>(box1, vec3(265.0, 0.0, 295.0));
    world.add(box1);

    std::shared_ptr<hittable> box2 = box(point3(0.0, 0.0, 0.0), point3(165.0, 165.0, 165.0), white);
    box2 = std::make_shared<rotate_y>(box2, -18.0);
    box2 = std::make_shared<translate>(box2, vec3(130.0, 0.0, 65.0));
    world.add(box2);

    world = hittable_list(std::make_shared<bvh_node>(world));

    const quad lights(point3(343.0, 554.0, 332.0), vec3(-130.0, 0.0, 0.0), vec3(0.0, 0.0, -105.0), nullptr);

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = point3(278.0, 278.0, -800.0);
    cam.lookat = point3(278.0, 278.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world, &lights);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void cornell_smoke() {
    hittable_list world;

    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    textures.push_back(std::make_unique<solid_color>(color(0.65, 0.05, 0.05)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* red = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* white = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(0.12, 0.45, 0.15)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* green = materials.back().get();

    textures.push_back(std::make_unique<solid_color>(color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<diffuse_light>(textures.back().get()));
    const material* light = materials.back().get();

    world.add(std::make_shared<quad>(point3(555.0,0.0,0.0), vec3(0.0,555.0,0.0), vec3(0.0,0.0,555.0), green));
    world.add(std::make_shared<quad>(point3(0.0,0.0,0.0), vec3(0.0,555.0,0.0), vec3(0.0,0.0,555.0), red));
    world.add(std::make_shared<quad>(point3(113.0,554.0,127.0), vec3(330.0,0.0,0.0), vec3(0.0,0.0,305.0), light));
    world.add(std::make_shared<quad>(point3(0.0,0.0,0.0), vec3(555.0,0.0,0.0), vec3(0.0,0.0,555.0), white));
    world.add(std::make_shared<quad>(point3(555.0,555.0,555.0), vec3(-555.0,0.0,0.0), vec3(0.0,0.0,-555.0), white));
    world.add(std::make_shared<quad>(point3(0.0,0.0,555.0), vec3(555.0,0.0,0.0), vec3(0.0,555.0,0.0), white));

    std::shared_ptr<hittable> box1 = box(point3(0.0, 0.0, 0.0), point3(165.0, 330.0, 165.0), white);
    box1 = std::make_shared<rotate_y>(box1, 15.0);
    box1 = std::make_shared<translate>(box1, vec3(265.0, 0.0, 295.0));

    std::shared_ptr<hittable> box2 = box(point3(0.0, 0.0, 0.0), point3(165.0, 165.0, 165.0), white);
    box2 = std::make_shared<rotate_y>(box2, -18.0);
    box2 = std::make_shared<translate>(box2, vec3(130.0, 0.0, 65.0));

    textures.push_back(std::make_unique<solid_color>(color(0.0, 0.0, 0.0)));
    materials.push_back(std::make_unique<isotropic>(textures.back().get()));
    const material* smoke = materials.back().get();
    world.add(std::make_shared<constant_medium>(box1, 0.01, smoke));

    textures.push_back(std::make_unique<solid_color>(color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<isotropic>(textures.back().get()));
    const material* fog = materials.back().get();
    world.add(std::make_shared<constant_medium>(box2, 0.01, fog));

    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = 600;
    cam.samples_per_pixel = 200;
    cam.max_depth = 50;
    cam.background = color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = point3(278.0, 278.0, -800.0);
    cam.lookat = point3(278.0, 278.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void final_scene(int image_width, int samples_per_pixel, int max_depth) {
    std::vector<std::unique_ptr<texture>> textures;
    std::vector<std::unique_ptr<material>> materials;

    hittable_list boxes1;

    textures.push_back(std::make_unique<solid_color>(color(0.48, 0.83, 0.53)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* ground = materials.back().get();

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            const double w = 100.0;
            const double x0 = -1000.0 + i * w;
            const double z0 = -1000.0 + j * w;
            const double y0 = 0.0;
            const double x1 = x0 + w;
            const double z1 = z0 + w;
            const double y1 = random_double(1, 101);
            boxes1.add(box(point3(x0, y0, z0), point3(x1, y1, z1), ground));
        }
    }

    hittable_list world;
    world.add(std::make_shared<bvh_node>(boxes1));

    textures.push_back(std::make_unique<solid_color>(color(7.0, 7.0, 7.0)));
    materials.push_back(std::make_unique<diffuse_light>(textures.back().get()));
    const material* light = materials.back().get();
    world.add(std::make_shared<quad>(point3(123.0, 554.0, 147.0), vec3(300.0, 0.0, 0.0), vec3(0.0, 0.0, 265.0), light));

    textures.push_back(std::make_unique<solid_color>(color(0.7, 0.3, 0.1)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* orange = materials.back().get();
    const point3 center1 = point3(400.0, 400.0, 200.0);
    const point3 center2 = center1 + vec3(30.0, 0.0, 0.0);
    world.add(std::make_shared<sphere>(center1, center2, 50.0, orange));

    materials.push_back(std::make_unique<dielectric>(1.5));
    const material* glass = materials.back().get();
    world.add(std::make_shared<sphere>(point3(260.0, 150.0, 45.0), 50.0, glass));

    materials.push_back(std::make_unique<metal>(color(0.8, 0.8, 0.9), 1.0));
    const material* metal_mat = materials.back().get();
    world.add(std::make_shared<sphere>(point3(0.0, 150.0, 145.0), 50.0, metal_mat));

    const auto boundary = std::make_shared<sphere>(point3(360.0, 150.0, 145.0), 70.0, glass);
    world.add(boundary);
    textures.push_back(std::make_unique<solid_color>(color(0.2, 0.4, 0.9)));
    materials.push_back(std::make_unique<isotropic>(textures.back().get()));
    const material* blue = materials.back().get();
    world.add(std::make_shared<constant_medium>(boundary, 0.2, blue));

    const auto mist_boundary = std::make_shared<sphere>(point3(0.0, 0.0, 0.0), 5000.0, glass);
    textures.push_back(std::make_unique<solid_color>(color(1.0, 1.0, 1.0)));
    materials.push_back(std::make_unique<isotropic>(textures.back().get()));
    const material* mist = materials.back().get();
    world.add(std::make_shared<constant_medium>(mist_boundary, 0.0001, mist));

    textures.push_back(std::make_unique<image_texture>("earthmap.jpg"));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* earth = materials.back().get();
    world.add(std::make_shared<sphere>(point3(400.0, 200.0, 400.0), 100.0, earth));

    textures.push_back(std::make_unique<noise_texture>(0.2));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* perlin_mat = materials.back().get();
    world.add(std::make_shared<sphere>(point3(220.0, 280.0, 300.0), 80.0, perlin_mat));

    hittable_list boxes2;
    textures.push_back(std::make_unique<solid_color>(color(0.73, 0.73, 0.73)));
    materials.push_back(std::make_unique<lambertian>(textures.back().get()));
    const material* white = materials.back().get();
    for (int j = 0; j < 1000; j++) {
        boxes2.add(std::make_shared<sphere>(point3::random(0, 165), 10.0, white));
    }
    world.add(std::make_shared<translate>(std::make_shared<rotate_y>(std::make_shared<bvh_node>(boxes2), 15.0), vec3(-100.0, 270.0, 395.0)));

    world = hittable_list(std::make_shared<bvh_node>(world));

    camera cam;

    cam.aspect_ratio = 1.0;
    cam.image_width = image_width;
    cam.samples_per_pixel = samples_per_pixel;
    cam.max_depth = max_depth;
    cam.background = color(0.0, 0.0, 0.0);

    cam.vfov = 40;
    cam.lookfrom = point3(478.0, 278.0, -600.0);
    cam.lookat = point3(278.0, 278.0, 0.0);
    cam.vup = vec3(0.0, 1.0, 0.0);
    cam.defocus_angle = 0.0;
    cam.focus_dist = 10.0;

    const auto start = std::chrono::steady_clock::now();
    cam.render(world);
    const auto end = std::chrono::steady_clock::now();

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}