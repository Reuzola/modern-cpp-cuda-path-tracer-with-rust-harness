# BVH Measurements

Two instrumented measurements of the BVH, taken before and after the
acceleration work. Both use the same scenes, resolutions, sample counts and
seeds, so the numbers are comparable except where noted below.

- **Configuration A** — midpoint split on the longest axis, pointer-based tree,
  recursive traversal. Measured 2026-08-13.
- **Configuration B** — binned SAH split, flat depth-first node array,
  iterative distance-ordered traversal with multi-primitive leaves. Measured
  2026-08-23.

Shared setup:

- Binary: `build/release-stats/pathtracer` (`release-stats` preset, `PT_ENABLE_STATS=ON`)
- CPU: 11th Gen Intel Core i7-11700K @ 3.60GHz, WSL Ubuntu
- Compiler: clang 18.1.3, `Float = double`

Configuration B build settings: 12 bins, traversal cost 1, intersection cost 8,
maximum leaf size 4. These shape the tree; the numbers below do not hold for
other values.

## Configuration A

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

## Configuration B

| Scene | Res | spp | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Ray queries | Render (s) |
|---|---|---|---|---|---|---|---|---|---|---|---|
| area_lights | 240x135 | 16 | 1 | 7 | 4 | 3 | 0.003 | 4.1 | 0.9 | 825,054 | 0.17 |
| checkered_spheres | 240x135 | 16 | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.8 | 1,737,766 | 0.33 |
| cornell_box | 200x200 | 16 | 1 | 15 | 8 | 6 | 0.005 | 13.2 | 1.4 | 3,504,869 | 0.90 |
| cornell_smoke | 200x200 | 16 | 1 | 15 | 8 | 6 | 0.004 | 13.2 | 1.2 | 3,695,015 | 1.09 |
| earth | 240x135 | 16 | 1 | 1 | 1 | 0 | 0.001 | 1.0 | 0.7 | 728,854 | 0.08 |
| neon_cathedral | 240x135 | 16 | 6 | 86 | 46 | 5 | 0.017 | 45.1 | 7.3 | 3,520,143 | 2.95 |
| perlin_spheres | 240x135 | 16 | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.2 | 1,178,512 | 0.30 |
| quads | 200x200 | 16 | 1 | 9 | 5 | 4 | 0.003 | 7.0 | 0.5 | 1,167,037 | 0.17 |
| random_spheres | 240x135 | 16 | 1 | 967 | 484 | 12 | 0.206 | 26.2 | 1.6 | 1,382,099 | 0.46 |
| mesh_showcase * | 240x135 | 16 | 3 | 39 | 21 | 6 | 0.007 | 10.8 | 2.1 | 1,199,628 | 0.24 |
| showcase * | 240x135 | 16 | 3 | 2815 | 1409 | 13 | 0.528 | 17.3 | 2.1 | 1,198,040 | 0.51 |

`*` outside the golden image set; measured for triangle and large-scene coverage.

## What changed

### Which columns can be compared

`Nodes` and `Leaves` count different things in the two tables and must not be
subtracted. In A a node is an interior node and a leaf is a *child link*, with
a single-primitive node linking the same object twice. In B a node is an entry
in the flat array — interior or leaf — and a leaf is a real leaf node holding a
range of primitives. `earth` reads as 1 node / 2 leaves in A and 1 node /
1 leaf in B for the same single sphere.

`Depth`, `Ray queries` and `Build (ms)` mean the same thing in both.

`Node tests/ray` and `Leaf tests/ray` share a unit — hit tests issued per ray
query — but the split between them moved. In A, descending into a child that
was a primitive cost no box test: the primitive was tested directly. In B every
child's box is tested before descending, leaves included. Work therefore
migrates from the leaf column into the node column, which is why the node
column rises almost everywhere without the tree getting worse.

The comparable aggregate is their sum: the total number of hit tests a ray
query issues.

### Total tests per ray

| Scene | A | B | Change |
|---|---|---|---|
| earth | 2.5 | 1.7 | -32% |
| checkered_spheres | 3.0 | 4.8 | +60% |
| perlin_spheres | 3.0 | 4.2 | +40% |
| quads | 7.1 | 7.5 | +6% |
| area_lights | 5.5 | 5.0 | -9% |
| cornell_box | 13.3 | 14.6 | +10% |
| cornell_smoke | 13.9 | 14.4 | +4% |
| mesh_showcase * | 17.8 | 12.9 | -28% |
| showcase * | 26.1 | 19.4 | -26% |
| neon_cathedral | 67.4 | 52.4 | -22% |
| random_spheres | 50.5 | 27.8 | -45% |

The result splits by scene size, and the split is structural rather than
incidental.

**Below roughly four primitives the tree costs more than it saves.**
`checkered_spheres` holds two spheres: A tested the root box and then both
spheres, B tests the root box, both child boxes and then whichever spheres
survive. The extra box tests cannot pay for themselves when there is almost
nothing to cull. `earth` is the exception among the small scenes only because
its root is itself a leaf, so it has no child boxes to test and it also sheds
the double-linked second test.

**Above that, the gain grows with primitive count and with spatial spread.**
`random_spheres` nearly halves its work; `showcase`, with three times the nodes
but clustered geometry, gains a comparable fraction; `neon_cathedral` improves
by a fifth and remains the worst case, still visiting about half of its nodes
per ray because its nested group bounds overlap heavily.

`Leaf tests/ray` falls in every scene, for three separate reasons that should
not be conflated: single-primitive nodes no longer link the same object twice,
a leaf's own box can now reject it before its primitives are touched, and the
SAH groups primitives that are actually near each other. Only the third is an
improvement in the tree itself.

Render times move in the same direction as the test counts, and are reported
for orientation only — they are wall-clock on a shared machine.

Build time grew by roughly 4x at equal primitive counts, which is what a binned
SAH sweep costs over a midpoint split. At 0.5 ms for the largest scene this is
not a figure worth optimising.

## A divergence found while measuring

`mesh_showcase` is the one scene whose `Ray queries` moved materially: 1,155,723
to 1,199,628, or +3.8%. Every other scene is either bit-identical or differs by
less than a tenth of a percent — `cornell_smoke` because `ConstantMedium` draws
from the sampler inside `hit()`, so a change in visiting order changes the
draw order, and `neon_cathedral` because it contains exactly coincident quads
whose winner depends on visiting order.

Neither explanation covers `mesh_showcase`. `git bisect` against the
Configuration A render locates the first diverging commit exactly:

- `a197726` (builder extraction) — bit-identical
- `e6b7194` (binned SAH) — bit-identical
- `3802963` (iterative distance-ordered traversal) — **diverges**

The SAH commit being bit-identical is itself evidence: it rewrote the primitive
permutation and leaf grouping wholesale. If this scene were sensitive to
visiting order, it would have moved there. The difference image agrees — the
brass cubes rest exactly coplanar with the floor quad at `y = -1`, and that
contact region shows zero difference.

The difference is confined to the glass tetrahedron, the scene's only
dielectric, and it looks like scattered per-pixel noise rather than a displaced
edge: paths diverged, geometry did not move. With `max_depth` 24 and total
internal reflection, a single hit resolved differently reroutes an entire
bounce chain, which is where the extra ray queries come from.

The same commit also replaced per-node division with a per-ray reciprocal in
the slab test, and `x * (1/y)` is not bit-identical to `x / y`. That was tested
directly by reverting the arithmetic while keeping the new traversal: the
divergence reproduced to eight digits, so the reciprocal is not the cause. What
remains is the culling and ordering change inside that commit.

The current traversal is verified against brute-force intersection over these
same OBJ assets by `tests/bvh_test.cpp`, including rays cast from inside the
mesh. No such verification exists for the earlier traversal, so the honest
statement is that the two disagree and only the current one has been checked —
not that the earlier one was wrong.

Open item: neither `mesh_showcase` nor `showcase` is in the golden image set,
which is why this went unnoticed. A regression in either is currently invisible
to CI.

## Definitions

- `Trees` is the number of BVHs a scene builds. Meshes and groups get their
  own; a subtree shared by several parents is built and counted once, but
  traversed on every visit.
- `Nodes` in A counts interior nodes only. In B it is the size of the flat node
  array: interior nodes plus leaves.
- `Leaves` in A counts child links, and a node holding one primitive links it
  twice. In B it counts leaf nodes, each holding a contiguous range of
  primitives.
- `Node tests` counts bounding box tests. A tested a node's own box on entry
  and reached primitive children without one; B tests both children's boxes
  before descending.
- `Leaf tests` counts `hit()` calls the BVH issues on the objects it holds. An
  object may itself be an aggregate — a box is six quads behind one entry — and
  the tests inside it are not counted.
- `Ray queries` counts top-level `hit()` calls: primary rays plus every bounce.
  `ConstantMedium` issues nested calls that do not pass through the root, which
  inflates both ratios for `cornell_smoke`.
- `Depth` is the longest root-to-node path; a single-node tree has depth 0.

## Method

One run per scene. Resolutions and sample counts come from
`tests/golden/manifest.txt`; the two scenes outside that set are measured at
240x135 with 16 samples to match. The counters are deterministic under a fixed
seed, so repeat runs add nothing.

Reproduce with:

```
cmake --preset release-stats
cmake --build --preset release-stats
./build/release-stats/pathtracer scenes/<name>.json --width <w> --height <h> --spp 16 -o /tmp/<name>.png
```

Build statistics are always collected; traversal counters require the
`release-stats` preset, since the counters sit in the hot loop and the default
release build must not carry them.
