# Golden Image Set

`tests/golden/` holds a small set of reference renders. They exist so that
refactors — mesh support, the flat BVH, SAH construction, traversal rewrites —
can be made under a regression test that fails loudly when the image changes.

They are deliberately small and noisy. Their job is to be a cheap
fingerprint, not a gallery.

How to run the scripts below is also summarised in [usage.md](usage.md); this
file is about why the set looks the way it does.

## Regenerating

```bash
cmake --preset release
cmake --build --preset release
scripts/render-goldens.sh
```

The script reads `tests/golden/manifest.txt`, which lists one scene per row
with the resolution, the sample count and the tolerance that scene is held to.
Everything else — max depth, seed, tone mapping, background — comes from the
scene file itself, so those authored settings are covered by the reference too.

To render somewhere else without touching the tracked set, pass a directory:

```bash
scripts/render-goldens.sh /tmp/actual
```

`PATHTRACER=<path>` overrides the renderer location for out-of-tree builds.

## Comparing

The whole loop — build, render into a scratch directory, compare every
reference against its own tolerance — runs in one command:

```bash
scripts/check-goldens.sh
```

It leaves the renders and the difference images behind only when something
failed. There is no threshold to pass on the command line: the tolerances are
manifest columns, and a single global override would silently replace all
thirteen decisions at once. `--diff-dir` sends the difference images somewhere
that outlives the run, which is how CI collects them.

The manual invocation underneath is for looking at a single scene.

```bash
tools/scene-tool/target/release/scene-tool compare \
    tests/golden/cornell_box.png /tmp/actual/cornell_box.png \
    --threshold 0.026 --diff /tmp/cornell_diff.png
```

The tool's exit codes and remaining flags are described in [usage.md](usage.md).

## Use the `release` preset

Reference renders are only valid from `release`. `release-native` adds
`-march=native`, which enables FMA contraction and changes floating-point
results.

The same drift appears on AArch64 with the plain `release` preset — identical
scenes, identical magnitude — because `fmadd` is baseline there, so Clang
contracts without being asked. Reference images are generated on x86-64.

That drift no longer needs a special case. Measured the way the comparison
measures, `-march=native` moves every scene by at most 0.0038, and
`argent_weave` — the one whose paths are most easily rerouted — by 0.020. Both
sit well inside the tolerance those scenes already carry, so an ARM machine
runs the same check as any other. The AArch64 figures themselves have not been
remeasured since the tolerances were set.

## Tolerance

The verdict is not measured per pixel. Both images are first averaged into
8×8 blocks, and it is the RMSE of those blocks that decides. The
full-resolution RMSE and the largest single-channel difference are still
reported, but they no longer gate anything.

The reason is that these references are noisy on purpose. At 16 samples per
pixel, re-rendering a scene with nothing changed but the sampling seed moves
`cornell_smoke` by an RMSE of 0.33 — two unbiased estimates of the same image,
disagreeing at almost every pixel. A tolerance loose enough to accept that is
looser than the distance between the reference and a black frame. At this
sample count a per-pixel tolerance cannot separate noise from a real change,
and the exact match it replaced could not survive a change in arithmetic order.

Monte Carlo noise is zero-mean and independent per pixel, so averaging 64 of
them divides it by eight. A real change — lost energy, a moved edge, a material
reading the wrong texture — moves the block mean instead and survives the
averaging untouched. Over the whole set, against a render with the maximum
depth cut from ten to two, the ratio between the two rises from a median of
1.2 at full resolution to 7.6 at 8×8.

8×8 rather than wider: 16×16 separates them further still, but a 240×135
reference is only 15 by 9 blocks at that size, and a defect confined to one
object would be averaged into the frame around it.

### The per-scene numbers

Every scene carries its own tolerance, because the noise floor belongs to the
scene and not to the renderer: `earth` sits at 0.0012 and `cornell_smoke` at
0.039, a factor of thirty apart.

Each value is 1.5× the error that scene shows when only the sampling seed
changes. That figure is the upper bound on legitimate drift, because reseeding
reroutes every path in the image while a change in arithmetic order — FMA
contraction, a vectorised kernel, a different traversal order — reroutes only
some of them. The 50% margin sits above that bound.

To re-derive them after the workload changes, render the set with a different
seed and read the numbers rather than the verdicts:

```bash
scripts/render-goldens.sh /tmp/reseeded   # with --seed added to the renderer
scene-tool compare tests/golden/<scene>.png /tmp/reseeded/<scene>.png \
    --threshold 1e9
```

### What this cannot catch

A change smaller than the scene's own noise. Truncating the maximum depth from
ten to two — which removes most of the indirect light — stays inside the
tolerance on `area_lights` and `showcase`: the first has almost no indirect
light to lose, the second is the noisiest reference in the set. `earth` is a
third case of a different kind, since it is one sphere under a sky and the
second bounce contributes nothing at all; the truncated render is
byte-identical to the reference.

Raising the sample count would lower those floors, at the cost of a set that is
no longer cheap to regenerate or to render on every push. The trade was made in
favour of keeping it cheap, and the limit is written down here instead.

## Determinism

The tolerance above is for holding a render against a reference produced by a
different build. It is not a licence for one build to disagree with itself.

That is a separate check, `scripts/check-determinism.sh`, which renders a scene
twice from the same binary and compares the two files byte for byte. It covers
what a tolerance cannot: the sampler is seeded from the pixel and sample index
alone, never from wall-clock time or thread identity, and scene loading, arena
allocation and tree construction have to produce the same tree on every run.
Every run is a fresh process, so two constructions at different addresses have
to agree as well.

Three scenes, picked for what they construct rather than for what they show:
`gilded_orrery` for the most trees, meshes, instances and a medium;
`random_spheres` as the only scene that generates its geometry while loading;
`cornell_smoke` for volumes, which draw from the sampler inside the integrator.

## Updating the references

A golden that fails is a question, not a verdict. Look at the difference image
first. If the change is intentional, regenerate the set and say so explicitly
in the commit message — a commit that silently rewrites the references removes
the only evidence that anything changed.

A change that is meant to alter the image, rather than to leave it alone, is
the one case where the tolerances themselves have to be reconsidered: they were
derived from the noise of the current renderer, and a change that lowers
variance lowers them too. Remeasure before regenerating, or the set will hold
the new images to the old renderer's noise floor.

## Coverage

Each scene earns its place by being the one where some feature dominates the
image, so a failure points at a short list of suspects.
One of them is also the only expensive reference in the set: `argent_weave`
renders in seconds rather than fractions of one, because the thing it
fingerprints is a tree too large to build quickly. The set is still cheap
enough to regenerate in one sitting, and a scene that is measured but not
compared would be the worse trade.
Two of them are different in kind. `gilded_orrery` and `showcase` combine
features rather than isolate one, because some defects only surface in the
interaction — a mesh reused under a transform inside a deep BVH, or two media
overlapping along a single ray — and no single-feature scene can produce that.

| Scene | Covers |
|---|---|
| `area_lights` | emissive materials against a black background, without importance sampling |
| `argent_weave` | traversal at scale: a 1.1M triangle mesh in one tree 26 deep, interpolated vertex normals across all of it, and a second mesh instanced under many transforms |
| `checkered_spheres` | procedural checker texture, sky background |
| `cornell_box` | dielectrics, boxes, instancing, importance sampling and mixture densities |
| `cornell_smoke` | constant-density volumes and the isotropic phase function |
| `earth` | image textures, texture path resolution, sphere UV mapping |
| `gilded_orrery` | dense triangle meshes reused under transforms, the deepest BVH in the set, and a thin medium over the whole frame |
| `mesh_showcase` | OBJ loading and triangle intersection in isolation: a dielectric mesh, uniform scaling, and the set's deepest ray recursion |
| `neon_cathedral` | metals, nested BVH groups, ACES tone mapping with exposure, defocus blur, a non-default seed |
| `perlin_spheres` | Perlin noise texture |
| `quads` | quad primitives in all six orientations |
| `random_spheres` | motion blur — the only scene in the set that moves geometry |
| `showcase` | the largest primitive count, two overlapping media, and a boundary shared between a visible object and a medium |
