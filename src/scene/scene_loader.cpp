#include "pt/scene/scene_loader.hpp"
#include "pt/core/hittable.hpp"
#include "pt/core/hittable_list.hpp"
#include "pt/core/sampleable.hpp"
#include "pt/geometry/box.hpp"
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
#include "pt/math/sampler.hpp"
#include "pt/math/scalar.hpp"
#include "pt/math/vec3.hpp"
#include "pt/post/tonemap.hpp"
#include "pt/scene/scene.hpp"
#include "pt/textures/checker_texture.hpp"
#include "pt/textures/image_texture.hpp"
#include "pt/textures/noise_texture.hpp"
#include "pt/textures/solid_color.hpp"
#include "pt/textures/texture.hpp"
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <map>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>
#include <string>
#include <string_view>
#include <utility>

namespace pt {

using Json = nlohmann::json;

namespace {

struct LoadContext {
    Scene& scene;
    std::filesystem::path base_dir;
    std::map<std::string, const Texture*> textures;
    std::map<std::string, const Material*> materials;
    std::map<std::string, const Hittable*> objects;
};

template <typename T>
[[nodiscard]] std::string available_names(const std::map<std::string, const T*>& table) {
    std::string names;
    for (const auto& entry : table) {
        if (!names.empty()) names += ", ";
        names += entry.first;
    }
    return names.empty() ? std::string("<none>") : names;
}

[[nodiscard]] const Json& field(const Json& obj, std::string_view key) {
    if (!obj.is_object()) throw SceneError("Expected JSON object to access field");

    const auto it = obj.find(key);
    if (it == obj.end()) throw SceneError(std::format("Missing required field: '{}'", key));

    return it.value();
}

[[nodiscard]] Float read_number(const Json& v) {
    if (!v.is_number()) throw SceneError("Expected number");
    return static_cast<Float>(v.get<double>());
}

[[nodiscard]] int read_int(const Json& v) {
    if (!v.is_number_integer()) throw SceneError("Expected integer number");

    const auto val = v.get<std::int64_t>();
    if (!std::in_range<int>(val)) throw SceneError(std::format("Integer value {} out of range for int", val));

    return static_cast<int>(val);
}

[[nodiscard]] std::uint64_t read_seed(const Json& v) {
    if (!v.is_number_unsigned()) throw SceneError("Expected unsigned integer for seed");
    return v.get<std::uint64_t>();
}

[[nodiscard]] const std::string& read_string(const Json& v) {
    if (!v.is_string()) throw SceneError("Expected string");
    return v.get_ref<const std::string&>();
}

[[nodiscard]] Vec3 read_vec3(const Json& v) {
    if (!v.is_array() || v.size() != 3) throw SceneError("Expected 3-element array for Vec3");
    return Vec3(read_number(v.at(0)), read_number(v.at(1)), read_number(v.at(2)));
}

[[nodiscard]] Color read_color(const Json& v) {
    if (!v.is_array() || v.size() != 3) throw SceneError("Expected 3-element array for Color");

    const Float r = read_number(v.at(0));
    const Float g = read_number(v.at(1));
    const Float b = read_number(v.at(2));
    if (r < 0 || g < 0 || b < 0)
        throw SceneError(std::format("Color components must be non-negative (>= 0) r:{}, g:{}, b:{}", r, g, b));

    return Color(r, g, b);
}

[[nodiscard]] const Texture* build_texture(const Json& j, LoadContext& ctx) {
    const std::string& type = read_string(field(j, "type"));

    if (type == "solid_color") {
        const Color albedo = read_color(field(j, "albedo"));
        return ctx.scene.create_texture<SolidColor>(albedo);
    }

    if (type == "checker") {
        const Float scale = read_number(field(j, "scale"));
        if (scale <= 0) throw SceneError(std::format("Checker texture scale must be positive, got {}", scale));

        const Texture* even = build_texture(field(j, "even"), ctx);
        const Texture* odd = build_texture(field(j, "odd"), ctx);
        return ctx.scene.create_texture<CheckerTexture>(scale, even, odd);
    }

    if (type == "noise") {
        const Float scale = read_number(field(j, "scale"));
        if (scale <= 0) throw SceneError(std::format("Noise texture scale must be positive, got {}", scale));

        const std::uint64_t seed = j.contains("seed") ? read_seed(j.at("seed")) : 0;
        Sampler sampler(sampler_seed(seed, 0, 0));
        return ctx.scene.create_texture<NoiseTexture>(scale, sampler);
    }

    if (type == "image") {
        const std::string& filename = read_string(field(j, "filename"));
        if (filename.empty()) throw SceneError("Image texture filename must not be empty");

        // If filename is absolute, operator/ automatically ignores base_dir (intended behavior).
        const std::filesystem::path resolved = ctx.base_dir / filename;
        return ctx.scene.create_texture<ImageTexture>(resolved.string());
    }

    throw SceneError(std::format("Unknown texture type: '{}'", type));
}

[[nodiscard]] const Texture* find_texture(const LoadContext& ctx, const Json& j) {
    const std::string& name = read_string(field(j, "texture"));

    const auto it = ctx.textures.find(name);
    if (it == ctx.textures.end())
        throw SceneError(std::format("Undefined texture '{}'. Defined textures: {}", name, available_names(ctx.textures)));
    return it->second;
}

[[nodiscard]] const Material* find_material(const LoadContext& ctx, const Json& j, std::string_view key) {
    const std::string& name = read_string(field(j, key));

    const auto it = ctx.materials.find(name);
    if (it == ctx.materials.end())
        throw SceneError(std::format("Undefined material '{}'. Defined materials: {}", name, available_names(ctx.materials)));
    return it->second;
}

[[nodiscard]] const Hittable* build_child(const Json& j, LoadContext& ctx);

[[nodiscard]] const Hittable* build_object(const Json& j, LoadContext& ctx) {
    const std::string& type = read_string(field(j, "type"));
    const Hittable* result = nullptr;

    if (type == "sphere") {
        const Vec3 center = read_vec3(field(j, "center"));
        const Float radius = read_number(field(j, "radius"));
        if (radius <= 0) throw SceneError(std::format("Sphere radius must be positive, got {}", radius));

        const Material* mat = find_material(ctx, j, "material");
        if (j.contains("center_end")) {
            const Vec3 center_end = read_vec3(j.at("center_end"));
            result = ctx.scene.create_object<Sphere>(center, center_end, radius, mat);
        } else
            result = ctx.scene.create_object<Sphere>(center, radius, mat);
    } else if (type == "quad") {
        const Vec3 q = read_vec3(field(j, "q"));
        const Vec3 u = read_vec3(field(j, "u"));
        const Vec3 v = read_vec3(field(j, "v"));
        const Material* mat = find_material(ctx, j, "material");

        result = ctx.scene.create_object<Quad>(q, u, v, mat);
    } else if (type == "box") {
        const Vec3 a = read_vec3(field(j, "a"));
        const Vec3 b = read_vec3(field(j, "b"));
        const Material* mat = find_material(ctx, j, "material");

        result = box(ctx.scene.object_arena(), a, b, mat);
    } else if (type == "group") {
        const Json& children = field(j, "children");
        if (!children.is_array() || children.empty()) throw SceneError("Field 'children' must be a non-empty array");

        HittableList list;
        for (const auto& element : children) {
            list.add(build_child(element, ctx));
        }

        result = ctx.scene.create_object<HittableList>(std::move(list));
    } else if (type == "translate") {
        const Hittable* child = build_child(field(j, "object"), ctx);
        const Vec3 offset = read_vec3(field(j, "offset"));

        result = ctx.scene.create_object<Translate>(child, offset);
    } else if (type == "rotate_y") {
        const Hittable* child = build_child(field(j, "object"), ctx);
        const Float angle = read_number(field(j, "angle"));

        result = ctx.scene.create_object<RotateY>(child, angle);
    } else if (type == "constant_medium") {
        const Hittable* boundary = build_child(field(j, "boundary"), ctx);
        const Float density = read_number(field(j, "density"));
        if (density <= 0) throw SceneError(std::format("Density must be positive, got {}", density));
        const Material* phase_function = find_material(ctx, j, "phase_function");

        result = ctx.scene.create_object<ConstantMedium>(boundary, density, phase_function);
    } else {
        throw SceneError(std::format("Unknown object type '{}'", type));
    }

    if (j.contains("name")) ctx.objects[read_string(j.at("name"))] = result;
    return result;
}

[[nodiscard]] const Hittable* build_child(const Json& j, LoadContext& ctx) {
    if (j.is_string()) {
        const std::string& name = read_string(j);

        const auto it = ctx.objects.find(name);
        if (it == ctx.objects.end())
            throw SceneError(std::format("Undefined object '{}'. Defined objects: {}", name, available_names(ctx.objects)));
        return it->second;
    }

    if (j.is_object()) return build_object(j, ctx);

    throw SceneError("Child node must be either a string or a JSON object");
}

void parse_textures(const Json& doc, LoadContext& ctx) {
    // Optional; an absent block means no textures at all.
    if (!doc.contains("textures")) return;

    const Json& textures = doc.at("textures");
    if (!textures.is_object()) throw SceneError("Field 'textures' must be a JSON object");

    for (const auto& [name, value] : textures.items()) {
        ctx.textures[name] = build_texture(value, ctx);
    }
}

void parse_materials(const Json& doc, LoadContext& ctx) {
    const Json& materials = field(doc, "materials");
    if (!materials.is_object()) throw SceneError("Field 'materials' must be a JSON object");

    for (const auto& [name, value] : materials.items()) {
        const std::string& type = read_string(field(value, "type"));
        const Material* material = nullptr;

        if (type == "lambertian") {
            material = ctx.scene.create_material<Lambertian>(find_texture(ctx, value));
        } else if (type == "diffuse_light") {
            material = ctx.scene.create_material<DiffuseLight>(find_texture(ctx, value));
        } else if (type == "isotropic") {
            material = ctx.scene.create_material<Isotropic>(find_texture(ctx, value));
        } else if (type == "metal") {
            const Color albedo = read_color(field(value, "albedo"));
            const Float fuzz = value.contains("fuzz") ? read_number(value.at("fuzz")) : 0.0_f;

            if (fuzz < 0 || fuzz > 1)
                throw SceneError(std::format("Material '{}': fuzz must be within [0, 1], got {}", name, fuzz));

            material = ctx.scene.create_material<Metal>(albedo, fuzz);
        } else if (type == "dielectric") {
            const Float refraction_index = read_number(field(value, "refraction_index"));

            if (refraction_index <= 0)
                throw SceneError(std::format("Material '{}': refraction_index must be positive, got {}", name, refraction_index));

            material = ctx.scene.create_material<Dielectric>(refraction_index);
        } else {
            throw SceneError(std::format("Material '{}': unknown type '{}'", name, type));
        }

        ctx.materials[name] = material;
    }
}

void parse_objects(const Json& doc, LoadContext& ctx) {
    const Json& objects = field(doc, "objects");
    if (!objects.is_array() || objects.empty()) throw SceneError("Field 'objects' must be a non-empty array");

    for (const auto& element : objects) {
        if (!element.is_object()) throw SceneError("Top-level entries in 'objects' must be definitions, not names");
        ctx.scene.add_object(build_object(element, ctx));
    }
}

void parse_importance_targets(const Json& doc, LoadContext& ctx) {
    // Optional; an absent list disables direct light sampling entirely.
    if (!doc.contains("importance_targets")) return;

    const Json& targets = doc.at("importance_targets");
    if (!targets.is_array()) throw SceneError("Field 'importance_targets' must be an array");

    for (const auto& element : targets) {
        const std::string& name = read_string(element);

        const auto it = ctx.objects.find(name);
        if (it == ctx.objects.end())
            throw SceneError(std::format("Undefined object '{}'. Defined objects: {}", name, available_names(ctx.objects)));

        // Cross-cast between unrelated bases Sampleable and Hittable; returns null if the object is wrapped or grouped.
        const Sampleable* target = dynamic_cast<const Sampleable*>(it->second);
        if (target == nullptr)
            throw SceneError(std::format("Object '{}' is not sampleable: only spheres and quads can be importance targets", name));

        ctx.scene.add_importance_target(target);
    }
}

void parse_camera(const Json& doc, LoadContext& ctx) {
    const Json& camera = field(doc, "camera");
    if (!camera.is_object()) throw SceneError("Field 'camera' must be a JSON object");

    const Vec3 lookfrom = read_vec3(field(camera, "lookfrom"));
    const Vec3 lookat = read_vec3(field(camera, "lookat"));

    const Vec3 vup = camera.contains("vup") ? read_vec3(camera.at("vup")) : Vec3(0, 1, 0);

    const Float vfov = read_number(field(camera, "vfov"));
    if (vfov <= 0 || vfov >= 180) throw SceneError(std::format("Camera vfov must be within (0, 180) degrees, got {}", vfov));

    Float defocus_angle = 0.0_f;
    if (camera.contains("defocus_angle")) {
        defocus_angle = read_number(field(camera, "defocus_angle"));
        if (defocus_angle < 0) throw SceneError(std::format("Camera defocus angle must be non-negative, got {}", defocus_angle));
    }

    Float focus_dist = 10.0_f;
    if (camera.contains("focus_dist")) {
        focus_dist = read_number(field(camera, "focus_dist"));
        if (focus_dist <= 0) throw SceneError(std::format("Camera focus distance must be positive, got {}", focus_dist));
    }

    ctx.scene.camera.lookfrom = lookfrom;
    ctx.scene.camera.lookat = lookat;
    ctx.scene.camera.vup = vup;
    ctx.scene.camera.vfov = vfov;
    ctx.scene.camera.defocus_angle = defocus_angle;
    ctx.scene.camera.focus_dist = focus_dist;
}

void parse_render(const Json& doc, LoadContext& ctx) {
    const Json& render = field(doc, "render");
    if (!render.is_object()) throw SceneError("Field 'render' must be a JSON object");

    const int width = read_int(field(render, "width"));
    if (width <= 0) throw SceneError(std::format("Render width must be positive, got {}", width));

    const int height = read_int(field(render, "height"));
    if (height <= 0) throw SceneError(std::format("Render height must be positive, got {}", height));

    const int spp = read_int(field(render, "samples_per_pixel"));
    if (spp <= 0) throw SceneError(std::format("Render samples per pixel must be positive, got {}", spp));

    const int max_depth = read_int(field(render, "max_depth"));
    if (max_depth <= 0) throw SceneError(std::format("Render max depth must be positive, got {}", max_depth));

    const Color background = read_color(field(render, "background"));

    std::uint64_t seed = 0;
    if (render.contains("seed")) seed = read_seed(render.at("seed"));

    ToneMapSettings settings{}; // defaults -> exposure = 1, operator = none
    if (render.contains("tone_map")) {
        const Json& tm = render.at("tone_map");
        if (!tm.is_object()) throw SceneError("Field 'tone_map' must be a JSON object");

        if (tm.contains("exposure")) {
            const Float exposure = read_number(tm.at("exposure"));
            if (exposure <= 0) throw SceneError(std::format("Tone map exposure must be positive, got {}", exposure));
            settings.exposure = exposure;
        }

        if (tm.contains("operator")) {
            const std::string& op_name = read_string(tm.at("operator"));

            if (op_name == "none")
                settings.op = ToneMapOperator::none;
            else if (op_name == "reinhard")
                settings.op = ToneMapOperator::reinhard;
            else if (op_name == "aces")
                settings.op = ToneMapOperator::aces;
            else
                throw SceneError(std::format("Unknown tone map operator '{}'. Valid operators: none, reinhard, aces", op_name));
        }
    }

    ctx.scene.render.image_width = width;
    ctx.scene.render.image_height = height;
    ctx.scene.render.samples_per_pixel = spp;
    ctx.scene.render.max_depth = max_depth;
    ctx.scene.render.background = background;
    ctx.scene.render.seed = seed;
    ctx.scene.render.tone_map = settings;
}

} // namespace

SceneError::~SceneError() = default;

Scene load_scene(const std::filesystem::path& path) {
    std::ifstream stream(path);
    if (!stream.is_open()) throw SceneError(std::format("Cannot open scene file: '{}'", path.string()));

    Json parsed;
    try {
        parsed = Json::parse(stream);
    } catch (const Json::parse_error& e) {
        throw SceneError(std::format("Cannot parse scene file: '{}': {}", path.string(), e.what()));
    }

    if (!parsed.is_object()) throw SceneError("Root JSON value must be an object");
    if (!parsed.contains("version")) throw SceneError("Missing 'version' key in JSON");

    const Json& version = parsed.at("version");
    if (!version.is_number_integer() || version.get<int>() != 1)
        throw SceneError(std::format("Unsupported scene version in '{}': expected 1", path.string()));

    Scene scene;
    LoadContext ctx{scene, path.parent_path(), {}, {}, {}};

    parse_camera(parsed, ctx);
    parse_render(parsed, ctx);
    parse_textures(parsed, ctx);
    parse_materials(parsed, ctx);
    parse_objects(parsed, ctx);
    parse_importance_targets(parsed, ctx);
    scene.build_bvh();

    return scene;
}

} // namespace pt
