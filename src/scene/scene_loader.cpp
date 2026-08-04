#include "pt/scene/scene_loader.hpp"
#include "pt/scene/scene.hpp"
#include <filesystem>
#include <format>
#include <fstream>
#include <nlohmann/json.hpp>
#include <nlohmann/json_fwd.hpp>

namespace pt {

using Json = nlohmann::json;

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

    return Scene{}; // camera/render/textures/materials/objects: subsequent steps
}

} // namespace pt
