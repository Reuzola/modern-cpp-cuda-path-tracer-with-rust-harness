# Path Tracer

A physically-based path tracer written from scratch in modern C++.

[![CI](https://github.com/Reuzola/modern-cpp-cuda-path-tracer-with-rust-harness/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/Reuzola/modern-cpp-cuda-path-tracer-with-rust-harness/actions/workflows/ci.yml)
![C++20](https://img.shields.io/badge/C%2B%2B-20-blue)
![License: MIT](https://img.shields.io/badge/License-MIT-green)

## Status

The CPU renderer works and the light-transport core is complete. The
infrastructure around it is not: scenes are selected by editing a `switch` in
`src/main.cpp`, and images are written as PPM to standard output.

Restructuring this into a modular, tested engine driven by scene files is in
progress — see the [roadmap](#roadmap).

## Features

**Geometry** — spheres, quads, boxes, BVH acceleration, instancing
(translation and Y-rotation), constant-density volumes.

**Materials** — Lambertian diffuse, metal with roughness, dielectric with
Fresnel reflectance, emissive, isotropic phase function.

**Textures** — solid colour, procedural checker, Perlin noise, image
textures.

**Light transport** — Monte Carlo integration with importance sampling;
cosine, sphere, hittable and mixture probability densities.

**Camera** — configurable position and field of view, defocus blur, motion
blur, stratified sampling, anti-aliasing.

## Requirements

- Clang with C++20 support
- CMake 3.25 or newer
- Ninja
- [vcpkg](https://github.com/microsoft/vcpkg), with `VCPKG_ROOT` pointing at the
  installation — the presets read it to locate the toolchain file

Building the [scene tool](#scene-tool) additionally requires a Rust toolchain.
Install [rustup](https://rustup.rs); it reads the pinned version from
`tools/scene-tool/rust-toolchain.toml` and installs it on first use.

Developed on WSL2 (Ubuntu).

## Building

```bash
cmake --preset dev            # Debug, warnings as errors
cmake --build --preset dev
```

Available presets:

| Preset | Purpose |
|---|---|
| `dev` | Debug build, warnings as errors |
| `asan-ubsan` | AddressSanitizer + UndefinedBehaviorSanitizer |
| `relwithdebinfo` | Optimized with debug info and frame pointers, for profiling |
| `release` | Optimized with ThinLTO, portable across x86-64 machines |
| `release-native` | Release tuned to the host CPU — not portable, not for reference renders |

## Running

Run from the repository root — image textures are resolved by relative path.

```bash
./build/release/pathtracer
```

The scene is chosen by the `switch` value in `main()`. Resolution, samples
per pixel and ray depth are set inside each scene function.

## Scene tool

`tools/scene-tool` is a Rust CLI that validates scene files and compares
renders against reference images. It is a separate cargo workspace with no
build-time coupling to the CMake project:

```bash
cd tools/scene-tool
cargo build
cargo run -- --help
```

Reference renders for regression testing live in `tests/golden/`.
See [docs/golden-images.md](docs/golden-images.md).

## Roadmap

**Engine infrastructure** — modular architecture, unit tests, continuous
integration, JSON scene files, OBJ mesh loading, SAH-built BVH, PNG and EXR
output, interactive viewer, Rust scene validation tool.

**Performance** — multithreading, SIMD, cache-friendly memory layout, BVH
traversal optimisation.

**GPU** — CUDA port, hardware ray tracing via OptiX.

## Acknowledgements

Built by working through the *Ray Tracing in One Weekend* series by Peter
Shirley, Trevor David Black and Steve Hollasch, then extended beyond it.

## License

MIT — see [LICENSE](LICENSE).

Third-party components and assets are listed in
[THIRD_PARTY_NOTICES.md](docs/THIRD_PARTY_NOTICES.md).
