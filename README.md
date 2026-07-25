# Path Tracer

A physically-based path tracer written from scratch in modern C++.

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

Developed on WSL2 (Ubuntu).

## Building

```bash
cmake --preset dev          # or: release
cmake --build --preset dev
```

## Running

Run from the repository root — image textures are resolved by relative path.

```bash
./build/pathtracer > render.ppm
```

The scene is chosen by the `switch` value in `main()`. Resolution, samples
per pixel and ray depth are set inside each scene function.

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
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
