# Scene Format

Scenes are JSON documents. The normative definition is
[`schema/scene.schema.json`](../schema/scene.schema.json), written against
JSON Schema draft 2020-12; this document explains it and records the parts a
schema cannot express.

**Both implementations read that one file.** The C++ loader and the Rust
`scene-tool` validator are two consumers of a single source of truth. When they
disagree about what a scene means, the schema decides.

**Editor support.** Adding a `$schema` key to a scene file gives completion and
inline errors in any editor with JSON Schema support:

```json
{ "$schema": "../schema/scene.schema.json", "version": 1 }
```

The key is optional and the loader ignores it.

---

## Document structure

| Key | Required | Description |
| --- | --- | --- |
| `version` | yes | Format version. Currently `1`. |
| `camera` | yes | Ray generation parameters. |
| `render` | yes | Image dimensions, sampling, background, post-processing. |
| `textures` | no | Named texture definitions. |
| `materials` | yes | Named material definitions. |
| `objects` | yes | The scene contents, in order. |
| `importance_targets` | no | Names of objects the integrator samples explicitly. |

Unknown keys are rejected everywhere, at every nesting level. A misspelled
field is an error, not a silently ignored one.

### A complete scene

```json
{
  "$schema": "../schema/scene.schema.json",
  "version": 1,
  "camera": {
    "lookfrom": [0, 2, 12],
    "lookat": [0, 1, 0],
    "vfov": 30
  },
  "render": {
    "width": 400,
    "height": 225,
    "samples_per_pixel": 100,
    "max_depth": 50,
    "background": [0, 0, 0]
  },
  "textures": {
    "grey": { "type": "solid_color", "albedo": [0.5, 0.5, 0.5] },
    "lamp": { "type": "solid_color", "albedo": [8, 8, 8] }
  },
  "materials": {
    "ground": { "type": "lambertian", "texture": "grey" },
    "glass": { "type": "dielectric", "refraction_index": 1.5 },
    "light": { "type": "diffuse_light", "texture": "lamp" }
  },
  "objects": [
    { "type": "sphere", "center": [0, -1000, 0], "radius": 1000, "material": "ground" },
    { "type": "sphere", "center": [0, 1, 0], "radius": 1, "material": "glass" },
    {
      "type": "quad",
      "name": "ceiling_light",
      "q": [-1, 4, -1],
      "u": [2, 0, 0],
      "v": [0, 0, 2],
      "material": "light"
    }
  ],
  "importance_targets": ["ceiling_light"]
}
```

---

## `camera`

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `lookfrom` | vec3 | yes | Eye position. |
| `lookat` | vec3 | yes | Point the camera aims at. |
| `vup` | vec3 | no | Up direction; need not be perpendicular to the view. |
| `vfov` | number | yes | Vertical field of view in degrees, in `(0, 180)`. |
| `defocus_angle` | number | no | Degrees. `0` disables depth of field. |
| `focus_dist` | number | no | Ignored when `defocus_angle` is `0`. |

There is no `aspect_ratio` field. The aspect ratio is `render.width /
render.height` and is never stated twice.

**Rationale.** `vfov` is required because no default is defensible: 20° and 90°
are both ordinary values and a wrong guess produces a plausible-looking wrong
image rather than an error. `vup` is optional because every scene so far uses
`[0, 1, 0]`.

---

## `render`

| Field | Type | Required | Notes |
| --- | --- | --- | --- |
| `width` | integer ≥ 1 | yes | Image width in pixels. |
| `height` | integer ≥ 1 | yes | Image height in pixels. |
| `samples_per_pixel` | integer ≥ 1 | yes | |
| `max_depth` | integer ≥ 1 | yes | Maximum ray bounces. |
| `background` | color | yes | Radiance returned by escaping rays. |
| `seed` | integer ≥ 0 | no | Base seed for deterministic sampling. |
| `tone_map` | object | no | See below. |

`background` is required so that a black background is always a decision and
never an omission — the distinction matters to the "no lights and a black
background" check the validator performs.

Values larger than 2⁵³ are exact in `seed` for both parsers used here, but
tools that hold JSON numbers as doubles will round them silently. Keep seeds
well below that.

### `tone_map`

| Field | Type | Default | Notes |
| --- | --- | --- | --- |
| `exposure` | number > 0 | `1` | Linear multiplier applied before the operator. |
| `operator` | `none`, `reinhard`, `aces` | `none` | |

Omitting the block is equivalent to `{"exposure": 1, "operator": "none"}`, and
that combination is the identity: the image is unchanged. `aces` is the
Narkowicz curve fit, not the full RRT/ODT.

Tone mapping applies to LDR output only; EXR output is written from the linear
film and skips this stage.

### Not part of a scene

Output path, image format, tile size and log level are command-line concerns.
Output describes an invocation, not a scene; tile size is a property of the
machine, not of the image. Keeping them out means there is never a question of
which one wins.

---

## `textures`

An object mapping names to definitions. Names match `^[A-Za-z_][A-Za-z0-9_]*$`.

| `type` | Fields |
| --- | --- |
| `solid_color` | `albedo` (color) |
| `checker` | `scale` (> 0), `even` and `odd` (nested texture objects) |
| `noise` | `scale` (> 0), `seed` (integer ≥ 0, optional) |
| `image` | `filename` (non-empty string) |

`checker` takes **inline** textures for `even` and `odd`, not names. A checker
owns its two children outright, whereas a material only observes a texture that
the scene owns; a name would imply a sharing the engine does not offer. The
cost is that a repeated checker pattern is written out twice.

`noise` is Perlin turbulence at a fixed depth, with no parameter for it. Its
`seed` selects the permutation table and is the only randomness that remains at
scene-construction time — geometry that was once generated randomly is written
out as literal coordinates.

Colors are `[r, g, b]` arrays. Components must be non-negative and may exceed
`1`: emissive materials routinely do.

---

## `materials`

An object mapping names to definitions.

| `type` | Fields |
| --- | --- |
| `lambertian` | `texture` (name) |
| `metal` | `albedo` (color), `fuzz` (0–1, optional) |
| `dielectric` | `refraction_index` (> 0) |
| `diffuse_light` | `texture` (name) |
| `isotropic` | `texture` (name) |

`metal` takes a colour directly rather than a texture, mirroring the engine.
`fuzz` is restricted to `0–1` because the engine clamps to that range, so a
larger value would be a silent no-op rather than a stronger effect.

For `diffuse_light`, `texture` supplies emitted radiance, not reflectance;
values well above `1` are normal.

`refraction_index` may be below `1` — that is how a bubble of a lighter medium
inside a denser one is expressed.

---

## `objects`

An array, not a map. Order is preserved and significant: it is the input to the
BVH build, and a different order produces a different tree.

Every object carries a `type` and may carry a `name`. A name is what
`importance_targets` and other objects refer to, and it is only needed when
something refers to it.

### Leaf primitives

| `type` | Fields |
| --- | --- |
| `sphere` | `center` (vec3), `radius` (> 0), `material` (name), `center_end` (vec3, optional) |
| `quad` | `q` (corner), `u` and `v` (edge vectors), `material` (name) |
| `box` | `a` and `b` (opposite corners), `material` (name) |

A `sphere` with `center_end` moves linearly from `center` to `center_end` over
the shutter interval, producing motion blur. Without it the sphere is static,
which is exactly the case `center_end == center`.

For `box`, the two corners may be given in any order; the engine normalizes
them. They are named `a` and `b` rather than `min` and `max` for that reason.

### Composite nodes

| `type` | Fields |
| --- | --- |
| `group` | `children` (array of objects or names) |
| `translate` | `object`, `offset` (vec3) |
| `rotate_y` | `object`, `angle` (degrees, counter-clockwise) |
| `constant_medium` | `boundary`, `density` (> 0), `phase_function` (material name) |

`group` collects children into one node, which is what makes it possible to
rotate or translate a set of primitives as a unit.

`constant_medium` fills its boundary volume with a participating medium. The
boundary contributes no visible surface of its own; if you also want to see it,
name it, list it among `objects`, and refer to it by name here.

### Inline objects and references

Wherever a child object is expected — `group.children`, `translate.object`,
`rotate_y.object`, `constant_medium.boundary` — you may write either the object
inline or the name of an object defined elsewhere:

```json
{ "type": "rotate_y", "angle": 15, "object": { "type": "box", "a": [0,0,0], "b": [1,1,1], "material": "white" } }
{ "type": "constant_medium", "boundary": "fog_volume", "density": 0.01, "phase_function": "fog" }
```

Inline is the normal case. A name is for when the same object must appear in
two places — most often a boundary that is both a visible surface and the shell
of a medium. Writing it out twice would create two distinct objects.

Top-level entries in `objects` are always definitions; a bare name there is not
accepted.

---

## `importance_targets`

Names of objects the integrator samples directly, in addition to sampling the
BRDF. This is what makes a small light converge instead of staying noisy.

Only spheres and quads can be sampled this way. The name is misleading in one
respect: targets need not be lights. A glass sphere is a useful target because
rays that hit it carry a disproportionate share of the image's variance.

An empty or absent list disables direct sampling entirely, which is correct for
a scene lit only by its background.

---

## Defaults

The schema's `default` annotations are documentation; validators do not insert
them. **This table is what the loader implements.**

| Field | Default |
| --- | --- |
| `camera.vup` | `[0, 1, 0]` |
| `camera.defocus_angle` | `0` |
| `camera.focus_dist` | `10` |
| `render.seed` | `0` |
| `render.tone_map.exposure` | `1` |
| `render.tone_map.operator` | `"none"` |
| `textures` | empty |
| `<texture>.seed` (noise) | `0` |
| `<material>.fuzz` (metal) | `0` |
| `<object>.name` | none |
| `sphere.center_end` | absent — the sphere is static |
| `importance_targets` | empty |

---

## What the schema does not check

Structural validity is not semantic validity. These are the validator's
responsibility, not the schema's:

- a `material` or `texture` name that resolves to nothing;
- an `importance_targets` entry that names a non-existent object, or one that
  is not a sphere or quad;
- duplicate `name` values;
- a reference cycle (an object that reaches itself through a child slot);
- a degenerate quad, whose `u` and `v` are parallel;
- an `image` texture whose `filename` cannot be opened;
- a scene with no emissive material and a black background, which renders black.

Path resolution for `image.filename` is a loader behaviour and is documented
with the loader.

---

## Not supported yet

Triangle meshes and OBJ references; general transforms beyond `translate` and
`rotate_y`, including instancing of a transformed object; sharing a named
texture as a `checker` child.

Adding a primitive or a material type is a backwards-compatible change and does
not bump `version`. `version` changes only when an existing scene stops being
valid.