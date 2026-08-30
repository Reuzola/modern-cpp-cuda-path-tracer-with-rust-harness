# Style Guide

The naming and declaration conventions the C++ code follows, and the reasons
behind the ones that are not obvious.

**Scope.** Names and declaration specifiers. Layout is `.clang-format`'s
department, with one caveat worth knowing: `ColumnLimit` is `0`, so the tool
normalizes indentation, spacing and braces but leaves line breaks alone. Where
a line is broken is the author's decision and stays that way.

**Enforcement.** `readability-identifier-naming` in `.clang-tidy` encodes most
of the table below. Constants are deliberately left out of it: the check
classifies a `const` private data member as a constant and strips its trailing
underscore, which contradicts the rule that matters more. Those, and rules a
checker cannot express — acronym capitalization, whether a name describes the
return value or the implementation — are applied by hand.

**Third-party code is exempt.** Everything under `external/` keeps its upstream
naming.

## Quick reference

| Entity | Convention | Example |
| --- | --- | --- |
| Class, struct | `PascalCase` | `HitRecord`, `ConstantMedium` |
| Type alias | `PascalCase` | `Float`, `Point3`, `PdfVariant` |
| Acronym inside a name | first letter only | `Aabb`, `BvhNode`, `CosinePdf` |
| Free function, method | `snake_case` | `bounding_box()`, `unit_vector()` |
| Local variable, parameter | `snake_case` | `ray_t`, `outward_normal` |
| Public data member | `snake_case` | `Interval::min` |
| Private / protected data member | `snake_case_` | `bbox_`, `mat_` |
| Constant (any scope) | `snake_case`, no suffix | `pi`, `Perlin::point_count` |
| enum class | `PascalCase` | `ImageFormat`, `ToneMapOperator` |
| Enumerator | `snake_case` | `ImageFormat::png`, `Key::left_shift` |
| Template parameter | `PascalCase` | `T`, `Ts`, `Base` |
| Macro | `PT_SCREAMING_SNAKE_CASE` | `PT_FLOAT_AS_DOUBLE` |
| Namespace | `lowercase` | `pt` |
| File | `snake_case.hpp` / `.cpp` | `hit_record.hpp` |
| CMake target | `snake_case` + `pt::` alias | `pathtracer_core` / `pt::core` |

## Types

Classes, structs and type aliases use `PascalCase`. A type alias is a type:
`Float` is not a special case that survived a convention, it is the convention.

```cpp
class BvhNode;
struct HitRecord;
using Float = double;
```

The case of a name should say what kind of entity it is without help from an
IDE: `Interval ray_t` needs no tooling to parse, `interval ray_t` does. The
alternative — `snake_case` types, as in the standard library — is internally
consistent, but its motivation is to make a library read as part of the
language, which is not what this is. The domain agrees: PBRT, Mitsuba and
Cycles all use `PascalCase`.

### Acronyms capitalize their first letter only

```cpp
class Aabb;        // not AABB
class BvhNode;     // not BVHNode
class CosinePdf;   // not CosinePDF
```

All-caps acronyms destroy word boundaries and the damage compounds:
`SAHBVHBuilder` has no readable segmentation, `SahBvhBuilder` has three. This
departs from the graphics convention — PBRT writes `BVHAggregate` — in exchange
for a rule with no exceptions, which answers every future acronym in advance.

## Functions

Functions and methods use `snake_case`, which contrasts cleanly against the
types: in `Interval::universe.contains(x)` the type, the object and the
operation are visually distinct.

A name states what the function returns or does, not how it is implemented.
`random_scalar()`, not `random_double()` — the return type is `Float`, which is
`double` or `float` depending on the build, so the concrete type name would be
wrong in one of the two configurations. `random_int()` keeps its name because
`int` is a concrete type, not an abstraction over one.

## Variables and constants

Locals and parameters take no prefix or suffix. Public data members are part of
the interface and are read like one, so they take none either:

```cpp
class Interval {
public:
    Float min{+infinity};
    Float max{-infinity};
};
```

**Private and protected data members carry a trailing underscore.** Two
concrete returns beyond the visual one: it marks object state at the point of
use — `bbox_ = Aabb(box0, box1);` says something `bbox = ...` does not — and it
removes the `Type x` / member `x` collision from constructors, which is what
makes `-Wshadow-all` usable here rather than a source of unfixable
diagnostics.

A trailing underscore rather than a leading one, because identifiers beginning
with an underscore are reserved to the implementation in several contexts, and
"never start an identifier with an underscore" is easier to remember than the
exact list.

**Constants use `snake_case` at every scope and take no underscore**, including
private static ones — the underscore marks object state, and a constant is not
state:

```cpp
inline constexpr Float pi = std::numbers::pi_v<Float>;   // namespace scope
static constexpr int point_count{256};                   // class scope, private
constexpr Float delta = 0.0001_f;                        // block scope
```

A `k` prefix or `SCREAMING_CASE` would encode into the name something the
reader can neither rely on nor verify: `const` and `constexpr` are not the same
guarantee and the name cannot distinguish them, while the compiler already
enforces both. Reserving `SCREAMING_CASE` for macros also keeps one visual
signal meaning exactly one thing — *this does not obey scope*. Constants that
mirror mathematics keep the mathematical name: `pi`, not `kPi`.

## Enumerations

Scoped enumerations only. The type is `PascalCase`, its enumerators
`snake_case`:

```cpp
enum class ToneMapOperator { none, reinhard, aces };
```

Most enumerators here mirror an external lowercase token: an operator named in
a scene file, a format spelled on the command line, a key on the keyboard.
Identical spellings make the mapping checkable by eye where the two meet. It
also avoids a collision `PascalCase` would invite — `None` is a macro in Xlib,
which the viewer links against.

## Declaration specifiers

Value types — `Vec3`, `Color`, `Interval`, `Aabb`, `Ray`, `Onb`, `Mat3`,
`Affine`, `Transform`, `Tile` — carry `[[nodiscard]]`, `constexpr` and
`noexcept` on their members and on the free functions operating on them,
wherever the language allows:

```cpp
[[nodiscard]] static constexpr Transform translation(const Vec3& offset) noexcept;
```

`constexpr` here is less about compile-time evaluation than about the contract
it makes the compiler enforce: no global state, no allocation, no side effects.
That is what a value type promises, and a later edit that breaks it becomes a
build error.

- **`constexpr` implies `inline`.** Never write both.
- **No `[[nodiscard]]` on compound assignment.** `operator+=` returns `*this`
  so assignments chain; marking it would warn on ordinary use.
- **Nothing calling `<cmath>` is `constexpr`.** `sqrt`, `sin`, `fabs`, `fmin`
  and friends are not constant expressions before C++26. Such functions still
  take `[[nodiscard]]` and `noexcept`. A compile-time replacement selected with
  `std::is_constant_evaluated()` is deliberately not used: two implementations
  of one formula diverge in the last ulp, and the reference images depend on
  the runtime one.
- **Guards live with the type.** Where a type can be constructed at compile
  time, its header ends with `static_assert`s over its constructors and
  operators, so a silent loss of `constexpr` fails the build. Their values stay
  restricted to integers and powers of two, exact in both `float` and `double`
  builds. `Onb` has none: its constructor calls `sqrt`, so no `Onb` can be
  built at compile time today.

## Files

File names use `snake_case`, headers `.hpp` and sources `.cpp`. They are **not**
required to match the name of the type they contain, and deliberately do not
follow its capitalization:

```
include/pt/geometry/bvh.hpp     // class Bvh, struct BvhNode
src/geometry/bvh.cpp
```

Three reasons, in order of weight. A file frequently holds more than one type —
`material.hpp` declares `Material`, `ScatterRecord`, `SpecularBounce` and
`DiffuseBounce` — so "file name equals type name" has exceptions from the first
day. Renaming a file only in case is unreliable in Git across case-insensitive
filesystems, which this project would never encounter but a contributor on
macOS or Windows would. And it is the domain's practice: PBRT, Embree, Cycles
and Mitsuba all pair `PascalCase` types with lowercase file names.

Headers use `#pragma once`. Includes are quoted and spelled as a full path from
the include root:

```cpp
#include "pt/math/vec3.hpp"     // project headers
#include <algorithm>            // standard and external headers
```

## Build files

CMake targets use `snake_case` and are exposed through a namespaced alias:

```cmake
add_library(pathtracer_core STATIC ...)
add_library(pt::core ALIAS pathtracer_core)
```

The alias is what other targets link against, and the `::` makes a missing
dependency a configure-time error rather than CMake silently treating an
unknown name as a raw library flag.

Cache variables share the macro prefix (`PT_ENABLE_IPO`, `PT_DOUBLE_PRECISION`)
because they share the same flat global namespace problem.

## Tests

A `TEST_CASE` name is a sentence describing the property under test, not the
name of the function being tested. Tags are lowercase, bracketed, and name a
suite:

```cpp
TEST_CASE("engine headers compile and link into the test binary", "[smoke]") { ... }
```

Catch2 prints the name on failure and the tag becomes a CTest label; both are
read by someone who is not looking at the source.

## Accepted departures

**Single-letter mathematical notation** is fine for locals and parameters when
the code is a direct transcription of a formula and the letter is the one the
formula uses:

```cpp
const Float a = r.direction().length_squared();
const Float h = dot(oc, r.direction());
```

For data members the letter is kept only when it is the domain's standard name
for the quantity — `u_` and `v_` for a quad's edge vectors — and always in
lowercase, since a capital reads as a type and the member outlives the formula
it came from.

**Mirroring a third-party API.** A type whose whole purpose is to adapt a
third-party interface may take its name from it: `StbiDeleter` is named after
`stbi_image_free`, and calling it `ImageDataDeleter` would hide which library it
belongs to.

**Literal operators** are lowercase and short by necessity — `1.0_f`
constructs a `Float` — and the leading underscore is mandated by the language.

**Members forced by the language.** A data member cannot share its name with a
member function of the same class, so where the accessor takes the natural name
the member takes the underscore, which is what the private-member rule already
requires:

```cpp
class Color {
public:
    [[nodiscard]] constexpr Float r() const noexcept { return r_; }
private:
    Float r_{};
};
```
