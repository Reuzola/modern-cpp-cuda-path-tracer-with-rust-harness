#pragma once
#include "pt/geometry/bvh.hpp"
#include "pt/geometry/level.hpp"
#include "pt/geometry/top_level_bvh.hpp"
#include <initializer_list>
#include <span>

namespace pt_test {

// A top level over primitives handed over directly, with no instance among them: how a test builds a world,
// a medium's boundary or a lone tree without going through a scene graph. The objects must outlive it.
[[nodiscard]] inline pt::TopLevelBvh top_level_of(std::span<const pt::LevelItem> items, const pt::BvhBuildSettings& settings = {}) {
    return pt::TopLevelBvh(items, {}, settings);
}

[[nodiscard]] inline pt::TopLevelBvh top_level_of(std::initializer_list<pt::LevelItem> items, const pt::BvhBuildSettings& settings = {}) {
    return top_level_of(std::span<const pt::LevelItem>(items.begin(), items.size()), settings);
}

} // namespace pt_test
