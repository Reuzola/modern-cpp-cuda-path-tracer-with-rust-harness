#pragma once
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/textures/texture.hpp"
#include <concepts>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace pt {

struct CameraSettings {
    Float aspect_ratio = 1.0_f;
    Float vfov = 90.0_f;
    Point3 lookfrom = Point3{0, 0, 0};
    Point3 lookat = Point3{0, 0, 0};
    Vec3 vup = Vec3{0, 0, 0};
    Float defocus_angle = 0.0_f;
    Float focus_dist = 10.0_f;
};

struct RenderSettings {
    int image_width = 100;
    int samples_per_pixel = 10;
    int max_depth = 10;
    Color background = Color{0, 0, 0};
};

class Scene {
public:
    CameraSettings camera{};
    RenderSettings render{};

    Scene() = default;
    Scene(const Scene&) = delete;
    Scene& operator=(const Scene&) = delete;
    Scene(Scene&&) noexcept = default;
    Scene& operator=(Scene&&) noexcept = default;

    [[nodiscard]] const Hittable& world() const noexcept { return world_; }

    [[nodiscard]] const Hittable* lights() const noexcept { return lights_.empty() ? nullptr : &lights_; }

    template <typename T, typename... Args>
        requires std::derived_from<T, Texture>
    [[nodiscard]] const T* create_texture(Args&&... args) {
        auto tex = std::make_unique<T>(std::forward<Args>(args)...);
        const T* tex_ptr = tex.get();
        textures_.push_back(std::move(tex));
        return tex_ptr;
    }

    template <typename T, typename... Args>
        requires std::derived_from<T, Material>
    [[nodiscard]] const T* create_material(Args&&... args) {
        auto mat = std::make_unique<T>(std::forward<Args>(args)...);
        const T* mat_ptr = mat.get();
        materials_.push_back(std::move(mat));
        return mat_ptr;
    }

    void add_object(std::shared_ptr<Hittable> obj);

    void add_light(std::shared_ptr<Hittable> obj);

    void build_bvh();

private:
    std::vector<std::unique_ptr<Texture>> textures_;
    std::vector<std::unique_ptr<Material>> materials_;
    HittableList world_;
    HittableList lights_;
};

static_assert(!std::is_copy_constructible_v<Scene>);
static_assert(std::is_default_constructible_v<Scene>);
static_assert(std::is_nothrow_move_constructible_v<Scene>);

} // namespace pt
