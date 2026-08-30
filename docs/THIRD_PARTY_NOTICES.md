# Third-Party Notices

This project incorporates third-party software. This file lists every such
component, its origin, and its license.

Components fall into two categories:

- **Vendored** — the source is committed to this repository under `external/`
  and is therefore redistributed with it. Attribution obligations apply to
  source distribution.
- **Build-time** — the source is fetched by the package manager at configure
  time and is *not* part of this repository. Attribution obligations apply
  only to binary distributions of this project.

---

## Vendored components

| Component | Version | Author | License | Local modifications |
|---|---|---|---|---|
| [stb_image](https://github.com/nothings/stb) | v2.30 | Sean Barrett | MIT *or* Public Domain (dual-licensed) | None |
| [stb_image_write](https://github.com/nothings/stb) | v1.16 | Sean Barrett | MIT *or* Public Domain (dual-licensed) | None |

`stb_image.h` and `stb_image_write.h` are offered by their author under a
choice of two licenses. This project elects the **MIT** alternative for both,
for consistency with its own license. The full original text of both
alternatives is preserved at the end of each header.

### stb_image and stb_image_write — MIT License

```
Copyright (c) 2017 Sean Barrett

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies
of the Software, and to permit persons to whom the Software is furnished to do
so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## Build-time dependencies

As dependencies are introduced through the package manager, each is recorded here

| Component | Version | Author | License | Role |
|---|---|---|---|---|
| [Catch2](https://github.com/catchorg/Catch2) | 3.15.2 | Martin Hořeňovský, Phil Nash and contributors | BSL-1.0 (Boost Software License 1.0) | Test framework - linked into the test binary only |
| [OpenEXR](https://github.com/AcademySoftwareFoundation/openexr) | 3.4.13 | Contributors to the OpenEXR Project (Academy Software Foundation) | BSD-3-Clause | HDR image output - linked into the engine library |
| [Imath](https://github.com/AcademySoftwareFoundation/Imath) | 3.2.2 | Contributors to the Imath Project (Academy Software Foundation) | BSD-3-Clause | Math types required by OpenEXR - arrives as a transitive dependency |
| [CLI11](https://github.com/CLIUtils/CLI11) | 2.6.2 | Henry Schreiner and contributors | BSD-3-Clause | Command-line argument parsing - header-only, compiled into the driver executable only |
| [nlohmann/json](https://github.com/nlohmann/json) | 3.12.0 | Niels Lohmann and contributors | MIT | JSON scene file parsing - confined to the scene loader translation unit |
| [tinyobjloader](https://github.com/tinyobjloader/tinyobjloader) | 2.0.0rc13 | Syoyo Fujita and contributors | MIT | Wavefront OBJ parsing - linked into the engine library |
| [GLFW](https://github.com/glfw/glfw) | 3.4 | Marcus Geelnard, Camilla Löwy and contributors | Zlib | Window creation, OpenGL context and input - linked into the viewer executable only |
| [glad](https://github.com/Dav1dde/glad) | 0.1.36 | David Herberth | MIT | OpenGL 3.3 function loader - linked into the viewer executable only |
| [Dear ImGui](https://github.com/ocornut/imgui) | 1.92.8 | Omar Cornut and contributors | MIT | Debug UI for runtime controls, with GLFW and OpenGL3 backends - linked into the viewer executable only |

---

### Rust dependencies (`tools/scene-tool`)

Fetched by Cargo at build time and linked into `scene-tool`, which is an
internal tool: it is marked `publish = false` and is not distributed.

| Component | Version | Author | License | Role |
|---|---|---|---|---|
| [clap](https://github.com/clap-rs/clap) | 4 | Kevin K. and contributors | MIT *or* Apache-2.0 | Command-line parsing |
| [exr](https://github.com/johannesvollmer/exrs) | 1.73 | Johannes Vollmer | BSD-3-Clause | Reads the engine's 32-bit float EXR output |
| [jsonschema](https://github.com/Stranger6667/jsonschema) | 0.46 | Dmitry Dygalo | MIT | Offline JSON Schema validation |
| [png](https://github.com/image-rs/image-png) | 0.17 | The image-rs developers | MIT *or* Apache-2.0 | Decodes renders, encodes difference images |
| [serde](https://github.com/serde-rs/serde) | 1.0 | Erick Tryzelaar, David Tolnay | MIT *or* Apache-2.0 | Scene model deserialization |
| [serde_json](https://github.com/serde-rs/json) | 1.0 | Erick Tryzelaar, David Tolnay | MIT *or* Apache-2.0 | JSON backend for serde |
| [thiserror](https://github.com/dtolnay/thiserror) | 2 | David Tolnay | MIT *or* Apache-2.0 | Error type derivation |
| [tempfile](https://github.com/Stebalien/tempfile) | 3 | Steven Allen and contributors | MIT *or* Apache-2.0 | Test fixtures only; not in the shipped binary |

Where a crate offers a choice of licenses, this project elects the **MIT**
alternative, as it does for the vendored headers above.

These are the direct dependencies. Each pulls in a transitive tree of its own;
the complete resolved set, with exact versions, is `tools/scene-tool/Cargo.lock`
and can be listed with `cargo tree` or `cargo license` from that directory.

## Assets

| Asset | Source | Credit | License |
|---|---|---|---|
| `earthmap.jpg` | [Blue Marble: Next Generation](https://svs.gsfc.nasa.gov/12564/) (`world.topo.2004-08`, August 2004), NASA Scientific Visualization Studio | NASA Earth Observatory; Blue Marble data courtesy of Reto Stöckli (NASA/GSFC) | Public domain — U.S. Government work, not subject to copyright ([NASA Media Usage Guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media)) |

Resized from 5400×2700 to 2048×1024 and re-encoded as JPEG. No other
modifications.
