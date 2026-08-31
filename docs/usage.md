# Usage

Two executables and one tool. `pathtracer` renders a scene file to an image,
`pathtracer_viewer` renders it in a window you can move around in, and
`scene-tool` checks scene files and compares images.

Build instructions are in [building.md](building.md); the scene file itself is
described in [scene-format.md](scene-format.md).

## Rendering

```bash
./build/release/pathtracer scenes/cornell_box.json --output out/cornell.png
```

| Option | Default | |
|---|---|---|
| `<scene>` | — | Path to the scene file. Required. |
| `-o`, `--output` | `out/image.png` | Missing parent directories are created. |
| `--format` | `png` | `ppm`, `png` or `exr`. |
| `--width`, `--height` | from the scene | Must be given together. |
| `--spp` | from the scene | Samples per pixel. |
| `--max-depth` | from the scene | Maximum bounces along a path. |
| `--seed` | from the scene | Base seed for sampling. |
| `--log-level` | `info` | `info`, `warning`, `error` or `off`. |
| `--bench` | off | Measure instead of rendering. See below. |
| `--version`, `--help` | | |

Everything except the output path and the format overrides a value the scene
file already carries, so the scene stays the description and the command line
stays the invocation. The overridden value is used for that run only; nothing
is written back.

Three things are worth knowing before the first surprise:

- **Samples per pixel are rounded down to a perfect square.** Sample positions
  are stratified on an `N × N` grid inside each pixel, so `--spp 50` renders
  49. This applies to the scene file's value too.
- **`--seed` moves the sampling, not the scene.** A texture that carries its own
  `seed` is unaffected, so changing the seed gives a different noise pattern of
  the same image rather than a different image.
- **EXR skips tone mapping.** It is written from the linear film. PNG and PPM
  go through the operator the scene selected.

Diagnostics — the progress line, the BVH summary, the render time — go to
standard error. The progress line is drawn only at `info`.

### Benchmark mode

`--bench` renders repeatedly without writing an image and prints one JSON
object to standard output:

```bash
./build/release/pathtracer scenes/cornell_box.json \
    --width 400 --height 400 --spp 49 --bench --bench-runs 3
```

`--bench-runs` defaults to 3 and only affects timing; the reported figure is
the minimum, since a slow run means interference and never a faster renderer.
`--output` and `--format` are rejected alongside `--bench` rather than ignored.

Records are self-describing — machine, build, scene settings, timings, BVH
statistics — so runs taken months apart can be concatenated. The method and the
recorded baseline are in [benchmarks.md](benchmarks.md).

## The viewer

```bash
./build/release-viewer/pathtracer_viewer scenes/cornell_box.json
```

It accepts the same scene overrides as the renderer, minus the output ones,
plus `--ui-scale` for platforms that misreport their content scale — XWayland
reports 1.0 regardless of DPI.

The image accumulates one sample pass at a time and keeps refining until it
reaches the target. Any change that invalidates the estimate — moving the
camera, changing the depth or the sample target — restarts it. Changing
exposure or the tone map operator does not: those are applied to the film that
is already there.

| | |
|---|---|
| Right mouse button, held | Look around. The cursor hides while held. |
| `W` `S` | Forward, back |
| `A` `D` | Left, right |
| `Q` `E` | Down, up |
| `Left Shift` / `Left Ctrl` | Move faster / slower |
| `R` | Return the camera to the pose the scene file specifies |
| `F1` | Show or hide the control panel |
| `F2` | Save a PNG screenshot |
| `F3` | Save an EXR screenshot |
| `Esc` | Quit |

Movement speed is proportional to the distance between the scene's `lookfrom`
and `lookat`, so a scene measured in hundreds of units does not feel a hundred
times slower than one measured in single digits.

The control panel adjusts exposure, the tone map operator, the maximum depth
and the sample target. It edits `N` rather than the sample count directly,
because rounding a count down to a square is not reversible — typing 17 into a
box showing 16 would leave it at 16.

The overlay reports accumulated and target samples, frame time, the time spent
accumulating, and the camera position. That position is the one to copy back
into a scene file after finding a shot worth keeping.

Screenshots land in `out/screenshots/` with a timestamped name. The PNG is
what the window shows, tone mapped with the current settings; the EXR is the
linear film, unaffected by them.

## The scene tool

Structural and semantic checks over a scene file:

```bash
tools/scene-tool/target/release/scene-tool validate scenes/cornell_box.json
```

It reports errors and warnings separately. An error is something the renderer
would also reject; a warning is something it would accept and probably render
wrongly — a material nothing refers to, a scene with no light and a black
background. Warnings do not affect the exit status.

Comparing a render against a reference:

```bash
scene-tool compare tests/golden/cornell_box.png out/cornell.png \
    --diff out/cornell_diff.png
```

`--threshold` is the largest RMSE that still counts as a pass and defaults to
`0.0`. A difference image is written only on failure, amplified by
`--diff-gain` (default 10) so that a one-level difference is visible. The two
images must be the same format: PNG is gamma-encoded and EXR is linear, and
comparing across the two would be comparing two different quantities.

Both subcommands use the same exit codes, and so do the scripts built on them:

| | |
|---|---|
| `0` | Clean — valid, or within the threshold |
| `1` | The input is wrong — invalid scene, or images that differ |
| `2` | The tool failed — file unreadable, malformed image |

## Scripts

All five resolve the repository root from their own location, so they can be
run from anywhere. All accept `PATHTRACER` to point at a renderer outside the
default build directory.

| | |
|---|---|
| `scripts/render-scenes.sh <preset> [dir]` | Renders every scene in `scenes/`. Scenes in the golden manifest use its resolution and sample count; the rest use their own settings. Output goes to `out/<preset>/` unless told otherwise. |
| `scripts/render-goldens.sh [dir]` | Regenerates the reference set. Writes over `tests/golden/` unless given a scratch directory. |
| `scripts/check-goldens.sh [--threshold V] [--no-build]` | Builds, renders into a scratch directory and compares every reference. Keeps the renders and difference images behind only when something failed. |
| `scripts/run-benchmarks.sh [file]` | Runs the benchmark set twice per scene, once from `release` for timing and once from `release-stats` for counters, appending NDJSON to `out/benchmarks.ndjson`. `BENCH_RUNS` overrides the repeat count. |
| `scripts/profile.sh [--out dir] [scene ...]` | Records a sampling profile per benchmark scene and renders a flame graph. Output goes to `out/profiles/`. Needs `perf` and `inferno`; see [profiling.md](profiling.md). |

The reference set and the reasoning behind it are in
[golden-images.md](golden-images.md).
