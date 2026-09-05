# Building

The project is developed and tested on Linux with Clang. The CMake files carry
an MSVC warning set, but no Windows build is exercised and none is claimed.

## Prerequisites

| | Version | Notes |
|---|---|---|
| Clang | 18 | `clang++` must be on `PATH`; the presets name it directly. |
| CMake | 3.25 or newer | Required by `CMakePresets.json` version 6. |
| Ninja | any | The generator every preset uses. |
| vcpkg | any recent | The dependency version is pinned by the manifest, not by your clone. |
| rustup | any | Only for `scene-tool`; the toolchain version is pinned in-tree. |

Set `VCPKG_ROOT` before configuring — the presets resolve the toolchain file
through it, and CMake's error when it is unset does not say so:

```bash
export VCPKG_ROOT=/path/to/vcpkg
```

Dependencies are declared in `vcpkg.json` and built on the first configure,
which takes a while. `builtin-baseline` pins the port versions, so the same
commit resolves the same dependency versions on any machine.

## Quick start

```bash
cmake --preset dev
cmake --build --preset dev
ctest --preset dev
```

Or all three at once:

```bash
cmake --workflow --preset dev
```

Each preset builds into `build/<preset-name>/`, so several configurations can
coexist. Installed dependencies are shared across presets in `vcpkg_installed/`
at the repository root rather than duplicated per build directory.

## Presets

| Preset | Build type | For |
|---|---|---|
| `dev` | Debug | Everyday work. Warnings are errors. |
| `dev-viewer` | Debug | `dev` plus the interactive viewer. |
| `dev-double` | Debug | `dev` with `Float` as `double`. The reference configuration for the non-default scalar type. |
| `asan-ubsan` | Debug + `-O1` | AddressSanitizer and UndefinedBehaviorSanitizer. |
| `release` | Release | Optimized and portable. **Reference images and benchmark timings are only valid from this one.** |
| `release-native` | Release | `release` plus `-march=native`. Faster on this machine, and changes floating-point results. |
| `release-viewer` | Release | `release` plus the viewer. |
| `release-stats` | Release | `release` plus BVH traversal counters. For measurement only. |
| `release-profiling` | Release | `release` plus debug info and frame pointers. For profiling only. |

`ctest` presets exist for `dev`, `dev-viewer`, `dev-double`, `asan-ubsan` and `release`.

Three presets are not interchangeable with the others and it matters:

- **`release-native`** enables fused multiply-add contraction. Every image it
  produces differs from `release` in the last few bits, so it must not be used
  to generate or check reference images.
- **`release-stats`** compiles counters into the traversal hot loop, which
  costs a few percent. It measures tree shape, never time.
- **`release-profiling`** carries debug info and keeps frame pointers, so a
  profile describes the same code generation `release` is timed with. It is not
  a timing build: the frame pointer costs a register.

## Options

Every option is a normal CMake cache variable and can be overridden at
configure time:

```bash
cmake --preset dev -DPT_DOUBLE_PRECISION=ON
```

| Option | Default | Effect |
|---|---|---|
| `PT_DOUBLE_PRECISION` | `OFF` | `Float` is `float`. `ON` selects `double`, which changes both speed and pixel values. |
| `PT_WARNINGS_AS_ERRORS` | `ON` | `-Werror`, in Debug builds only. |
| `PT_ENABLE_IPO` | `ON` | ThinLTO, in Release builds only. |
| `PT_ENABLE_STATS` | `OFF` | Compiles the traversal counters in. |
| `PT_BUILD_VIEWER` | `OFF` | Builds the viewer. See below. |

The scalar type is echoed at configure time, because it affects every pixel and
is easy to forget having changed.

An override placed on the command line applies to that configure only. Every
preset pins the scalar type, so the next plain `cmake --preset <name>` returns
the build directory to `float` rather than inheriting whatever the cache last
held.

## The viewer

The viewer is off by default so that a headless machine — CI, a build that only
renders to a file — never has to resolve a windowing stack.

Turning it on means flipping two switches that must agree: the CMake option and
the vcpkg manifest feature that supplies GLFW, glad and Dear ImGui. Use the
presets that set both:

```bash
cmake --preset dev-viewer
cmake --build --preset dev-viewer
```

Setting `PT_BUILD_VIEWER=ON` on another preset fails at configure time with a
message saying so, rather than failing later inside `find_package`.

The viewer's ports need X11 and OpenGL development headers, which are not
installed by default on a minimal system:

```bash
sudo apt-get install -y \
    libgl1-mesa-dev libx11-dev libxcursor-dev libxi-dev \
    libxinerama-dev libxrandr-dev
```

## The scene tool

```bash
cd tools/scene-tool
cargo build --locked --release
```

`rust-toolchain.toml` pins the toolchain, and rustup installs it on the first
invocation. The pin is resolved from the working directory, so build from
inside `tools/scene-tool` rather than passing `--manifest-path` — the latter
silently uses whatever toolchain happens to be default.

`--locked` fails instead of updating `Cargo.lock`, which is committed.

The release build matters here: the comparison loop runs over whole images.

## Static analysis

`.clang-tidy` is applied by any editor reading `compile_commands.json`, which
every preset exports. To run it over the whole project the way CI does:

```bash
cmake --preset dev-viewer
run-clang-tidy-18 -p build/dev-viewer -quiet -warnings-as-errors='*'
```

Use `dev-viewer` rather than `dev`: the viewer's translation units are absent
from the plain `dev` compilation database and would go unanalysed. The
warnings-as-errors flag is passed here rather than set in `.clang-tidy`, so the
gate lives in CI and an editor stays usable.

## Installing

```bash
cmake --install build/release --prefix /usr/local
```

This installs the `pathtracer` executable and the public headers. The engine
library itself is not installed: it is an implementation detail of the
executables, not a library this project offers to others.
