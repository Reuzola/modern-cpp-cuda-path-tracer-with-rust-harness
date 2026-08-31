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
with the resolution and sample count to render it at. Everything else — max
depth, seed, tone mapping, background — comes from the scene file itself, so
those authored settings are covered by the reference too.

To render somewhere else without touching the tracked set, pass a directory:

```bash
scripts/render-goldens.sh /tmp/actual
```

`PATHTRACER=<path>` overrides the renderer location for out-of-tree builds.

## Comparing

The whole loop — build, render into a scratch directory, compare every
reference — runs in one command:

```bash
scripts/check-goldens.sh
```

It leaves the difference images behind only when something failed, and takes
`--threshold` for the AArch64 case below. The manual invocation underneath is
for looking at a single scene.

```bash
tools/scene-tool/target/release/scene-tool compare \
    tests/golden/cornell_box.png /tmp/actual/cornell_box.png \
    --diff /tmp/cornell_diff.png
```

The tool's exit codes and remaining flags are described in [usage.md](usage.md).

## Use the `release` preset

Reference renders are only valid from `release`. `release-native` adds
`-march=native`, which enables FMA contraction and changes floating-point
results.

The same drift appears on AArch64 with the plain `release` preset — identical
scenes, identical magnitude — because `fmadd` is baseline there, so Clang
contracts without being asked. Reference images are generated on x86-64; on an
ARM machine, compare with `--threshold 5e-4` instead of regenerating them.

## Threshold

The comparison threshold defaults to 0.0, and the whole set reproduces
bit-identically from the same build. Determinism is a property the renderer
was designed for: sampler state is derived from the pixel and sample index,
never from wall-clock time or thread identity.

If a machine ever produces the one-level drift measured above, `5e-4` is the
tolerance to use: it absorbs a full 1% of channels being off by one level
while still sitting more than 100x below a genuine image change.

## Updating the references

A golden that fails is a question, not a verdict. Look at the difference image
first. If the change is intentional, regenerate the set and say so explicitly
in the commit message — a commit that silently rewrites the references removes
the only evidence that anything changed.

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
