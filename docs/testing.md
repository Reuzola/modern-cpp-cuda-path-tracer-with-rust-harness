# Testing

The project has two test suites: a Catch2 suite for the C++ engine, run through
CTest, and a `cargo test` suite for the Rust scene tool. They are independent —
neither builds or invokes the other.

## Running the C++ suite

Prerequisites and preset details are in [building.md](building.md).

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

`cmake --workflow --preset dev` does all three in one step, and
`scripts/run-workflows.sh` does it for every preset in turn (see
[building.md](building.md#presets)).

Cases are registered individually via `catch_discover_tests`, so a crash isolates
to one case rather than taking down the run. Catch2 tags are exposed as CTest
labels, and test names are matchable by regex:

```bash
ctest --preset dev -R Vec3        # by name
ctest --preset dev -L '\[bvh\]'   # by tag
```

Discovery happens at build time: the test binary is executed once during the
build to enumerate cases. A failure at that point surfaces as a build error, not
a test error.

## Running the Rust suite

```bash
cd tools/scene-tool
cargo test --locked
```

## What each build configuration checks

The same suite is run under several configurations, each answering a different
question.

| Preset | What it adds |
|---|---|
| `dev` | Debug, warnings as errors. The everyday build. |
| `asan-ubsan` | AddressSanitizer and UndefinedBehaviorSanitizer, with `-fno-sanitize-recover=all` so the first report fails the run. |
| `tsan` | ThreadSanitizer: data races and lock-order inversions, stopping at the first report. It cannot share a build with `asan-ubsan`. Run locally; CI runs `tsan-stats` instead, which covers everything this build does. |
| `tsan-stats` | ThreadSanitizer with the traversal counters compiled in, the only build in which it sees threads counting and their totals being merged. It also runs every line `tsan` does, so it is the one CI runs. |
| `release` | Optimizer and ThinLTO. Catches issues that only appear once the compiler is allowed to transform the code. |
| `release-stats` | The traversal counters compiled in. The only configuration in which the counted branch of the counter tests runs; everywhere else the counters compile out and those cases check that nothing is counted. CI runs it in the counter regression job. |

Both scalar precisions are worth exercising, since tolerances and a few
numerical paths depend on the width of `Float` (see [building.md](building.md)).
`float` is the default; `dev-double` builds and tests the other:

```bash
cmake --preset dev-double
cmake --build --preset dev-double
ctest --preset dev-double
```

## Layout

`tests/` mirrors the engine's directory structure — `tests/geometry/` holds the
tests for `src/geometry/`, and so on. The include root is `tests/` itself, so
shared headers are addressed by path (`support/test_support.hpp`) rather than by
bare filename.

`tests/support/` holds the helpers shared across suites: comparison utilities
with `Float`-dependent tolerances, an RAII temporary directory, log capture and
silencing, a texture that records the coordinates it was sampled at, and helpers
for asserting on distributions.

`tests/smoke/` is not a unit suite. It is a build-configuration canary: it
asserts that the scalar type, the NaN and infinity semantics, and the linkage of
each layer are what the build was configured to produce.

Fixture paths are baked in as compile definitions (`PT_SCENES_DIR`,
`PT_ASSETS_DIR`) rather than resolved from the working directory, so cases stay
correct under `ctest -j` and inside IDEs.

## Thread invariance

A render must not depend on how many threads produced it, or on how the image
was cut into tiles. Every sample is seeded from its pixel and pass alone, each
pixel is written by one tile per pass, and passes run in order, so every pixel
adds the same samples in the same order whatever the schedule. That is what
makes the result exact rather than close: floating-point addition is not
associative, and a pixel that summed its samples in another order would differ
in its last bits.

`tests/render/thread_invariance_test.cpp` holds that claim against the real
pipeline. Each case loads a shipped scene, renders it serially as the
reference, renders it again on another pool size and tile size, and compares
the two images bit for bit and the traversal counters exactly. Bits, not `==`:
`==` treats `+0` and `-0` as equal and a NaN as unequal to itself.

A small render can be finished by the calling thread before any worker wakes,
and a comparison between two serial renders proves nothing. The test therefore
wraps the integrator in one that holds the first thread to start tracing until
a second one has started too, and fails if that never happens.

The scene is loaded inside each render, after its pool exists, so a stage that
is later handed the pool is covered by the same comparison.

Two other checks cover what this one cannot. `scripts/check-determinism.sh`
asserts the same property from outside, on the shipped binary (see
[golden-images.md](golden-images.md#determinism)). The `tsan-stats` CI leg
checks the mechanism: that no two threads touch the same memory without
synchronisation. The questions differ: a race need not change the output on
any given run, and an atomic sum of floats, race-free by construction, still
depends on the order in which the threads arrive.

## Golden images

Rendered output is checked separately, by comparing renders against a tracked
reference set rather than by assertion. That mechanism, including how to
regenerate the references, is documented in
[golden-images.md](golden-images.md).

Reproducibility is checked on its own rather than as a side effect of that
comparison, since the comparison now carries a tolerance;
`scripts/check-determinism.sh` is what asserts it, between runs and between
thread counts.

## Scope

The suite covers the engine library (`include/pt/`, `src/`) and the scene tool.
Two areas are deliberately outside it:

- `src/app/cli.cpp` — argument parsing is delegated to CLI11 and exercised by
  running the binary.
- `src/viewer/` — the interactive frontend needs a window and a GL context.

Line coverage is not measured, and the suite is not written against a coverage
target. Cases were added layer by layer as the code was restructured; several
came from investigating a specific bug, and the assertion that pinned the fix
stayed behind.

## Continuous integration

Every push to `main` runs five independent jobs:

- the C++ suite under `dev`, `dev-double`, `release`, `asan-ubsan` and
  `tsan-stats`, with `release` and `asan-ubsan` also rendering one scene end
  to end;
- the Rust suite, plus `cargo clippy` with warnings denied;
- clang-tidy over the `dev-viewer` compilation database, which is configured
  but not built — the analyser needs the commands and the headers, not an
  artefact. `dev` is not used because the viewer's translation units are absent
  from it and would go unanalysed;
- a regression job that validates every scene file, checks that the renderer
  produces byte-identical images on one thread and on several, and compares a
  full render of the golden set against the references, uploading the
  difference images when it fails;
- a performance job that runs the C++ suite in the instrumented `release-stats`
  build, then measures the BVH traversal counters over the benchmark set on
  every core and compares them against a baseline recorded on one thread. It
  gates on counters rather than on time, because a shared runner's wall clock
  measures its neighbours as much as this renderer; and since the totals do not
  depend on the thread count, a pass also confirms that they do not. The method
  and the policy are in [benchmarks.md](benchmarks.md).

See [`.github/workflows/ci.yml`](../.github/workflows/ci.yml).
