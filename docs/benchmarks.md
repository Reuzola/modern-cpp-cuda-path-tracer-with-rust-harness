# Benchmarks

The performance baseline the renderer is measured against, recorded before any
optimisation work on the render loop begins. Every later measurement is only
meaningful next to these numbers, so this file also states what a comparison
requires and what invalidates one.

This is not a comparison against other renderers. It measures this renderer
against itself, over a fixed workload, on named machines.

The renderer is single threaded today. Every figure below is one core.

## Machines

| | Reference | Secondary |
|---|---|---|
| CPU | Intel Core i7-11700K @ 3.60 GHz (Rocket Lake) | Snapdragon X X1E-26-100 (Qualcomm Oryon) |
| Architecture | x86_64 | aarch64 |
| Logical cores | 16 | 8 |
| OS | Ubuntu 24.04 (WSL2) | Ubuntu 24.04 (WSL2) |
| Compiler | Clang 18.1.3 | Clang 18.1.3 |
| Scalar type | `double` | `double` |

The reference machine is the one to compare against. The secondary machine is
recorded because it runs the same code on a different instruction set, which is
the only cheap way to tell an algorithmic improvement apart from one that
happens to suit a single microarchitecture.

Both are laptops or desktops running a general purpose OS, not isolated
benchmarking hosts. Treat differences under about two percent as noise.

## Method

The workload is `benchmarks/manifest.txt`: one row per scene, giving the
resolution and sample count. Everything else — maximum depth, seed, background,
tone mapping — comes from the scene file, so a record describes exactly the
work the renderer was asked to do.

Sample counts were chosen so that each scene takes roughly ten seconds: much
below that and scheduler noise hides the improvements worth finding, much above
it and a full sweep stops getting run. Both machines use the file unchanged,
since adjusting a row per machine would make the two tables incomparable.

Each scene is measured twice, from two different builds:

- The `release` build supplies timing. Three runs; the **minimum** is reported,
  because a slow run means interference and never a faster renderer. The full
  set of runs is kept in the raw record.
- The `release-stats` build supplies the traversal counters. One run: the
  counters are deterministic under a fixed seed, so repeating them costs time
  and adds nothing.

They are separate builds because the counters sit in the traversal hot loop. A
build carrying them cannot also be timed honestly — see the measured cost
below.

Observed spread across the three timed runs was under 1.4% on both machines for
every scene but two: `earth` at 2.3% and `gilded_orrery` at 3.1%, both on the
reference machine.

Records are written as one JSON object per line to `out/benchmarks.ndjson`.
Each object is self-describing — machine, build configuration, scene settings,
timings, BVH statistics — so runs taken months apart can be concatenated and
still be told apart.

Where that time goes inside the renderer is a separate measurement, in
[profiling.md](profiling.md).

### Reproduce

```bash
cmake --preset release       && cmake --build --preset release
cmake --preset release-stats && cmake --build --preset release-stats
scripts/run-benchmarks.sh
```

A single scene, without the script:

```bash
./build/release/pathtracer scenes/cornell_box.json \
    --width 400 --height 400 --spp 49 --bench --bench-runs 3
```

The flags are described in [usage.md](usage.md).

### What invalidates a comparison

Any of these makes two records incomparable, and the older number has to be
remeasured rather than reused:

- A changed row in `benchmarks/manifest.txt` — different resolution or sample
  count is a different workload.
- A changed scene file, including geometry, materials, seed or maximum depth.
- A different build preset. `release-native` enables FMA contraction;
  `release-stats` carries the counters; a Debug figure is worse than none.
- A different scalar type. `Float = float` changes both speed and results.
- A different machine, or the same machine in a different thermal or power
  state.

Statistics and timing may be quoted from the same run only when both come from
the same record.

### Definitions

- **Trees** — the number of BVHs the scene builds. Meshes and groups get their
  own; a subtree shared by several parents is built and counted once, but
  traversed on every visit.
- **Nodes** — the size of the flat node array: interior nodes plus leaves.
- **Leaves** — leaf nodes, each holding a contiguous range of primitives.
- **Build** — total construction time over all trees.
- **Node tests/ray** — bounding box tests per ray query. Both children's boxes
  are tested before descending, leaves included.
- **Leaf tests/ray** — `hit()` calls the BVH issues on the objects it holds,
  per ray query. An object may itself be an aggregate — a box is six quads
  behind one entry — and the tests inside it are not counted.
- **Total** — the sum of the two, and the comparable aggregate: the number of
  hit tests one ray query issues.
- **Ray queries** — top-level `hit()` calls: primary rays plus every bounce.
- **Depth** — the longest root-to-node path; a single-node tree has depth 0.

## Baseline: x86_64

| Scene | Res | spp | Render (s) | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Total | Ray queries |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `area_lights` | 480x270 | 225 | 8.18 | 1 | 7 | 4 | 3 | 0.003 | 4.1 | 0.91 | 5.0 | 46,369,309 |
| `checkered_spheres` | 480x270 | 121 | 8.75 | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.80 | 4.8 | 52,536,489 |
| `cornell_box` | 400x400 | 49 | 9.92 | 1 | 15 | 8 | 6 | 0.004 | 13.2 | 1.38 | 14.6 | 42,910,672 |
| `cornell_smoke` | 400x400 | 36 | 12.22 | 1 | 11 | 6 | 4 | 0.003 | 9.4 | 0.84 | 10.2 | 33,294,066 |
| `earth` | 480x270 | 484 | 7.84 | 1 | 1 | 1 | 0 | 0.001 | 1.0 | 0.74 | 1.7 | 88,192,212 |
| `gilded_orrery` | 480x270 | 16 | 9.28 | 15 | 168,397 | 84,206 | 19 | 37.347 | 52.8 | 5.90 | 58.7 | 9,475,994 |
| `mesh_showcase` | 480x270 | 169 | 9.19 | 3 | 39 | 21 | 6 | 0.007 | 10.8 | 2.13 | 13.0 | 50,692,911 |
| `neon_cathedral` | 480x270 | 16 | 9.17 | 6 | 86 | 46 | 5 | 0.014 | 45.1 | 6.32 | 51.4 | 14,075,424 |
| `perlin_spheres` | 480x270 | 144 | 9.45 | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.24 | 4.2 | 42,436,119 |
| `quads` | 400x400 | 225 | 8.10 | 1 | 9 | 5 | 4 | 0.004 | 7.0 | 0.45 | 7.4 | 65,572,484 |
| `random_spheres` | 480x270 | 81 | 8.48 | 1 | 967 | 484 | 12 | 0.181 | 26.2 | 1.63 | 27.8 | 28,048,162 |
| `showcase` | 400x400 | 81 | 12.36 | 3 | 2,811 | 1,407 | 13 | 0.501 | 17.6 | 1.28 | 18.9 | 31,812,306 |

Two entries in the table are worth reading before drawing conclusions from it.

`gilded_orrery` is the only scene where BVH construction is visible at all:
37 ms, four orders of magnitude above every other scene, across fifteen trees
and 168k nodes. It is also the only scene whose render time is dominated by
traversal rather than shading — 58.7 hit tests per ray query, the highest in
the set, against just 9.5M ray queries.

`cornell_smoke` is the slowest scene per sample despite an eleven-node tree.
Its cost is volumetric: free-flight sampling runs per segment, and the BVH is
close to irrelevant to its total.

## Baseline: aarch64

Same workload, same source, same compiler version.

| Scene | Res | spp | Render (s) | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Total | Ray queries |
|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `area_lights` | 480x270 | 225 | 7.17 | 1 | 7 | 4 | 3 | 0.005 | 4.1 | 0.91 | 5.0 | 46,369,309 |
| `checkered_spheres` | 480x270 | 121 | 7.94 | 1 | 3 | 2 | 1 | 0.003 | 3.0 | 1.80 | 4.8 | 52,536,489 |
| `cornell_box` | 400x400 | 49 | 9.59 | 1 | 15 | 8 | 6 | 0.006 | 13.2 | 1.38 | 14.6 | 42,910,672 |
| `cornell_smoke` | 400x400 | 36 | 10.58 | 1 | 11 | 6 | 4 | 0.006 | 9.4 | 0.84 | 10.2 | 33,294,066 |
| `earth` | 480x270 | 484 | 7.09 | 1 | 1 | 1 | 0 | 0.002 | 1.0 | 0.74 | 1.7 | 88,192,212 |
| `gilded_orrery` | 480x270 | 16 | 8.67 | 15 | 168,397 | 84,206 | 19 | 45.107 | 53.3 | 5.93 | 59.2 | 9,475,994 |
| `mesh_showcase` | 480x270 | 169 | 8.88 | 3 | 39 | 21 | 6 | 0.010 | 10.8 | 2.13 | 13.0 | 50,693,021 |
| `neon_cathedral` | 480x270 | 16 | 8.41 | 6 | 86 | 46 | 5 | 0.017 | 45.1 | 6.32 | 51.4 | 14,075,424 |
| `perlin_spheres` | 480x270 | 144 | 7.95 | 1 | 3 | 2 | 1 | 0.003 | 3.0 | 1.24 | 4.2 | 42,436,119 |
| `quads` | 400x400 | 225 | 7.24 | 1 | 9 | 5 | 4 | 0.005 | 7.0 | 0.45 | 7.4 | 65,572,484 |
| `random_spheres` | 480x270 | 81 | 7.96 | 1 | 967 | 484 | 12 | 0.215 | 26.2 | 1.63 | 27.8 | 28,048,164 |
| `showcase` | 400x400 | 81 | 11.91 | 3 | 2,811 | 1,407 | 13 | 0.587 | 17.6 | 1.28 | 18.9 | 31,812,306 |

## Cross-architecture observations

The two machines do not produce the same numbers, and this set is not designed
to explain why. Three things matter for reading the tables above.

**Timing differs in both directions.** The secondary machine renders every
scene faster, by 3% to 16%, and builds the largest trees about 20% slower. No
cause is attributed here: establishing one would need measurements this set
does not take.

**Counters are reproducible within one architecture, not across two.** Eight of
the twelve scenes produce byte-identical counters on both machines. The other
four differ, by between two ray queries and 0.96% of node tests, for the reason
already documented for the reference images: `fmadd` is baseline on AArch64, so
Clang contracts multiply-add without being asked and intersection arithmetic
differs in the last unit in the last place. Comparing an optimisation's
counters against a baseline taken on the other machine is not a valid
comparison.

**The counters cost 1-4%, and only measurably on x86_64.** The instrumented
build against the plain one, same machine, same scene: a median of +2.1% on the
reference machine and +0.4% on the secondary, where five of the twelve scenes
came out *faster* with the counters compiled in — which is only possible if
their cost sits below that machine's noise floor. That is the concrete argument
for taking timing and counters from two separate builds. A single instrumented
run would have produced a table claiming the counters speed up rendering.

---

# Historical record: BVH construction and traversal

Two instrumented measurements of the BVH taken during the acceleration work,
kept for the record. **These numbers are frozen and are not the baseline.**
Compare new work against the tables above, not against these.

They are also stale in two specific ways, and were already stale when frozen:

- `cornell_smoke` and `showcase` were measured while volumes still participated
  in BVH traversal. Media sampling has since moved into the integrator, so
  their tree shapes and counters no longer describe the same renderer.
- `gilded_orrery` did not exist yet and does not appear.

What remains useful is the comparison between the two configurations and the
reasoning about which columns can be subtracted, which is why this is kept
rather than deleted.

Shared setup: `release-stats` preset, i7-11700K, Clang 18.1.3, `Float = double`,
one run per scene, resolutions and sample counts from the golden image set.

- **Configuration A** — midpoint split on the longest axis, pointer-based tree,
  recursive traversal. Measured 2026-08-13.
- **Configuration B** — binned SAH split (12 bins, traversal cost 1,
  intersection cost 8, maximum leaf size 4), flat depth-first node array,
  iterative distance-ordered traversal with multi-primitive leaves. Measured
  2026-08-23.

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
| mesh_showcase | 240x135 | 16 | 3 | 23 | 26 | 3 | 0.005 | 9.8 | 8.0 | 1,155,723 | 0.20 |
| showcase | 240x135 | 16 | 3 | 1547 | 1550 | 9 | 0.544 | 21.2 | 4.9 | 1,198,040 | 0.61 |

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
| mesh_showcase | 240x135 | 16 | 3 | 39 | 21 | 6 | 0.007 | 10.8 | 2.1 | 1,199,628 | 0.24 |
| showcase | 240x135 | 16 | 3 | 2815 | 1409 | 13 | 0.528 | 17.3 | 2.1 | 1,198,040 | 0.51 |

## Which columns can be compared

`Nodes` and `Leaves` count different things in the two tables and must not be
subtracted. In A a node is an interior node and a leaf is a *child link*, with
a single-primitive node linking the same object twice. In B a node is an entry
in the flat array — interior or leaf — and a leaf is a real leaf node holding a
range of primitives. `earth` reads as 1 node / 2 leaves in A and 1 node /
1 leaf in B for the same single sphere.

`Depth`, `Ray queries` and `Build (ms)` mean the same thing in both.

`Node tests/ray` and `Leaf tests/ray` share a unit but the split between them
moved. In A, descending into a child that was a primitive cost no box test: the
primitive was tested directly. In B every child's box is tested before
descending, leaves included. Work therefore migrates from the leaf column into
the node column, which is why the node column rises almost everywhere without
the tree getting worse. The comparable aggregate is their sum.

## Total tests per ray

| Scene | A | B | Change |
|---|---|---|---|
| earth | 2.5 | 1.7 | -32% |
| checkered_spheres | 3.0 | 4.8 | +60% |
| perlin_spheres | 3.0 | 4.2 | +40% |
| quads | 7.1 | 7.5 | +6% |
| area_lights | 5.5 | 5.0 | -9% |
| cornell_box | 13.3 | 14.6 | +10% |
| cornell_smoke | 13.9 | 14.4 | +4% |
| mesh_showcase | 17.8 | 12.9 | -28% |
| showcase | 26.1 | 19.4 | -26% |
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

Build time grew by roughly 4x at equal primitive counts, which is what a binned
SAH sweep costs over a midpoint split. At 0.5 ms for the largest scene there it
was not a figure worth optimising.

## A divergence found while measuring

`mesh_showcase` was the one scene whose ray queries moved materially between
the two configurations: 1,155,723 to 1,199,628, or +3.8%. Every other scene was
either bit-identical or differed by less than a tenth of a percent —
`cornell_smoke` because volumes then drew from the sampler inside `hit()`, so a
change in visiting order changed the draw order, and `neon_cathedral` because
it contains exactly coincident quads whose winner depends on visiting order.

Neither explanation covers `mesh_showcase`. Bisecting against the Configuration
A render located the first diverging commit exactly:

- `a197726` (builder extraction) — bit-identical
- `e6b7194` (binned SAH) — bit-identical
- `3802963` (iterative distance-ordered traversal) — **diverges**

The SAH commit being bit-identical is itself evidence: it rewrote the primitive
permutation and leaf grouping wholesale, so a scene sensitive to visiting order
would have moved there.

The difference was confined to the scene's only dielectric and looked like
scattered per-pixel noise rather than a displaced edge: paths diverged,
geometry did not move. With deep recursion and total internal reflection, a
single hit resolved differently reroutes an entire bounce chain, which accounts
for the extra ray queries.

The same commit also replaced per-node division with a per-ray reciprocal, and
`x * (1/y)` is not bit-identical to `x / y`. Reverting that arithmetic while
keeping the new traversal reproduced the divergence to eight digits, so the
reciprocal was not the cause. What remains is the culling and ordering change.

The current traversal is verified against brute-force intersection over the
same assets, including rays cast from inside the mesh. No such verification
exists for the earlier one, so the honest statement is that the two disagree
and only the current one has been checked — not that the earlier one was wrong.
