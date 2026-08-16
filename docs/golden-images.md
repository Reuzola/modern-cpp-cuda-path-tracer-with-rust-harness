# Golden Image Set

`tests/golden/` holds a small set of reference renders. They exist so that
refactors — mesh support, the flat BVH, SAH construction, traversal rewrites —
can be made under a regression test that fails loudly when the image changes.

They are deliberately small and noisy. Their job is to be a cheap fingerprint,
not a showcase.

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

```bash
tools/scene-tool/target/release/scene-tool compare \
    tests/golden/cornell_box.png /tmp/actual/cornell_box.png \
    --diff /tmp/cornell_diff.png
```

Exit code 0 means the RMSE is within the threshold, 1 means it is not, and 2
means the tool itself failed. A difference image is written only on failure.

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

| Scene | Covers |
|---|---|
| `area_lights` | emissive materials against a black background, without importance sampling |
| `checkered_spheres` | procedural checker texture, sky background |
| `cornell_box` | dielectrics, boxes, instancing, importance sampling and mixture densities |
| `cornell_smoke` | constant-density volumes and the isotropic phase function |
| `earth` | image textures, texture path resolution, sphere UV mapping |
| `neon_cathedral` | metals, nested BVH groups, ACES tone mapping with exposure, defocus blur, a non-default seed |
| `perlin_spheres` | Perlin noise texture |
| `quads` | quad primitives in all six orientations |
| `random_spheres` | motion blur — the only scene in the set that moves geometry |
