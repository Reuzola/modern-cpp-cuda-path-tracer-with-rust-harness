#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/geometry/box.hpp"
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/constant_medium.hpp"
#include "pt/geometry/quad.hpp"
#include "pt/geometry/rotate_y.hpp"
#include "pt/geometry/sphere.hpp"
#include "pt/geometry/translate.hpp"
#include "pt/io/color.hpp"
#include "pt/materials/dielectric.hpp"
#include "pt/materials/diffuse_light.hpp"
#include "pt/materials/isotropic.hpp"
#include "pt/materials/lambertian.hpp"
#include "pt/materials/metal.hpp"
#include "pt/math/color.hpp"
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/render/camera.hpp"
#include "pt/render/film.hpp"
#include "pt/render/path_integrator.hpp"
#include "pt/render/renderer.hpp"
#include "pt/scene/scene.hpp"
#include "pt/textures/checker_texture.hpp"
#include "pt/textures/image_texture.hpp"
#include "pt/textures/noise_texture.hpp"
#include "pt/textures/solid_color.hpp"
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <format>
#include <iostream>

namespace { // TEMP
constexpr std::uint64_t scene_construction_seed = 1;
} // namespace

pt::Scene bouncing_spheres();
pt::Scene checkered_spheres();
pt::Scene earth();
pt::Scene perlin_spheres();
pt::Scene quads();
pt::Scene simple_light();
pt::Scene cornell_box();
pt::Scene cornell_smoke();
pt::Scene final_scene();
void render_scene(const pt::Scene& scene);
void write_ppm(std::ostream& out, const pt::Film& film);

int main() {
    pt::Scene scene;

    switch (7) {
    case 1:
        scene = bouncing_spheres();
        break;
    case 2:
        scene = checkered_spheres();
        break;
    case 3:
        scene = earth();
        break;
    case 4:
        scene = perlin_spheres();
        break;
    case 5:
        scene = quads();
        break;
    case 6:
        scene = simple_light();
        break;
    case 7:
        scene = cornell_box();
        break;
    case 8:
        scene = cornell_smoke();
        break;
    case 9:
        scene = final_scene();
        break;
    }
    render_scene(scene);
}

pt::Scene bouncing_spheres() {
    pt::Scene scene;
    pt::Sampler sampler(pt::sampler_seed(scene_construction_seed, 0, 0));

    const auto* ground_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.5, 0.5, 0.5)); // ground
    const auto* ground_mat = scene.create_material<pt::Lambertian>(ground_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, -1000, 0.0), 1000.0, ground_mat));

    for (int a = -11; a < 11; a++) {
        for (int b = -11; b < 11; b++) {
            const auto choose_mat = sampler.next_scalar();

            const pt::Float dx = sampler.next_scalar();
            const pt::Float dz = sampler.next_scalar();
            const pt::Point3 center(a + 0.9 * dx, 0.2, b + 0.9 * dz);
            if ((center - pt::Point3(4.0, 0.2, 0.0)).length() > 0.9) {
                if (choose_mat < 0.8) { // %80 Diffuse
                    const auto albedo_a = pt::Color::random(sampler);
                    const auto albedo_b = pt::Color::random(sampler);
                    const auto albedo = albedo_a * albedo_b;
                    const auto* tex = scene.create_texture<pt::SolidColor>(albedo);
                    const auto* mat = scene.create_material<pt::Lambertian>(tex);
                    const auto center2 = center + pt::Vec3(0.0, sampler.next_scalar(0.0, 0.5), 0.0);
                    scene.add_object(scene.create_object<pt::Sphere>(center, center2, 0.2, mat));
                } else if (choose_mat < 0.95) { // %15 Metal
                    const auto albedo = pt::Color::random(0.5, 1.0, sampler);
                    const auto fuzz = sampler.next_scalar(0.0, 0.5);
                    const auto* mat = scene.create_material<pt::Metal>(albedo, fuzz);
                    scene.add_object(scene.create_object<pt::Sphere>(center, 0.2, mat));
                } else { // %5 Glass
                    const auto* mat = scene.create_material<pt::Dielectric>(1.5);
                    scene.add_object(scene.create_object<pt::Sphere>(center, 0.2, mat));
                }
            }
        }
    }

    const auto* big_glass = scene.create_material<pt::Dielectric>(1.5);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 1.0, 0.0), 1.0, big_glass));

    const auto* brown_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.4, 0.2, 0.1));
    const auto* brown_mat = scene.create_material<pt::Lambertian>(brown_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(-4.0, 1.0, 0.0), 1.0, brown_mat));

    const auto* silver_mat = scene.create_material<pt::Metal>(pt::Color(0.7, 0.6, 0.5), 0.0);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(4.0, 1.0, 0.0), 1.0, silver_mat));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.7, 0.8, 1.0);

    scene.camera.aspect_ratio = 16.0 / 9.0;
    scene.camera.vfov = 20;
    scene.camera.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    scene.camera.lookat = pt::Point3(0.0, 0.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.6;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene checkered_spheres() {
    pt::Scene scene;

    const auto* checker_tex = scene.create_texture<pt::CheckerTexture>(0.32, pt::Color(0.2, 0.3, 0.1), pt::Color(0.9, 0.9, 0.9));
    const auto* checker_mat = scene.create_material<pt::Lambertian>(checker_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, -10.0, 0.0), 10.0, checker_mat));
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 10.0, 0.0), 10.0, checker_mat));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.7, 0.8, 1.0);

    scene.camera.aspect_ratio = 16.0 / 9.0;
    scene.camera.vfov = 20;
    scene.camera.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    scene.camera.lookat = pt::Point3(0.0, 0.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene earth() {
    pt::Scene scene;

    const auto* earth_tex = scene.create_texture<pt::ImageTexture>("earthmap.jpg");
    const auto* earth_mat = scene.create_material<pt::Lambertian>(earth_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 0.0, 0.0), 2.0, earth_mat));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.7, 0.8, 1.0);

    scene.camera.aspect_ratio = 16.0 / 9.0;
    scene.camera.vfov = 20;
    scene.camera.lookfrom = pt::Point3(0.0, 0.0, 12.0);
    scene.camera.lookat = pt::Point3(0.0, 0.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene perlin_spheres() {
    pt::Scene scene;
    pt::Sampler sampler(pt::sampler_seed(scene_construction_seed, 0, 0));

    const auto* noise_tex = scene.create_texture<pt::NoiseTexture>(4.0, sampler);
    const auto* noise_mat = scene.create_material<pt::Lambertian>(noise_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, -1000.0, 0.0), 1000.0, noise_mat));
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 2.0, 0.0), 2.0, noise_mat));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.7, 0.8, 1.0);

    scene.camera.aspect_ratio = 16.0 / 9.0;
    scene.camera.vfov = 20;
    scene.camera.lookfrom = pt::Point3(13.0, 2.0, 3.0);
    scene.camera.lookat = pt::Point3(0.0, 0.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene quads() {
    pt::Scene scene;

    const auto* left_red_tex = scene.create_texture<pt::SolidColor>(pt::Color(1.0, 0.2, 0.2));
    const auto* left_red = scene.create_material<pt::Lambertian>(left_red_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(-3.0, -2.0, 5.0), pt::Vec3(0.0, 0.0, -4.0), pt::Vec3(0.0, 4.0, 0.0), left_red));

    const auto* back_green_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.2, 1.0, 0.2));
    const auto* back_green = scene.create_material<pt::Lambertian>(back_green_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(-2.0, -2.0, 0.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 4.0, 0.0), back_green));

    const auto* right_blue_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.2, 0.2, 1.0));
    const auto* right_blue = scene.create_material<pt::Lambertian>(right_blue_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(3.0, -2.0, 1.0), pt::Vec3(0.0, 0.0, 4.0), pt::Vec3(0.0, 4.0, 0.0), right_blue));

    const auto* top_orange_tex = scene.create_texture<pt::SolidColor>(pt::Color(1.0, 0.5, 0.0));
    const auto* top_orange = scene.create_material<pt::Lambertian>(top_orange_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(-2.0, 3.0, 1.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 4.0), top_orange));

    const auto* bottom_teal_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.2, 0.8, 0.8));
    const auto* bottom_teal = scene.create_material<pt::Lambertian>(bottom_teal_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(-2.0, -3.0, 5.0), pt::Vec3(4.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -4.0), bottom_teal));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.7, 0.8, 1.0);

    scene.camera.aspect_ratio = 1.0;
    scene.camera.vfov = 80;
    scene.camera.lookfrom = pt::Point3(0.0, 0.0, 9.0);
    scene.camera.lookat = pt::Point3(0.0, 0.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene simple_light() {
    pt::Scene scene;
    pt::Sampler sampler(pt::sampler_seed(scene_construction_seed, 0, 0));

    const auto* noise_tex = scene.create_texture<pt::NoiseTexture>(4.0, sampler);
    const auto* noise_mat = scene.create_material<pt::Lambertian>(noise_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, -1000.0, 0.0), 1000.0, noise_mat));
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 2.0, 0.0), 2.0, noise_mat));

    const auto* light_tex = scene.create_texture<pt::SolidColor>(pt::Color(4.0, 4.0, 4.0));
    const auto* light_mat = scene.create_material<pt::DiffuseLight>(light_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(3.0, 1.0, -2.0), pt::Vec3(2.0, 0.0, 0.0), pt::Vec3(0.0, 2.0, 0.0), light_mat));
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 7.0, 0.0), 2, light_mat));

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 100;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.0, 0.0, 0.0);

    scene.camera.aspect_ratio = 16.0 / 9.0;
    scene.camera.vfov = 20;
    scene.camera.lookfrom = pt::Point3(26.0, 3.0, 6.0);
    scene.camera.lookat = pt::Point3(0.0, 2.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene cornell_box() {
    pt::Scene scene;

    const auto* red_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.65, 0.05, 0.05));
    const auto* red = scene.create_material<pt::Lambertian>(red_tex);

    const auto* white_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73));
    const auto* white = scene.create_material<pt::Lambertian>(white_tex);

    const auto* green_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.12, 0.45, 0.15));
    const auto* green = scene.create_material<pt::Lambertian>(green_tex);

    const auto* light_tex = scene.create_texture<pt::SolidColor>(pt::Color(15.0, 15.0, 15.0));
    const auto* light = scene.create_material<pt::DiffuseLight>(light_tex);

    const auto* glass = scene.create_material<pt::Dielectric>(1.5);

    const auto* light_quad = scene.create_object<pt::Quad>(pt::Point3(343.0, 554.0, 332.0), pt::Vec3(-130.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -105.0), light);
    const auto* glass_sphere = scene.create_object<pt::Sphere>(pt::Point3(190.0, 90.0, 190.0), 90.0, glass);

    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), green));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), red));
    scene.add_object(light_quad);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), white));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(555.0, 555.0, 555.0), pt::Vec3(-555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -555.0), white));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 555.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), white));

    const pt::Hittable* box1 = pt::box(scene.object_arena(), pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 330.0, 165.0), white);
    box1 = scene.create_object<pt::RotateY>(box1, 15.0);
    box1 = scene.create_object<pt::Translate>(box1, pt::Vec3(265.0, 0.0, 295.0));
    scene.add_object(box1);

    scene.add_object(glass_sphere);

    scene.add_importance_target(light_quad);
    scene.add_importance_target(glass_sphere);

    scene.build_bvh();

    scene.render.image_width = 600;
    scene.render.samples_per_pixel = 200;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.0, 0.0, 0.0);

    scene.camera.aspect_ratio = 1.0;
    scene.camera.vfov = 40;
    scene.camera.lookfrom = pt::Point3(278.0, 278.0, -800.0);
    scene.camera.lookat = pt::Point3(278.0, 278.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene cornell_smoke() {
    pt::Scene scene;

    const auto* red_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.65, 0.05, 0.05));
    const auto* red = scene.create_material<pt::Lambertian>(red_tex);

    const auto* white_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73));
    const auto* white = scene.create_material<pt::Lambertian>(white_tex);

    const auto* green_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.12, 0.45, 0.15));
    const auto* green = scene.create_material<pt::Lambertian>(green_tex);

    const auto* light_tex = scene.create_texture<pt::SolidColor>(pt::Color(7.0, 7.0, 7.0));
    const auto* light = scene.create_material<pt::DiffuseLight>(light_tex);

    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), green));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), red));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(113.0, 554.0, 127.0), pt::Vec3(330.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 305.0), light));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 0.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 555.0), white));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(555.0, 555.0, 555.0), pt::Vec3(-555.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, -555.0), white));
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(0.0, 0.0, 555.0), pt::Vec3(555.0, 0.0, 0.0), pt::Vec3(0.0, 555.0, 0.0), white));

    const pt::Hittable* box1 = pt::box(scene.object_arena(), pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 330.0, 165.0), white);
    box1 = scene.create_object<pt::RotateY>(box1, 15.0);
    box1 = scene.create_object<pt::Translate>(box1, pt::Vec3(265.0, 0.0, 295.0));

    const pt::Hittable* box2 = pt::box(scene.object_arena(), pt::Point3(0.0, 0.0, 0.0), pt::Point3(165.0, 165.0, 165.0), white);
    box2 = scene.create_object<pt::RotateY>(box2, -18.0);
    box2 = scene.create_object<pt::Translate>(box2, pt::Vec3(130.0, 0.0, 65.0));

    const auto* smoke_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.0, 0.0, 0.0));
    const auto* smoke = scene.create_material<pt::Isotropic>(smoke_tex);
    scene.add_object(scene.create_object<pt::ConstantMedium>(box1, 0.01, smoke));

    const auto* fog_tex = scene.create_texture<pt::SolidColor>(pt::Color(1.0, 1.0, 1.0));
    const auto* fog = scene.create_material<pt::Isotropic>(fog_tex);
    scene.add_object(scene.create_object<pt::ConstantMedium>(box2, 0.01, fog));

    scene.build_bvh();

    scene.render.image_width = 600;
    scene.render.samples_per_pixel = 200;
    scene.render.max_depth = 50;
    scene.render.background = pt::Color(0.0, 0.0, 0.0);

    scene.camera.aspect_ratio = 1.0;
    scene.camera.vfov = 40;
    scene.camera.lookfrom = pt::Point3(278.0, 278.0, -800.0);
    scene.camera.lookat = pt::Point3(278.0, 278.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

pt::Scene final_scene() {
    pt::Scene scene;
    pt::Sampler sampler(pt::sampler_seed(scene_construction_seed, 0, 0));

    pt::HittableList boxes1;

    const auto* ground_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.48, 0.83, 0.53));
    const auto* ground = scene.create_material<pt::Lambertian>(ground_tex);

    for (int i = 0; i < 20; i++) {
        for (int j = 0; j < 20; j++) {
            const pt::Float w = 100.0;
            const pt::Float x0 = -1000.0 + i * w;
            const pt::Float z0 = -1000.0 + j * w;
            const pt::Float y0 = 0.0;
            const pt::Float x1 = x0 + w;
            const pt::Float z1 = z0 + w;
            const pt::Float y1 = sampler.next_scalar(1, 101);
            boxes1.add(pt::box(scene.object_arena(), pt::Point3(x0, y0, z0), pt::Point3(x1, y1, z1), ground));
        }
    }

    scene.add_object(scene.create_object<pt::BvhNode>(scene.object_arena(), boxes1));

    const auto* light_tex = scene.create_texture<pt::SolidColor>(pt::Color(7.0, 7.0, 7.0));
    const auto* light = scene.create_material<pt::DiffuseLight>(light_tex);
    scene.add_object(scene.create_object<pt::Quad>(pt::Point3(123.0, 554.0, 147.0), pt::Vec3(300.0, 0.0, 0.0), pt::Vec3(0.0, 0.0, 265.0), light));

    const auto* orange_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.7, 0.3, 0.1));
    const auto* orange = scene.create_material<pt::Lambertian>(orange_tex);
    const pt::Point3 center1 = pt::Point3(400.0, 400.0, 200.0);
    const pt::Point3 center2 = center1 + pt::Vec3(30.0, 0.0, 0.0);
    scene.add_object(scene.create_object<pt::Sphere>(center1, center2, 50.0, orange));

    const auto* glass = scene.create_material<pt::Dielectric>(1.5);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(260.0, 150.0, 45.0), 50.0, glass));

    const auto* metal_mat = scene.create_material<pt::Metal>(pt::Color(0.8, 0.8, 0.9), 1.0);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(0.0, 150.0, 145.0), 50.0, metal_mat));

    const auto* boundary = scene.create_object<pt::Sphere>(pt::Point3(360.0, 150.0, 145.0), 70.0, glass);
    scene.add_object(boundary);
    const auto* blue_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.2, 0.4, 0.9));
    const auto* blue = scene.create_material<pt::Isotropic>(blue_tex);
    scene.add_object(scene.create_object<pt::ConstantMedium>(boundary, 0.2, blue));

    const auto* mist_boundary = scene.create_object<pt::Sphere>(pt::Point3(0.0, 0.0, 0.0), 5000.0, glass);
    const auto* mist_tex = scene.create_texture<pt::SolidColor>(pt::Color(1.0, 1.0, 1.0));
    const auto* mist = scene.create_material<pt::Isotropic>(mist_tex);
    scene.add_object(scene.create_object<pt::ConstantMedium>(mist_boundary, 0.0001, mist));

    const auto* earth_tex = scene.create_texture<pt::ImageTexture>("earthmap.jpg");
    const auto* earth = scene.create_material<pt::Lambertian>(earth_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(400.0, 200.0, 400.0), 100.0, earth));

    const auto* perlin_tex = scene.create_texture<pt::NoiseTexture>(0.2, sampler);
    const auto* perlin_mat = scene.create_material<pt::Lambertian>(perlin_tex);
    scene.add_object(scene.create_object<pt::Sphere>(pt::Point3(220.0, 280.0, 300.0), 80.0, perlin_mat));

    pt::HittableList boxes2;
    const auto* white_tex = scene.create_texture<pt::SolidColor>(pt::Color(0.73, 0.73, 0.73));
    const auto* white = scene.create_material<pt::Lambertian>(white_tex);
    for (int j = 0; j < 1000; j++) {
        boxes2.add(scene.create_object<pt::Sphere>(pt::Point3::random(0, 165, sampler), 10.0, white));
    }
    const pt::Hittable* spheres = scene.create_object<pt::BvhNode>(scene.object_arena(), boxes2);
    spheres = scene.create_object<pt::RotateY>(spheres, 15.0);
    spheres = scene.create_object<pt::Translate>(spheres, pt::Vec3(-100.0, 270.0, 395.0));
    scene.add_object(spheres);

    scene.build_bvh();

    scene.render.image_width = 400;
    scene.render.samples_per_pixel = 250;
    scene.render.max_depth = 4;
    scene.render.background = pt::Color(0.0, 0.0, 0.0);

    scene.camera.aspect_ratio = 1.0;
    scene.camera.vfov = 40;
    scene.camera.lookfrom = pt::Point3(478.0, 278.0, -600.0);
    scene.camera.lookat = pt::Point3(278.0, 278.0, 0.0);
    scene.camera.vup = pt::Vec3(0.0, 1.0, 0.0);
    scene.camera.defocus_angle = 0.0;
    scene.camera.focus_dist = 10.0;

    return scene;
}

void render_scene(const pt::Scene& scene) {
    const int image_width = scene.render.image_width;
    const int image_height = std::max(static_cast<int>(static_cast<pt::Float>(image_width) / scene.camera.aspect_ratio), 1);

    const pt::Camera camera(scene.camera, image_width, image_height);
    const pt::PathIntegrator integrator(scene.world(), scene.importance_targets(), scene.render.background, scene.render.max_depth);
    const pt::Renderer renderer(camera, integrator, scene.render, image_height);

    const auto start = std::chrono::steady_clock::now();
    const pt::Film film = renderer.render();
    const auto end = std::chrono::steady_clock::now();

    write_ppm(std::cout, film); // temp

    const std::chrono::duration<double> elapsed = end - start;

    std::clog << std::format("Render time: {:.2f}s\n", elapsed.count());
}

void write_ppm(std::ostream& out, const pt::Film& film) {
    out << "P3\n"
        << film.width() << ' ' << film.height() << "\n255\n";

    for (int j = 0; j < film.height(); j++) {
        for (int i = 0; i < film.width(); i++) {
            pt::write_color(out, film.pixel(i, j));
        }
    }
}
