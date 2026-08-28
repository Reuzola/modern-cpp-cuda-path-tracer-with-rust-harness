#pragma once
#include "pt/post/tonemap.hpp"

namespace pt {

// Seeded from the scene, then owned by the viewer: editing these must not mutate the loaded Scene.
struct ViewerControls {
    ToneMapSettings tone_map{};
    int max_depth{};
    int target_spp{};
};

// display: re-tone-map the cached film. accumulation: the estimator is invalid, restart it.
struct ControlChange {
    bool display{};
    bool accumulation{};
};

} // namespace pt
