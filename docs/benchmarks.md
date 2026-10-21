# Benchmarks

The performance baseline the renderer is measured against, and the rules a
comparison against it has to follow. It measures this renderer against itself,
over a fixed workload, on one named machine — not against other renderers.

The current figures were taken on 2026-10-15 from revision `926703c48c2d`,
with `Float = float`, from the `release` and `release-stats` presets, on one
thread and on sixteen, under native Linux. Sections measured under WSL2 are
marked as such; their timings are not comparable with these.

## Machine

| | Reference |
|---|---|
| CPU | Intel Core i7-11700K @ 3.60 GHz (Rocket Lake) |
| Architecture | x86_64 |
| Logical cores | 16 |
| OS | Ubuntu 24.04.5, native |
| Kernel | 7.0.0-38-generic |
| Compiler | Clang 18.1.3 |
| Scalar type | `float` |
| Frequency | `intel_pstate`, `powersave` governor, turbo on, SMT on |
| Power limit | PL1 = PL2 = 150 W, set in firmware |

Measured from a terminal in the desktop session, on an otherwise idle machine,
with the default frequency policy. Anything else running competes for the same
cores and cache, so an idle machine is a condition of the measurement. A second
machine on a different instruction set will be measured once the current
optimisation work is finished.

## Method

The workload is `benchmarks/manifest.txt`: resolution and sample count per
scene. Everything else — maximum depth, seed, background, tone mapping — comes
from the scene file.

Each scene is measured from two builds:

- **`release`** supplies timing: five runs, minimum reported, because a slow
  run means interference and never a faster renderer. Routine measurements use
  the default of three.
- **`release-stats`** supplies the traversal counters: one run, since they are
  deterministic under a fixed seed.

The counters sit in the traversal hot loop, so the instrumented build is never
timed. It measured 0.6% slower at the median and at most 4.5%, on
`argent_weave`.

**One-thread timing is pinned** to logical CPU 2 with `taskset`. Two of the
eight cores turbo to 5.0 GHz and the rest to 4.9, and an unpinned thread may
land on either: unpinned processes differed by up to 4.0%, pinned ones by at
most 1.2%. The default 2% threshold of `scene-tool bench-compare` therefore
holds only for pinned one-thread records. The sixteen-thread pass is not
pinned.

### Threads

- Timing is taken at one thread and at sixteen, into separate files;
  `bench-compare` only compares files taken at the same count.
- Counters do not depend on the thread count: each thread counts on its own
  and the totals are merged after the render.
- Whether the renderer got faster is the one-thread figure across revisions.
  Scaling is the ratio between the two counts within one revision.

The accumulator stays packed row-major despite false sharing between
neighbouring tiles. Under a deliberately worst-case round-robin schedule,
cross-core transfers (`mem_load_l3_hit_retired.xsnp_hitm`) rose more than
twentyfold on `earth` and `quads`, yet neither a cache-line-aligned buffer nor
per-pixel padding made a full-thread render faster; padding cost 1–4%. This
holds at tile 16; below it, see [Tile size](#tile-size).

### Throughput and memory

- **Mray/s** counts primary rays only — `width × height × spp`, with spp
  floored to a perfect square — not bounces. It is not interchangeable with the
  counters' ray queries.
- **Peak RSS** is the process high-water mark and includes scene loading and
  BVH construction, which the timed runs exclude. For `argent_weave` that is
  most of the figure.

Where the time goes inside the renderer is in [profiling.md](profiling.md).

### Reproduce

Each record is one self-describing JSON object per line — machine, revision,
build, threads, scene settings, every timed run, BVH statistics — specified by
[`schema/benchmark.schema.json`](../schema/benchmark.schema.json). The records
behind the tables below are `benchmarks/baseline-1t.ndjson` and
`benchmarks/baseline-16t.ndjson`.

```bash
cmake --preset release       && cmake --build --preset release
cmake --preset release-stats && cmake --build --preset release-stats
BENCH_RUNS=5 taskset -c 2 scripts/run-benchmarks.sh --threads 1 out/benchmarks-1t.ndjson
BENCH_RUNS=5 scripts/run-benchmarks.sh --threads 16 out/benchmarks-16t.ndjson

scene-tool bench-compare benchmarks/baseline-1t.ndjson out/benchmarks-1t.ndjson
```

A single scene, without the script (flags in [usage.md](usage.md)):

```bash
taskset -c 2 ./build/release/pathtracer scenes/cornell_box.json \
    --width 400 --height 400 --spp 49 --threads 1 --bench --bench-runs 5
```

### What invalidates a comparison

Two records are incomparable, and the older one has to be remeasured, if any of
these differ:

- The manifest row: resolution or sample count.
- The scene file: geometry, materials, seed or maximum depth.
- The build preset. `release-native` enables FMA contraction; `release-stats`
  carries the counters.
- The scalar type.
- The thread count — for timing only.
- The intersection arithmetic, which moves the counters, not just the timings.
- How the scene's primitives are grouped into trees — for counters. Records
  taken before primitives were gathered into per-kind sets counted a box's six
  faces as one test, and a medium's boundary or a transformed sphere or quad as
  none.
- The source revision, or a dirty working tree. The record cannot see the
  latter; this one is discipline, not a check.
- The machine, or its thermal, power or load state — except for counters; see
  [The counter regression gate](#the-counter-regression-gate).

Statistics and timing may be quoted together only from the same record.

### Definitions

- **Spread** — the slowest timed run against the fastest, in percent.
- **Trees** — BVHs the scene builds: one per mesh; per container (the world, a
  group), one per primitive kind it holds, plus one over those and its other
  children when there is more than one; one for a box, sphere or quad placed
  alone under a transform or as a medium's boundary. A shared mesh or group is
  built once; a shared sphere or quad is copied into every container that holds
  it.
- **Nodes / Leaves** — size of the flat node array / leaf nodes, each holding a
  contiguous range of primitives.
- **Depth** — the longest root-to-node path; a single-node tree is depth 0.
- **Build** — construction time over all trees, including reading primitive
  bounds and copying each tree’s primitives into leaf order.
- **Node tests/ray** — box tests per ray query; both children are tested before
  descending.
- **Leaf tests/ray** — primitive tests issued by leaves per ray query. Every
  primitive test in every tree is counted, a box’s faces and a medium’s
  boundary included.
- **Total** — the sum of the two, and the comparable aggregate.
- **Ray queries** — top-level `hit()` calls: primary rays plus every bounce.
- **Queries/primary** — ray queries per primary ray: a direct measure of path
  length.

## The counter regression gate

Every push to `main` runs the `release-stats` build over the manifest: the test
suite first, the only place the counted branches of the tests run, then one
counter pass per scene on every core of the runner, compared against
`benchmarks/counter-baseline.ndjson`. Timings are not checked; a shared
runner's clock measures its neighbours as much as the renderer.

**The threshold is zero.** All thirteen scenes reproduced every counter exactly
on the runner against the reference machine at the same revision: the baseline
ISA has no FMA, nothing relaxes floating point, and both hosts run the same
Clang and libm. The CPU model is therefore left out of this comparison, and
only this one. The thread count is left out of every counter comparison; since
the baseline is single-threaded and the runner uses all its cores, each run
also checks that independence. Everything else has to match, and a record
carrying a timing is refused.

Two figures carry the verdict: hit tests per ray query and ray queries, which
together pin the absolute work. The gate is deliberately blind to speed — a
cheaper or dearer box test moves no counter — and to tree shape, which is
recorded but not compared.

### Refreshing the baseline

A falling counter is reported as a gain and passes, so an improvement leaves
the baseline describing work the renderer no longer does. Refresh it from a
clean tree, in the commit that explains the change, and never to make a red job
green:

```bash
cmake --preset release-stats && cmake --build --preset release-stats
scripts/run-benchmarks.sh --stats-only benchmarks/counter-baseline.ndjson
```

## Baseline

### Timing and throughput

From the `release` build, on one thread pinned to logical CPU 2. Five runs per
scene, minimum reported.

| Scene | Res | spp | Render (s) | Spread | Mray/s | Peak RSS (MiB) |
|---|---|---|---|---|---|---|
| `area_lights` | 480x270 | 225 | 6.21 | 0.6% | 4.69 | 8.1 |
| `argent_weave` | 480x270 | 9 | 11.62 | 0.2% | 0.10 | 250.9 |
| `checkered_spheres` | 480x270 | 121 | 6.59 | 0.4% | 2.38 | 8.1 |
| `cornell_box` | 400x400 | 49 | 8.54 | 0.5% | 0.92 | 8.8 |
| `cornell_smoke` | 400x400 | 36 | 10.26 | 0.1% | 0.56 | 8.7 |
| `earth` | 480x270 | 484 | 5.48 | 0.3% | 11.44 | 35.1 |
| `gilded_orrery` | 480x270 | 16 | 11.01 | 0.5% | 0.19 | 46.6 |
| `mesh_showcase` | 480x270 | 169 | 7.14 | 0.4% | 3.07 | 8.3 |
| `neon_cathedral` | 480x270 | 16 | 10.38 | 0.3% | 0.20 | 8.1 |
| `perlin_spheres` | 480x270 | 144 | 7.25 | 0.2% | 2.57 | 8.2 |
| `quads` | 400x400 | 225 | 5.69 | 0.0% | 6.33 | 8.8 |
| `random_spheres` | 480x270 | 81 | 8.44 | 0.1% | 1.24 | 9.1 |
| `showcase` | 400x400 | 81 | 12.46 | 0.2% | 1.04 | 36.2 |

### Tree and traversal

Tree shape and build time from the `release` record; per-ray counters from the
`release-stats` one.

| Scene | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Total | Ray queries | Queries/primary |
|---|---|---|---|---|---|---|---|---|---|---|
| `area_lights` | 1 | 7 | 4 | 3 | 0.002 | 4.1 | 0.9 | 5.0 | 46,370,305 | 1.59 |
| `argent_weave` | 7 | 2,297,221 | 1,148,614 | 26 | 499.962 | 118.7 | 7.9 | 126.6 | 4,856,140 | 4.16 |
| `checkered_spheres` | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.8 | 4.8 | 52,536,071 | 3.35 |
| `cornell_box` | 1 | 15 | 8 | 6 | 0.004 | 13.3 | 1.4 | 14.6 | 43,111,046 | 5.50 |
| `cornell_smoke` | 1 | 11 | 6 | 4 | 0.006 | 9.4 | 0.8 | 10.2 | 33,294,299 | 5.78 |
| `earth` | 1 | 1 | 1 | 0 | 0.001 | 1.0 | 0.7 | 1.7 | 88,192,214 | 1.41 |
| `gilded_orrery` | 17 | 168,497 | 84,257 | 19 | 26.784 | 53.2 | 6.0 | 59.2 | 9,454,034 | 4.56 |
| `mesh_showcase` | 3 | 39 | 21 | 6 | 0.006 | 11.4 | 2.5 | 13.9 | 48,857,291 | 2.23 |
| `neon_cathedral` | 6 | 86 | 46 | 5 | 0.022 | 43.4 | 6.2 | 49.7 | 14,250,319 | 6.87 |
| `perlin_spheres` | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.2 | 4.2 | 42,438,985 | 2.27 |
| `quads` | 1 | 9 | 5 | 4 | 0.003 | 7.0 | 0.5 | 7.5 | 65,572,774 | 1.82 |
| `random_spheres` | 1 | 967 | 484 | 12 | 0.140 | 26.2 | 1.6 | 27.8 | 28,046,486 | 2.67 |
| `showcase` | 3 | 2,811 | 1,407 | 13 | 0.414 | 17.7 | 1.3 | 19.0 | 31,812,234 | 2.45 |

`Build (ms)` is a single sample; below a millisecond it measures the timer. The
two measurable trees, `argent_weave` and `gilded_orrery`, repeated within 0.3%.

### Sixteen-thread timing

From the `release` build, on sixteen threads, unpinned. Five runs per scene,
minimum reported. Scaling is the pinned one-thread time divided by the
sixteen-thread time.

| Scene | 1 thread (s) | 16 threads (s) | Spread | Mray/s | Scaling |
|---|---|---|---|---|---|
| `area_lights` | 6.21 | 0.87 | 2.9% | 33.59 | 7.16 |
| `argent_weave` | 11.62 | 1.25 | 0.7% | 0.94 | 9.33 |
| `checkered_spheres` | 6.59 | 0.89 | 0.4% | 17.65 | 7.42 |
| `cornell_box` | 8.54 | 1.17 | 0.5% | 6.68 | 7.27 |
| `cornell_smoke` | 10.26 | 1.41 | 1.6% | 4.08 | 7.27 |
| `earth` | 5.48 | 0.72 | 1.4% | 86.81 | 7.59 |
| `gilded_orrery` | 11.01 | 1.30 | 0.8% | 1.60 | 8.49 |
| `mesh_showcase` | 7.14 | 1.15 | 0.7% | 19.10 | 6.23 |
| `neon_cathedral` | 10.38 | 1.24 | 1.1% | 1.67 | 8.35 |
| `perlin_spheres` | 7.25 | 1.04 | 2.7% | 17.88 | 6.95 |
| `quads` | 5.69 | 0.82 | 1.5% | 43.91 | 6.94 |
| `random_spheres` | 8.44 | 1.07 | 0.7% | 9.79 | 7.87 |
| `showcase` | 12.46 | 1.56 | 0.9% | 8.31 | 8.00 |

The geometric mean of the scaling is 7.6, against 8.9 under WSL2. One-thread
times barely moved between the two, so the difference is the power limit, not
the operating system: WSL2 ran under the factory limits (125 W sustained,
250 W for 56 s), and its short sixteen-thread runs stayed in the burst budget at
about 4.6 GHz. Here both limits are 150 W, so a run's speed does not depend on
its length or on what ran before it; under full load the cores hold about
3.9 GHz at 143 W and 70 °C (`turbostat`).

### Reading the tables

- `argent_weave` is the traversal workload: a 1.1M triangle mesh, 126.6 hit
  tests per query against 59.2 for the next heaviest scene, 94% of them box
  tests over heavily overlapping tube strands. Its cost depends on how much of
  the frame the geometry covers — 106 tests per query from sixteen units
  further back — so its camera is part of the workload.
- `gilded_orrery` is the only other scene where BVH construction is visible.
- `cornell_smoke` costs what it costs volumetrically; the BVH is close to
  irrelevant to it.
- Sample counts were chosen for about ten seconds per scene and now span 5.5 to
  12.5 s. The rows stay unchanged, because re-tuning them would invalidate
  every earlier profile and record.

## Scaling across threads

Measured under WSL2 on 2026-10-02, revision `3f4daa1d6378`. Its absolute
timings are superseded by the baseline; the findings stand. Comparisons pair
runs from the same round and report the median of three, because sixteen
threads drift by around 12% over minutes. **Utilisation** is CPU time over
threads × wall time; **inflation** is CPU time over the one-thread CPU time.

### How a pass is shared out

The image is cut into 16×16 tiles, handed out one at a time from a shared
counter to whichever thread is free, the calling one included; passes run in
order. A sample is seeded by its pixel and pass, and a pixel is written by one
tile per pass, so the image is bit-identical on any thread count, schedule or
tile size. Golden images and counters confirmed it, and unit tests hold it.

### Where the time goes

- **Dynamic hand-out beat contiguous blocks** by a geometric mean of 1.43 at
  eight threads and 1.31 at sixteen. The blocks were horizontal bands of
  unequal cost; `area_lights` went from 42% to 99% utilisation at eight.
- **At sixteen threads the loss is inflation, not waiting.** Utilisation is
  above 90% everywhere but `earth`; inflation is 1.55–1.96, about 1.07 of it
  clock frequency and the rest two threads sharing each core.
- **The second thread per core helps most where traversal stalls on memory**:
  sixteen over eight threads gains 1.14 on `mesh_showcase` but 1.55–1.56 on
  `gilded_orrery` and `argent_weave`. Any change that removes memory stalls
  should be expected to shrink this gain.
- **The pass boundary costs about 250–300 µs** at sixteen threads, after waking
  only the waiting thread instead of every worker cut context switches per pass
  from 57 to 35. It shows only on short passes: 18% of `earth`, about 10% of
  `quads`, and every pass of the interactive viewer.

### Tile size

Chosen by a rule fixed before measuring: the geometric mean of paired
sixteen-thread time against tile 16; a candidate may be at most 2% slower on one
thread and 5% on any scene; a difference inside the 2% noise floor keeps 16.

| Tile | 1 thread | 16 threads | Worst scene, 16 threads |
|---|---|---|---|
| 4 | 1.013 | 1.046 | 1.328, `earth` |
| 8 | 1.003 | 1.007 | 1.094, `earth` |
| 12 | — | 1.001 | 1.062, `earth` |
| 16 | 1 | 1 | — |
| 24 | — | 1.003 | 1.027, `mesh_showcase` |
| 32 | 1.004 | 1.012 | 1.062, `mesh_showcase` |

Sixteen is the only size that passes. At 32 the pass tail grows. Below 16,
`earth` and `quads`, at about 100 ns per pixel, pay for cache lines shared
between threads — the tile counter and tile edges in the accumulator, not
separated. Anything that makes a pixel cheaper widens that set, so a smaller
tile must not be chosen without remeasuring. The choice has not been re-derived
since the self-intersection fix or the move to native Linux.

## The interactive viewer

Measured with `scripts/measure-viewer.sh` on `quads`, `cornell_box`,
`gilded_orrery` and `argent_weave`, with a fixed camera step per edit. Under
WSLg — software GL (`llvmpipe`), swap interval ignored — unless noted.

- **Rendering is asynchronous.** The synchronous viewer waited on every pass,
  up to 1250 ms per frame on one thread; the asynchronous one keeps the median
  frame at 1.7–6.8 ms. Images arrive a pass later, and passes are 11–23% slower
  because rendering and drawing compete for the same cores.
- **One edit is in flight at a time.** Otherwise each edit cancels the previous
  pass and the image freezes while the camera moves.
- **Fifteen render threads by default.** The sixteenth changes time per pass by
  −6% to +2% but roughly doubles the window's p95 frame time.
- **Spinning before sleeping was rejected**: no scene gained beyond the noise
  floor, and one was slower.

### Frame pacing on a native driver

Native Linux, NVIDIA driver through XWayland, 159.91 Hz display, `quads`,
fifteen threads, one run per configuration, 2026-10-15. The driver honours the
swap interval, and a timer cap on top of vsync made presentation uneven, so
frames are paced by vsync alone and a timer only on request; see
[usage.md](usage.md#the-viewer). Render throughput is unaffected. Drawing a new
image costs 5.7 ms at 400x400 — tone mapping, byte conversion and upload on the
window's thread — most of a 6.25 ms frame. Frame times under vsync include the
wait for the display and are not comparable with the WSLg figures.

## Earlier measurements

Earlier baselines, and the analysis of the move to `float` and robust
intersection, are in this file's history. What is worth keeping:

- The SAH tree cuts total tests per ray query by 22–45% above roughly four
  primitives per scene, and increases it below that.
- Across the pointer tree and the flat array, `Nodes` and `Leaves` count
  different things; `Depth`, `Ray queries` and `Build` are comparable.
- A 3.8% shift in one scene's ray queries was bisected to the culling and
  ordering of the distance-ordered traversal. Only the current traversal is
  verified against brute-force intersection.
- Removing the epsilon along `t` exposed a self-intersection on the radius-1000
  ground sphere that tripled path length in `area_lights`, `perlin_spheres` and
  `random_spheres`. After the fix (`a6ae8fb1a730`) all three are back within
  1.3% of their earlier counters. The same fix cut `gilded_orrery`'s ray
  queries by 4.8%, cause not isolated.
- The widened slab test costs in proportion to box tests: the heaviest scenes
  took 12–31% longer for the same number of tests. That comparison was
  unpaired and is an observation, not a result.
