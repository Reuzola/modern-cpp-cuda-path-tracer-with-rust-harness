#include "pt/scene/obj_loader.hpp"
#include "pt/geometry/mesh.hpp"
#include "pt/math/scalar.hpp"
#include "pt/scene/scene_error.hpp"
#include "pt/util/log.hpp"
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <functional>
#include <string_view>
#include <tiny_obj_loader.h>
#include <unordered_map>
#include <vector>

namespace pt {

namespace {

// One corner of a face, as OBJ addresses it. Two corners collapse into a single
// vertex only if all three attribute indices match; -1 marks an absent attribute.
struct VertexKey {
    int position{};
    int normal{};
    int uv{};

    [[nodiscard]] bool operator==(const VertexKey&) const noexcept = default;
};

// hash_combine, as popularised by Boost: mixing the golden-ratio constant with
// shifts keeps neighbouring index triples from colliding in the same bucket.
struct VertexKeyHash {
    [[nodiscard]] std::size_t operator()(const VertexKey& key) const noexcept {
        std::size_t seed = 0;
        for (const int value : {key.position, key.normal, key.uv}) {
            seed ^= std::hash<int>{}(value) + 0x9e3779b9U + (seed << 6U) + (seed >> 2U);
        }
        return seed;
    }
};

[[nodiscard]] std::string_view trim_trailing_newlines(std::string_view text) noexcept {
    while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) {
        text.remove_suffix(1);
    }
    return text;
}

[[nodiscard]] bool index_in_range(int index, std::size_t array_size, std::size_t stride) noexcept {
    return index >= 0 && stride * static_cast<std::size_t>(index) + stride <= array_size;
}

} // namespace

MeshData load_obj(const std::filesystem::path& path) {
    tinyobj::ObjReaderConfig config;

    // MeshData's index buffer is triangles-only, so n-gons must be split here.
    config.triangulate = true;

    // Skip non-standard OBJ vertex colors to avoid populating unused buffers.
    config.vertex_color = false;

    // Leave search path untouched; .mtl material files are intentionally not parsed.

    tinyobj::ObjReader reader;
    const bool parsed = reader.ParseFromFile(path.string(), config);

    if (!reader.Warning().empty())
        log_warning("OBJ file '{}': {}", path.string(), trim_trailing_newlines(reader.Warning()));
    if (!parsed)
        throw SceneError(std::format("Cannot load OBJ file '{}': {}", path.string(), trim_trailing_newlines(reader.Error())));

    const tinyobj::attrib_t& attrib = reader.GetAttrib();
    const std::vector<tinyobj::shape_t>& shapes = reader.GetShapes();

    bool keep_normals = !attrib.normals.empty();
    bool keep_uvs = !attrib.texcoords.empty();
    std::size_t corner_count{0};

    for (const tinyobj::shape_t& shape : shapes) {
        corner_count += shape.mesh.indices.size();

        for (const tinyobj::index_t& index : shape.mesh.indices) {
            if (index.normal_index < 0) keep_normals = false;
            if (index.texcoord_index < 0) keep_uvs = false;
        }
    }

    if (!attrib.normals.empty() && !keep_normals)
        log_warning("OBJ file '{}': dropping vertex normals, some faces do not reference them", path.string());

    if (!attrib.texcoords.empty() && !keep_uvs)
        log_warning("OBJ file '{}': dropping UVs, some faces do not reference them", path.string());

    MeshData data;
    data.indices.reserve(corner_count);
    data.positions.reserve(attrib.vertices.size() / 3);

    std::unordered_map<VertexKey, std::uint32_t, VertexKeyHash> lookup;
    lookup.reserve(attrib.vertices.size() / 3);

    for (const tinyobj::shape_t& shape : shapes) {
        for (const tinyobj::index_t& index : shape.mesh.indices) {
            const VertexKey key{
                .position = index.vertex_index,
                .normal = keep_normals ? index.normal_index : -1,
                .uv = keep_uvs ? index.texcoord_index : -1
            };

            const auto [entry, inserted] = lookup.try_emplace(key, static_cast<std::uint32_t>(data.positions.size()));

            if (inserted) {
                if (!index_in_range(key.position, attrib.vertices.size(), 3) ||
                    (keep_normals && !index_in_range(key.normal, attrib.normals.size(), 3)) ||
                    (keep_uvs && !index_in_range(key.uv, attrib.texcoords.size(), 2))) {
                    throw SceneError(std::format("OBJ file '{}': attribute index out of range", path.string()));
                }

                const std::size_t position_base = 3 * static_cast<std::size_t>(key.position);
                data.positions.emplace_back(
                    static_cast<Float>(attrib.vertices[position_base]),
                    static_cast<Float>(attrib.vertices[position_base + 1]),
                    static_cast<Float>(attrib.vertices[position_base + 2]));

                if (keep_normals) {
                    const std::size_t normal_base = 3 * static_cast<std::size_t>(key.normal);
                    data.normals.emplace_back(
                        static_cast<Float>(attrib.normals[normal_base]),
                        static_cast<Float>(attrib.normals[normal_base + 1]),
                        static_cast<Float>(attrib.normals[normal_base + 2]));
                }

                if (keep_uvs) {
                    // OBJ's v axis already matches what ImageTexture expects; it flips v itself.
                    const std::size_t uv_base = 2 * static_cast<std::size_t>(key.uv);
                    data.uvs.push_back(Uv{
                        .u = static_cast<Float>(attrib.texcoords[uv_base]),
                        .v = static_cast<Float>(attrib.texcoords[uv_base + 1])
                    });
                }
            }

            data.indices.push_back(entry->second);
        }
    }

    if (data.indices.empty())
        throw SceneError(std::format("OBJ file '{}' contains no triangles", path.string()));

    log_info("Loaded OBJ '{}': {} triangles, {} vertices", path.string(), data.indices.size() / 3, data.positions.size());
    return data;
}

} // namespace pt
