#include "camera.hpp"
#include "hittable_list.hpp"
#include "lambertian.hpp"
#include "sphere.hpp"
#include "vec3.hpp"
#include <memory>

int main() {
    const lambertian material_ground{ color(0.8, 0.8, 0.0) };
    const lambertian material_center{ color(0.1, 0.2, 0.5) };

    hittable_list world;

    world.add(std::make_shared<sphere>(point3(0, 0, -1), 0.5, &material_center));
    world.add(std::make_shared<sphere>(point3(0, -100.5, -1), 100.0, &material_ground));

    camera cam;

    cam.aspect_ratio = 16.0 / 9.0;
    cam.image_width = 400;
    cam.samples_per_pixel = 100;

    cam.render(world);

    return 0;
}