# Benchmarks

The performance baseline the renderer is measured against, and the rules a
comparison against it has to follow. Every later measurement is only meaningful
next to these numbers, so this file also states what invalidates one.

This is not a comparison against other renderers. It measures this renderer
against itself, over a fixed workload, on one named machine.

The figures below were taken on 2026-09-19 from revision `40e55a0e61ca`, with
`Float = float`, from the `release` and `release-stats` presets, single
threaded. They replace an earlier set taken while `Float` was `double`; what
moved between the two, and why, is in
[What changed since the previous measurement](#what-changed-since-the-previous-measurement).

## Machine

| | Reference |
|---|---|
| CPU | Intel Core i7-11700K @ 3.60 GHz (Rocket Lake) |
| Architecture | x86_64 |
| Logical cores | 16 |
| OS | Ubuntu 24.04 (WSL2) |
| Compiler | Clang 18.1.3 |
| Scalar type | `float` |

This set was measured from a bare login shell on an otherwise idle machine,
with the editor closed. That is a condition of the measurement, not a detail:
the machine is a desktop running a general purpose OS, not an isolated
benchmarking host, and anything else running competes for the same cores and
the same cache.

A second machine on a different instruction set is the only cheap way to tell
an algorithmic improvement apart from one that happens to suit a single
microarchitecture. It is not recorded here; its measurement is deliberately
infrequent and is taken once the current optimisation work is finished.

## Method

The workload is `benchmarks/manifest.txt`: one row per scene, giving the
resolution and sample count. Everything else — maximum depth, seed, background,
tone mapping — comes from the scene file, so a record describes exactly the
work the renderer was asked to do.

Each scene is measured twice, from two different builds:

- The `release` build supplies timing. Five runs; the **minimum** is reported,
  because a slow run means interference and never a faster renderer. Every run
  is kept in the raw record. Five rather than the usual three because this set
  is the figure every later measurement is subtracted from; routine
  measurements keep the default.
- The `release-stats` build supplies the traversal counters. One run: the
  counters are deterministic under a fixed seed, so repeating them costs time
  and adds nothing.

They are separate builds because the counters sit in the traversal hot loop. A
build carrying them cannot also be timed honestly. Measured here, the
instrumented build was 0.6% slower than the plain one at the median and 4.5%
slower at the worst, on `argent_weave`. Read that as an upper bound rather than
a cost: it compares one instrumented run against the minimum of five plain
ones, so part of it is the spread of a single sample.

The spread across the five timed runs is in the timing table. It reaches 5.6%
on `checkered_spheres` and stays under 2% on eight of the thirteen scenes. The
two percent figure quoted as this machine's noise floor — and used as the
default threshold by `scene-tool bench-compare` — is therefore optimistic for
the short scenes, and a one-scene movement near that size is not a result.

### Threads

The renderer is single threaded today, and every figure below is one core.
Measurements are taken at both one thread and the full thread count; while
those are the same configuration, the record carries `threads: 1` and one
number is reported. When the renderer becomes parallel this whole set is
remeasured, because a figure taken on more threads is not a faster renderer.

### Throughput and memory

Throughput is reported as primary rays per second: one ray per sample, so
`width × height × samples_per_pixel` divided by the fastest run. It counts no
bounces. A ray query in the traversal counters is a different quantity —
primary rays plus every bounce — and the two are not interchangeable. They are
also never both trustworthy in one record: the counters come from the
instrumented build, whose timing does not count.

The sample count in that product is the one the renderer used, floored to a
perfect square, not the one the manifest asked for. Every row of the manifest
is already square, so the two agree today.

Peak memory is the process-wide high-water mark, and it is not measured over
the same work as the timing. The timed runs deliberately exclude scene loading
and BVH construction, which the renderer performs once before any of them; the
memory figure cannot exclude them, because a high-water mark only rises. For
`argent_weave` — a 79 MB OBJ and a 2.3M node tree — that difference is most of
the number. Read it as the footprint the process demanded, not as what the
render loop allocates. Separating the two needs allocation instrumentation this
set does not have.

Where that time goes inside the renderer is a separate measurement, in
[profiling.md](profiling.md).

### The record

Records are written as one JSON object per line to the file named on the
command line. Each object is self-describing — machine, source revision, build
configuration, thread count, scene settings, every timed run, throughput, peak
memory, BVH statistics — so runs taken months apart can be concatenated and
still be told apart. The fields are specified in
[`schema/benchmark.schema.json`](../schema/benchmark.schema.json), which
validates a single line rather than the file.

The raw records behind the tables below are kept at
`benchmarks/baseline.ndjson`, so a later run can be compared against this one
without remeasuring it:

```bash
scene-tool bench-compare benchmarks/baseline.ndjson out/benchmarks.ndjson
```

### Reproduce

```bash
cmake --preset release       && cmake --build --preset release
cmake --preset release-stats && cmake --build --preset release-stats
BENCH_RUNS=5 scripts/run-benchmarks.sh out/benchmarks.ndjson
```

A single scene, without the script:

```bash
./build/release/pathtracer scenes/cornell_box.json \
    --width 400 --height 400 --spp 49 --bench --bench-runs 5
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
- A different scalar type. `Float = double` changes both speed and results.
- A different thread count.
- A change to the intersection arithmetic itself. It moves the counters, not
  just the timings, and the tables below record exactly such a change.
- A different source revision, or a build taken with uncommitted changes. The
  record names the commit but cannot see a dirty working tree, so this one is
  discipline rather than a check.
- A different machine, or the same machine in a different thermal, power or
  load state.
- The machine is the one exemption a counter-only comparison makes; see
  [The counter regression gate](#the-counter-regression-gate).

Statistics and timing may be quoted from the same run only when both come from
the same record.

### Definitions

- **Revision** — the commit the binary was built from, captured when CMake last
  configured. It says nothing about uncommitted edits.
- **Threads** — worker threads the render used, which is not the machine's core
  count. One today.
- **Spread** — the slowest of the timed runs against the fastest, as a
  percentage. It stays in the table so the reported minimum is never read as a
  measurement without variance.
- **Primary rays** — one per sample: `width × height × samples_per_pixel`.
- **Mray/s** — millions of primary rays divided by the reported render time,
  derived from the same run so the two cannot disagree.
- **Peak RSS** — the process's high-water mark, scene loading and tree
  construction included.
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
- **Queries/primary** — ray queries divided by primary rays: the average number
  of hit tests a single sample sets off, and therefore a direct measure of path
  length. A scene whose paths terminate on the first bounce sits near one.
- **Depth** — the longest root-to-node path; a single-node tree has depth 0.

## The counter regression gate

The traversal counters are checked on every push to `main`, against a recorded
set kept at `benchmarks/counter-baseline.ndjson`. Timings are not checked there
and are not worth checking there: a shared runner's wall clock measures its
neighbours as much as it measures this renderer, while the counters measure the
work itself.

The workload is `benchmarks/manifest.txt` unchanged — the same rows the tables
below were measured from. Only the instrumented pass runs, one run per scene,
which is the whole of the measurement: the counters are deterministic under a
fixed seed, so a second run would cost time and say nothing.

### Why the threshold is zero

The counters were measured to reproduce exactly on the runner: thirteen scenes,
every node test, leaf test and ray query identical to the figures taken on the
reference machine at the same revision. That is expected rather than lucky. The
baseline ISA carries no FMA, so the compiler cannot fold a multiply and an add
into a single rounding; nothing in the build relaxes floating point; and both
hosts run the same Clang and the same libm. It is a measurement nonetheless,
and the threshold rests on the measurement rather than on the argument.

The consequence is that the CPU model is left out of the comparison, and only
there. Every other rule above still holds: architecture, scalar type, thread
count, resolution, sample count, seed and maximum depth all have to match, and
a record carrying a timing is refused outright, because two machines cannot be
timed against each other however well their counters agree. A run taken on a
different thread count will refuse the comparison rather than quietly report
the difference as a result.

### What it catches, and what it does not

Two figures carry a verdict: hit tests per ray query, and ray queries. Between
them they pin the absolute work — a test count that moves while the ratio holds
has to have moved the denominator with it, and that is the second figure. A
different tree, a different traversal order, a path that terminates somewhere
else: all three move one of the two.

It says nothing about speed. A change that halves the cost of a box test moves
no counter at all, and neither does one that doubles it — the widening of the
slab test recorded below is exactly that case. Tree shape is recorded in the
same file but not compared, so a build that reshapes the tree without changing
what traversal touches passes unremarked. Both gaps are deliberate: this gate
exists because timings cannot be trusted on a runner, not because counters are
the whole of performance. The timings are taken here, by hand, on the machine
named above.

### Refreshing the baseline

A falling counter is reported as a gain and does not fail the run, so an
improvement leaves the recorded set describing work the renderer no longer
does. Refreshing it is deliberate, and the commit that refreshes it is where
the change is accounted for:

```bash
cmake --preset release-stats && cmake --build --preset release-stats
scripts/run-benchmarks.sh --stats-only benchmarks/counter-baseline.ndjson
```

Regenerate from a clean tree at the commit being recorded. NDJSON carries no
comments, so the file cannot say which revision produced it beyond the
`revision` field in each record, and that field is blind to uncommitted edits.

A refresh is warranted when the counters moved because the work genuinely
changed, and never to make a red job green. The two are told apart by the same
question the rest of this document asks: is the new number explained.

## Baseline

### Timing and throughput

From the `release` build. Five runs per scene, minimum reported.

| Scene | Res | spp | Render (s) | Spread | Mray/s | Peak RSS (MB) |
|---|---|---|---|---|---|---|
| `area_lights` | 480x270 | 225 | 32.06 | 0.2% | 0.91 | 8.0 |
| `argent_weave` | 480x270 | 9 | 12.14 | 4.2% | 0.10 | 250.9 |
| `checkered_spheres` | 480x270 | 121 | 6.50 | 5.6% | 2.41 | 7.9 |
| `cornell_box` | 400x400 | 49 | 9.37 | 0.8% | 0.84 | 8.7 |
| `cornell_smoke` | 400x400 | 36 | 12.28 | 0.9% | 0.47 | 8.8 |
| `earth` | 480x270 | 484 | 5.48 | 1.6% | 11.44 | 35.1 |
| `gilded_orrery` | 480x270 | 16 | 12.14 | 2.1% | 0.17 | 46.7 |
| `mesh_showcase` | 480x270 | 169 | 6.91 | 4.0% | 3.17 | 8.3 |
| `neon_cathedral` | 480x270 | 16 | 10.30 | 0.7% | 0.20 | 8.0 |
| `perlin_spheres` | 480x270 | 144 | 29.09 | 0.3% | 0.64 | 8.0 |
| `quads` | 400x400 | 225 | 5.47 | 3.3% | 6.58 | 8.8 |
| `random_spheres` | 480x270 | 81 | 15.57 | 1.1% | 0.67 | 9.0 |
| `showcase` | 400x400 | 81 | 11.89 | 1.6% | 1.09 | 36.2 |

The slowest of the five runs was the first one on four of the thirteen scenes
and scattered over the rest, so nothing here is a warm-up effect that reporting
the minimum hides.

### Tree and traversal

Tree shape and build time from the `release` record; the per-ray counters from
the `release-stats` one.

| Scene | Trees | Nodes | Leaves | Depth | Build (ms) | Node tests/ray | Leaf tests/ray | Total | Ray queries | Queries/primary |
|---|---|---|---|---|---|---|---|---|---|---|
| `area_lights` | 1 | 7 | 4 | 3 | 0.003 | 3.3 | 1.0 | 4.3 | 144,689,285 | 4.96 |
| `argent_weave` | 7 | 2,297,221 | 1,148,614 | 26 | 532.104 | 118.7 | 8.0 | 126.7 | 4,855,966 | 4.16 |
| `checkered_spheres` | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.8 | 4.8 | 52,571,828 | 3.35 |
| `cornell_box` | 1 | 15 | 8 | 6 | 0.003 | 13.2 | 1.6 | 14.8 | 43,007,501 | 5.49 |
| `cornell_smoke` | 1 | 11 | 6 | 4 | 0.003 | 9.3 | 1.1 | 10.4 | 33,101,843 | 5.75 |
| `earth` | 1 | 1 | 1 | 0 | 0.001 | 1.0 | 0.7 | 1.7 | 88,192,214 | 1.41 |
| `gilded_orrery` | 15 | 168,397 | 84,206 | 19 | 27.401 | 53.6 | 6.1 | 59.7 | 9,509,310 | 4.59 |
| `mesh_showcase` | 3 | 39 | 21 | 6 | 0.005 | 11.4 | 2.5 | 13.9 | 48,857,664 | 2.23 |
| `neon_cathedral` | 6 | 86 | 46 | 5 | 0.012 | 43.8 | 6.4 | 50.1 | 14,065,679 | 6.78 |
| `perlin_spheres` | 1 | 3 | 2 | 1 | 0.002 | 3.0 | 1.1 | 4.1 | 124,543,231 | 6.67 |
| `quads` | 1 | 9 | 5 | 4 | 0.003 | 7.0 | 0.5 | 7.5 | 65,572,769 | 1.82 |
| `random_spheres` | 1 | 967 | 484 | 12 | 0.146 | 10.5 | 1.2 | 11.7 | 80,513,606 | 7.67 |
| `showcase` | 3 | 2,811 | 1,407 | 13 | 0.525 | 17.8 | 1.3 | 19.1 | 31,824,353 | 2.46 |

`Build (ms)` is one sample per scene. Construction runs once per process, so
the two records of a scene supply two independent samples, and those agree to
within about one percent everywhere except `argent_weave`, where the figure has
been seen to move by tens of percent between runs of the same binary. Treat
that one as an order of magnitude rather than a measurement.

### Reading the tables

`argent_weave` is the traversal workload the rest of the set does not provide:
one tree of 2.3M nodes over a 1.1M triangle mesh, 126.7 hit tests per ray query
against 59.7 for the next heaviest scene, and the fewest ray queries in the
set. Ninety-four percent of those tests are box tests, because the geometry is
interlaced tube strands whose bounds overlap heavily — the case a hierarchy of
axis-aligned boxes handles worst. Its cost per ray query also depends on how
much of the frame the geometry covers: a ray that escapes into the background
costs about two tests, and the same geometry measured 106 tests per query from
a camera sixteen units further back. The camera there is a measured setting
rather than only a composition, and re-framing the shot is a change of
workload.

`gilded_orrery` is the only scene where BVH construction is visible at all: 27
ms across fifteen trees and 168k nodes, three orders of magnitude above every
scene but `argent_weave`.

`cornell_smoke` costs what it costs volumetrically rather than geometrically.
Free-flight sampling runs per segment against an eleven-node tree, so the BVH
is close to irrelevant to its total.

The set no longer sits in one timing range. Sample counts were originally
chosen so that each scene took roughly ten seconds; they now span 5.5 to 32
seconds, because the intersection work per sample has changed since they were
chosen. The rows are kept unchanged anyway: re-tuning them would invalidate
every profile and every record taken against them, and the spread within each
scene is small enough that the short scenes are still measured well above the
noise.

## What changed since the previous measurement

The previous set was taken with `Float = double` and before the intersection
arithmetic was made robust. Two things moved between the two measurements — the
scalar type, and the ray origin offset together with the widened slab test — so
a timing difference cannot be attributed to either one alone. The counters can
be attributed, and were.

**Three scenes now trace paths roughly three times as long.** `area_lights`,
`perlin_spheres` and `random_spheres` moved from 1.59, 2.27 and 2.67 ray
queries per primary ray to 4.96, 6.67 and 7.67. That was isolated by building
the commit before the robustness work with the current scalar type and counting
again on the same scenes: it reproduced the old ratios to three digits — 1.60,
2.29, 2.70 — which leaves the robustness work as the whole of the difference
and the scalar type as none of it. The direction is the useful part. Paths that
used to end early now continue, and the same change made the affected images
brighter, so the earlier numbers were counting rays that should never have been
lost. These three are the whole of the set that scatters off a sphere of radius 1000,
and a surface that large is where a fixed epsilon along `t` stops being an
offset at all. The only larger sphere in the set bounds a participating medium
rather than scattering, and its scene did not move.

Two consequences are easy to misread. Their *ratios* per ray query fell —
`random_spheres` from 26.2 to 10.5 node tests per query — purely because the
denominator grew; its absolute test count rose by about 15%, and the tree did
not improve. And their render times grew with the extra work rather than with
any loss of speed.

**The other ten scenes count what they counted before.** Every one of them is
within 0.6% of its previous ray query total, which is the last-bit drift
expected from changing the scalar type. The exception is `mesh_showcase` at
−3.6%, a scene whose single dielectric is already on record as sensitive to
visiting order and to arithmetic at that precision.

**Among those ten, the timing moved in both directions, ordered by how much
box testing the scene does.** The four scenes with the fewest node tests per
ray — `quads`, `earth`, `checkered_spheres`, `mesh_showcase`, all at or below
11 — render in 68% to 75% of their previous time. The three heaviest —
`neon_cathedral`, `gilded_orrery`, `argent_weave`, all at or above 44 — take
12% to 31% longer, while issuing the same number of tests as before. That is
what a widened slab test costs: not more tests, but a more expensive one, paid
once per box and therefore in proportion to how many boxes a scene touches.
This is an observation rather than a result. The earlier timings were taken on
the same machine in an unrecorded load state, which is exactly the comparison
this document's own rules call invalid; the counters are what carry weight
here, and they say the work did not change in these scenes.

## Earlier measurements

Three earlier tables were removed from this file when the baseline above was
taken: the `double` measurement of this workload, and two instrumented
measurements of the BVH taken while the acceleration structure was being built,
comparing a midpoint split with a pointer tree against a binned SAH with a flat
depth-first array. All three are in the file's history. Three findings from them
are worth keeping:

- The SAH tree changes the total tests per ray query by between −22% and −45%
  above roughly four primitives per scene, and *increases* it below that, where
  the extra child box tests cannot pay for themselves.
- `Nodes` and `Leaves` counted different things in the two configurations and
  must not be subtracted across them; `Depth`, `Ray queries` and `Build` mean
  the same in both, and the comparable traversal figure is the sum of the two
  test columns.
- One scene's ray queries moved by 3.8% between those configurations.
  Bisecting located it in the iterative distance-ordered traversal commit, and
  reverting the per-ray reciprocal while keeping the new traversal reproduced
  the divergence, which leaves the culling and ordering change as the cause.
  Only the current traversal has been verified against brute-force
  intersection over the same assets.
