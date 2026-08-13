# BVH Baseline

Instrumented measurement of the BVH as it stands before any acceleration work:
midpoint split on the longest axis, pointer-based tree, recursive traversal.

- Binary: `build/release-stats/pathtracer` (`release-stats` preset, `PT_ENABLE_STATS=ON`)
- CPU: 11th Gen Intel Core i7-11700K @ 3.60GHz, WSL Ubuntu
- Compiler: clang 18.1.3, `Float = double`
- Measured: 2026-08-13

| Scene | Res | spp | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Ray queries | Render (s) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| area_lights | 240x135 | 16 | 1 | 3 | 4 | 1 | 0.001 | 3.0 | 2.5 | 825,054 | 0.13 |
| checkered_spheres | 240x135 | 16 | 1 | 1 | 2 | 0 | 0.000 | 1.0 | 2.0 | 1,737,766 | 0.21 |
| cornell_box | 200x200 | 16 | 1 | 7 | 8 | 2 | 0.004 | 6.9 | 6.4 | 3,504,869 | 0.81 |
| cornell_smoke | 200x200 | 16 | 1 | 7 | 8 | 2 | 0.001 | 6.9 | 7.0 | 3,696,923 | 1.32 |
| earth | 240x135 | 16 | 1 | 1 | 2 | 0 | 0.001 | 1.0 | 1.5 | 728,854 | 0.05 |
| neon_cathedral | 240x135 | 16 | 6 | 52 | 58 | 3 | 0.006 | 43.4 | 24.0 | 3,520,104 | 4.21 |
| perlin_spheres | 240x135 | 16 | 1 | 1 | 2 | 0 | 0.000 | 1.0 | 2.0 | 1,178,512 | 0.22 |
| quads | 200x200 | 16 | 1 | 5 | 6 | 2 | 0.001 | 4.2 | 2.9 | 1,167,037 | 0.12 |
| random_spheres | 240x135 | 16 | 1 | 511 | 512 | 8 | 0.132 | 45.3 | 5.2 | 1,382,099 | 0.57 |
| mesh_showcase * | 240x135 | 16 | 3 | 23 | 26 | 3 | 0.005 | 9.8 | 8.0 | 1,155,723 | 0.20 |
| showcase * | 240x135 | 16 | 3 | 1547 | 1550 | 9 | 0.544 | 21.2 | 4.9 | 1,198,040 | 0.61 |

`*` outside the golden image set; measured for triangle and large-scene coverage.

## Reading the numbers

- `random_spheres` and `showcase` are the only scenes where the tree earns its
  keep: ~1% and ~0.3% of primitives reach an intersection test. Spatial spread
  matters more than size -- `showcase` has 3x the nodes of `random_spheres` and
  visits half as many per ray, because its geometry is clustered.
- `neon_cathedral` is the worst case: 43.4 of 52 nodes visited per ray (~83%).
  Nested group bounds overlap heavily, so almost nothing is culled.
- Single-primitive leaves are linked twice. A node holding one object assigns it
  to both children, so every visit tests it twice: `earth` is a single sphere at
  1.5 leaf tests/ray, `quads` reports 6 links for 5 quads.
- Small scenes get no acceleration at all. One node and two leaves means the
  tree adds an AABB test and culls nothing.
- Build time is irrelevant at this scale (0.544 ms at 1547 nodes).

## Definitions

- `Nodes` / `Leaves` are counted once per tree built; a mesh subtree shared by
  several parents is built and counted once, but traversed on every visit.
- `Leaves` counts child links, not distinct primitives (see the double-linking
  note above).
- `Leaf tests` measures the BVH's own work. A leaf may itself be a
  `HittableList` -- a box is six quads behind one link -- and those tests are
  not counted.
- `Ray queries` counts `world_.hit()` calls only: primary rays plus every
  bounce. `ConstantMedium` issues nested `hit()` calls that never pass through
  `world_`, which inflates both ratios for `cornell_smoke`.

## Method

One run per scene, resolutions and spp from `tests/golden/manifest.txt`. The
counters are deterministic under a fixed seed, so repeat runs are unnecessary.
Render time is indicative only -- it is wall-clock on a shared machine and is
not a comparison metric.
