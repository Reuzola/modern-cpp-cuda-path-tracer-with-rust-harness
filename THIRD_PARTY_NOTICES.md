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

`stb_image.h` is offered by its author under a choice of two licenses. This
project elects the **MIT** alternative, for consistency with its own license.
The full original text of both alternatives is preserved at the end of
`external/stb_image.h`.

### stb_image — MIT License

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

This project has no build-time third-party dependencies yet. As dependencies
are introduced through the package manager, each is recorded here.

---

## Assets

| Asset | Source | Credit | License |
|---|---|---|---|
| `earthmap.jpg` | [Blue Marble: Next Generation](https://svs.gsfc.nasa.gov/12564/) (`world.topo.2004-08`, August 2004), NASA Scientific Visualization Studio | NASA Earth Observatory; Blue Marble data courtesy of Reto Stöckli (NASA/GSFC) | Public domain — U.S. Government work, not subject to copyright ([NASA Media Usage Guidelines](https://www.nasa.gov/nasa-brand-center/images-and-media)) |

Resized from 5400×2700 to 2048×1024 and re-encoded as JPEG. No other
modifications.