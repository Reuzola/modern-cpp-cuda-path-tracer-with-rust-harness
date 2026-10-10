# Usage

Two executables and one tool. `pathtracer` renders a scene file to an image,
`pathtracer_viewer` renders it in a window you can move around in, and
`scene-tool` checks scene files, compares images, and compares benchmark runs.

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
| `-f`, `--format` | `png` | `ppm`, `png` or `exr`. |
| `-w`, `--width`, `-H`, `--height` | from the scene | Must be given together. |
| `-s`, `--spp` | from the scene | Samples per pixel. |
| `-d`, `--max-depth` | from the scene | Maximum bounces along a path. |
| `-S`, `--seed` | from the scene | Base seed for sampling. |
| `-t`, `--threads` | every hardware thread | Threads to render with, the calling one included. `-1` leaves one free. |
| `-l`, `--log-level` | `info` | `info`, `warning`, `error` or `off`. |
| `-b`, `--bench` | off | Measure instead of rendering. See below. |
| `--version`, `--help` | | |

Everything except the output path and the format overrides a value the scene
file already carries, so the scene stays the description and the command line
stays the invocation. The overridden value is used for that run only; nothing
is written back.

Four things are worth knowing before the first surprise:

- **Samples per pixel are rounded down to a perfect square.** Sample positions
  are stratified on an `N × N` grid inside each pixel, so `--spp 50` renders
  49. This applies to the scene file's value too.
- **`--seed` moves the sampling, not the scene.** A texture that carries its own
  `seed` is unaffected, so changing the seed gives a different noise pattern of
  the same image rather than a different image.
- **EXR skips tone mapping.** It is written from the linear film. PNG and PPM
  go through the operator the scene selected.
- **The thread count never changes the image.** Every sample is seeded from its
  pixel and pass, and each pixel is written by one thread per pass, so any
  `--threads` value renders the same image bit for bit. Nor does it change the
  traversal counters of the instrumented `release-stats` build: every thread
  counts on its own and the totals are merged, so any thread count reports the
  same figures.

Diagnostics — the progress line, the thread count, the BVH summary, the render
time — go to standard error. The progress line is drawn only at `info`.

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

Records are self-describing — machine, source revision, build, thread count,
scene settings, timings, throughput, peak memory, BVH statistics — so runs
taken months apart can be concatenated. The fields are specified in
`schema/benchmark.schema.json`; the method and the recorded baseline are in
[benchmarks.md](benchmarks.md).

## The viewer

```bash
./build/release-viewer/pathtracer_viewer scenes/cornell_box.json
```

| Option | Default | |
|---|---|---|
| `<scene>` | — | Path to the scene file. Required. |
| `-w`, `--width`, `-H`, `--height` | from the scene | Must be given together. |
| `-s`, `--spp` | from the scene | Samples per pixel, rounded down to a square as in the renderer. |
| `-d`, `--max-depth` | from the scene | Maximum bounces along a path. |
| `-S`, `--seed` | from the scene | Base seed for sampling. |
| `-t`, `--threads` | every hardware thread but one | Threads that render. The window's own thread is not counted. `-1` gives the default. |
| `-f`, `--max-fps` | the display's refresh rate | Frame rate cap. `0` removes it. |
| `-u`, `--ui-scale` | the platform's content scale | XWayland reports 1.0 regardless of DPI. |
| `-l`, `--log-level` | `info` | `info`, `warning`, `error` or `off`. |
| `-m`, `--measure-images` | off | Scripted measurement run. See below. |
| `--version`, `--help` | | |

The viewer has no short options.

Rendering runs on threads of its own, and the window never waits for it. The
image accumulates one sample pass at a time and keeps refining until it reaches
the target. Any change that invalidates the estimate — moving the camera,
changing the depth or the sample target — restarts it and cancels the pass in
progress. Changing exposure or the tone map operator does not: those are applied
to the image that is already there.

While the camera moves, the image is replaced each time the first pass of the
newest view completes. Only one view is in flight at a time, so continuous
movement shows a new image at the rate passes complete instead of freezing
until it stops. On a heavy scene that is a few images a second, while the
window, the overlay and the panel keep their full frame rate.

The default leaves one hardware thread to the window and the GL driver. On the
reference machine that halves the frame-time spikes at no measurable cost in
render speed; see [benchmarks.md](benchmarks.md#default-thread-count).

The frame rate is capped because vsync is not honoured everywhere, and a loop
without it draws frames no display can show, taking that time from the render.
The cap follows the refresh rate the platform reports. WSLg reports 60 Hz
whatever the monitor, so a faster display needs `--max-fps` set to its own
rate.

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

The overlay reports accumulated and target samples, the window's frame rate,
the wall time spent accumulating the current image, and the camera position.
That position is the one to copy back into a scene file after finding a shot
worth keeping.

Screenshots land in `out/screenshots/` with a timestamped name. The PNG is
what the window shows, tone mapped with the current settings; the EXR is the
linear image, unaffected by them. Once the image has converged from the scene's
own camera — `R` returns to it — the EXR is bit for bit the one `pathtracer`
writes for the same sample count and seed, on any thread count of either.

### Measuring the viewer

`--measure-images N` replaces the mouse and keyboard with a scripted camera:
it turns the view one step per image for N images, holds it still for up to N
more, prints one JSON object to standard output and exits. Leave the window
alone while it runs. `scripts/measure-viewer.sh` runs it over a fixed scene set;
what the record holds and how to read it is in
[benchmarks.md](benchmarks.md#the-interactive-viewer).

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

Both images are averaged into square blocks before the error is measured, and
it is that block RMSE the verdict uses. --block-size is the side of the
block and defaults to 8; --block-size 1 measures per pixel. --threshold
is the largest block RMSE that still counts as a pass and defaults to 0.0,
so the tool on its own demands an exact match — the tolerance each reference
is actually held to sits beside it in tests/golden/manifest.txt, and
scripts/check-goldens.sh is what applies it. The full-resolution RMSE, the
largest single-channel difference and PSNR are reported alongside but decide
nothing. A difference image is written only on failure, at full resolution,
amplified by --diff-gain (default 10) so that a one-level difference is
visible. The two images must be the same format: PNG is gamma-encoded and EXR
is linear, and comparing across the two would be comparing two different
quantities.

Comparing two benchmark runs:

```bash
scene-tool bench-compare baseline.ndjson current.ndjson
```

Both files are NDJSON as written by `--bench`, and both must describe the same
workload: the same scenes, the same resolution, sample count, depth and seed,
on the same machine, from the same scalar type and build type, and — for the
timing pass — on the same thread count. The counter pass is exempt from that
last rule, because its totals do not depend on how many threads produced them.
Anything else makes the two runs incomparable, and the tool refuses the whole
comparison rather than reporting a difference it cannot attribute. The revision
is the one field expected to differ.

Each scene is reported on its own, over six metrics: render time, throughput,
peak memory, BVH build time, hit tests per ray query, and ray queries. The
first four are read from the timing pass only — the instrumented build carries
the counters in its hot loop and its timing does not count — and the last two
from the counter pass. A metric that neither run measured is printed as `-`
rather than as zero.

`--threshold` is the relative change below which a difference counts as noise,
and defaults to `0.02`. It exists because the machines are not isolated
benchmarking hosts: the observed noise floor is about two percent, and a
report that called every one percent movement a result would be unreadable.
The figure the report was produced with is printed under it. BVH build times
under a millisecond carry no verdict at all: at that scale the figure is timer
resolution rather than work, and the smallest scenes swing tens of percent
between two runs of the same binary.

All three subcommands use the same exit codes, and so do the scripts built on
them:

| | |
|---|---|
| `0` | Clean — valid, within the threshold, or no regression |
| `1` | The input is wrong — invalid scene, images that differ, or a benchmark regression |
| `2` | The tool failed — file unreadable, malformed image, incomparable runs |

## Scripts

All seven resolve the repository root from their own location, so they can be
run from anywhere. Those that run the renderer accept `PATHTRACER` to point at
one outside the default build directory.

| | |
|---|---|
| `scripts/render-scenes.sh <preset> [dir]` | Renders every scene in `scenes/`. Scenes in the golden manifest use its resolution and sample count; the rest use their own settings. Output goes to `out/<preset>/` unless told otherwise. |
| `scripts/render-goldens.sh [dir]` | Regenerates the reference set. Writes over `tests/golden/` unless given a scratch directory. |
| `scripts/check-goldens.sh [--no-build] [--diff-dir D]` | Builds, renders into a scratch directory and compares every reference against the tolerance its manifest row carries. Keeps the renders and difference images behind only when something failed. |
| `scripts/run-benchmarks.sh [--stats-only] [--threads N] [file]` | Runs the benchmark set twice per scene, once from `release` for timing and once from `release-stats` for counters, writing NDJSON to `out/benchmarks.ndjson`. `--stats-only` drops the timing pass. `--threads` sets both passes and defaults to every hardware thread; the counters come out the same on any count. One file holds one thread count, so a second count goes to a second file. `BENCH_RUNS` overrides the repeat count. |
| `scripts/profile.sh [--out dir] [--threads N] [scene ...]` | Records a sampling profile per benchmark scene and renders a flame graph. One thread unless told otherwise. Output goes to `out/profiles/`. Needs `perf` and `inferno`; see [profiling.md](profiling.md). |
| `scripts/check-determinism.sh [scene ...]` | Renders each scene twice from the same binary and compares the two files byte for byte. Defaults to three scenes chosen for what they construct. |
| `scripts/run-workflows.sh [preset ...]` | Runs every CMake workflow preset (configure, build, test) one after another, or only the named ones, and prints a pass/fail table at the end. A failure does not stop the run; each failed workflow's log is kept in `out/workflows/`. See [building.md](building.md#presets). |

The reference set and the reasoning behind it are in
[golden-images.md](golden-images.md).
