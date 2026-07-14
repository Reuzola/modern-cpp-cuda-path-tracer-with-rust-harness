#include "camera.hpp"
#include "hittable_list.hpp"
#include "lambertian.hpp"
#include "metal.hpp"
#include "dielectric.hpp"
#include "sphere.hpp"
#include "vec3.hpp"
#include <cmath>
#include <memory>

int main() {
    hittable_list world;

    const lambertian material_ground{ color(0.8, 0.8, 0.0) };
    const lambertian material_center{ color(0.1, 0.2, 0.5) };
    const dielectric material_left{ 1.5 };
    const dielectric material_bubble{ 1.0 / 1.5};
    const metal material_right{ color(0.8, 0.6, 0.2), 1.0 };

    world.add(std::make_shared<sphere>(point3(0.0, -100.5, -1.0), 100.0, &material_ground));
    world.add(std::make_shared<sphere>(point3(0.0, 0.0, -1.2), 0.5, &material_center));
    world.add(std::make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.5, &material_left));
    world.add(std::make_shared<sphere>(point3(-1.0, 0.0, -1.0), 0.4, &material_bubble));
    world.add(std::make_shared<sphere>(point3( 1.0, 0.0, -1.0), 0.5, &material_right));
    
    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;
    cam.max_depth = 50;

    cam.vfov = 20;
    cam.lookfrom = point3(-2.0, 2.0, 1.0);
    cam.lookat = point3(0.0, 0.0, -1.0);
    cam.vup = vec3(0.0, 1.0, 0.0);

    cam.render(world);

    return 0;
}