#pragma once
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/materials/material.hpp"
#include "pt/math/color.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/sampling/importance_targets.hpp"
#include "pt/textures/texture.hpp"
#include "pt/util/arena.hpp"
#include <cstddef>
#include <cstdint>
#include <memory>
#include <type_traits>
#include <utility>
#include <vector>

namespace pt {

struct CameraSettings {
    Float vfov = 90.0_f;
    Point3 lookfrom = Point3{0, 0, 0};
    Point3 lookat = Point3{0, 0, 0};
    Vec3 vup = Vec3{0, 1, 0};
    Float defocus_angle = 0.0_f;
    Float focus_dist = 10.0_f;
};

struct RenderSettings {
    int image_width = 100;
    int image_height = 100;
    int samples_per_pixel = 10;
    int max_depth = 10;
    ToneMapSettings tone_map{};
    Color background = Color{0, 0, 0};
    std::uint64_t seed = 0;
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

    [[nodiscard]] const ImportanceTargets& importance_targets() const noexcept { return importance_targets_; }

    [[nodiscard]] Arena<Hittable>& object_arena() noexcept { return objects_; }

    template <typename T, typename... Args>
    [[nodiscard]] const T* create_texture(Args&&... args) {
        return textures_.create<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    [[nodiscard]] const T* create_material(Args&&... args) {
        return materials_.create<T>(std::forward<Args>(args)...);
    }

    template <typename T, typename... Args>
    [[nodiscard]] const T* create_object(Args&&... args) {
        return objects_.create<T>(std::forward<Args>(args)...);
    }

    template <typename... Args>
    [[nodiscard]] const Mesh* create_mesh(Args&&... args) {
        auto owned = std::make_unique<Mesh>(std::forward<Args>(args)...);
        const Mesh* ptr = owned.get();

        meshes_.push_back(std::move(owned));
        return ptr;
    }

    [[nodiscard]] std::size_t mesh_count() const noexcept { return meshes_.size(); }

    void add_object(const Hittable* obj);

    void add_importance_target(const Sampleable* target);

    void build_bvh();

private:
    // Order is critical: C++ destroys members in reverse. Arenas (owners) must outlive lists (non-owners).
    Arena<Texture> textures_;
    Arena<Material> materials_;
    std::vector<std::unique_ptr<Mesh>> meshes_;
    Arena<Hittable> objects_;
    HittableList world_;
    ImportanceTargets importance_targets_;
};

static_assert(!std::is_copy_constructible_v<Scene>);
static_assert(std::is_default_constructible_v<Scene>);
static_assert(std::is_nothrow_move_constructible_v<Scene>);

} // namespace pt
