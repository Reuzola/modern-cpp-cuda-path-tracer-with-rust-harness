# Testing

The project has two test suites: a Catch2 suite for the C++ engine, run through
CTest, and a `cargo test` suite for the Rust scene tool. They are independent —
neither builds or invokes the other.

## Running the C++ suite

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

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
| `release` | Optimizer and ThinLTO. Catches issues that only appear once the compiler is allowed to transform the code. |

Both scalar precisions are worth exercising, since tolerances and a few
numerical paths depend on the width of `Float`:

```bash
cmake --preset dev -DPT_DOUBLE_PRECISION=OFF
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

## Golden images

Rendered output is checked separately, by comparing renders against a tracked
reference set rather than by assertion. That mechanism, including how to
regenerate the references, is documented in
[golden-images.md](golden-images.md).

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

Every push to `main` runs the C++ suite under `dev`, `release` and
`asan-ubsan`, the Rust suite with Clippy, clang-tidy over the full compilation
database, and the golden image comparison. See
[`.github/workflows/ci.yml`](../.github/workflows/ci.yml).
