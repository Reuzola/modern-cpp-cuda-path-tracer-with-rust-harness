# Style Guide

Naming and declaration conventions for the C++ engine (`include/`, `src/`, `tests/`) and for the
build files that describe it.

**Scope.** This document covers *names* and *declaration specifiers*. Layout — indentation, brace
placement, line breaking, include ordering — is not described here and is not
open to discussion in review: it is produced mechanically by `.clang-format`,
which is the single source of truth for formatting.

**Enforcement.** Naming is checked mechanically by `readability-identifier-naming`
in `.clang-tidy`, which encodes the table below. Rules that a checker cannot
express — acronym capitalization, whether a name describes the return value or
the implementation — are still applied by hand; where a rule is phrased loosely,
the tighter reading is the intended one.

**Third-party code is exempt.** Everything under `external/` keeps its upstream
naming. Our own code that mirrors a third-party API may follow that API's shape
where doing otherwise would be confusing (see *Exceptions*).

---

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
| Constant (any scope) | `snake_case` | `pi`, `Perlin::point_count` |
| `enum class` | `PascalCase` | `ImageFormat`, `ToneMapOperator` |
| Enumerator | `snake_case` | `ImageFormat::png`, `Key::left_shift` |
| Template parameter | `PascalCase` | `T`, `Ts` |
| Macro | `PT_SCREAMING_SNAKE_CASE` | `PT_FLOAT_AS_DOUBLE` |
| Namespace | `lowercase` | `pt`, `pt::detail` |
| File | `snake_case.hpp` / `.cpp` | `hit_record.hpp` |
| CMake target | `snake_case` + `pt::` alias | `pathtracer_core` / `pt::core` |

---

## Types

### Classes, structs and type aliases use `PascalCase`

```cpp
class BvhNode;
struct HitRecord;
using Float = double;
using Point3 = Vec3;
```

A type alias is a type. `Float` is not a special case that survived a
convention; it is the convention.

**Rationale.** The case of a name should tell you what kind of entity it is
without help from an IDE. `Interval ray_t` needs no tooling to parse;
`interval ray_t` does. The alternative — `snake_case` types, as in the standard
library and Boost — is internally consistent and free, but its motivation is to
make a library read as though it were part of the language. That is not this
project's goal, and the domain agrees: PBRT, Mitsuba and Cycles all use
`PascalCase` types.

### Acronyms capitalize their first letter only

```cpp
class Aabb;        // not AABB
class BvhNode;     // not BVHNode
class Onb;         // not ONB
class CosinePdf;   // not CosinePDF
```

**Rationale.** All-caps acronyms destroy word boundaries, and the damage
compounds: `SAHBVHBuilder` has no readable segmentation, while
`SahBvhBuilder` has three. This is a deliberate departure from the graphics
convention (PBRT writes `BVHAggregate`), taken because the rule is then
exception-free and answers every future acronym in advance.

### Nested types follow the same rule

```cpp
struct UvCoords { Float u{}, v{}; };
```

---

## Functions

### Functions and methods use `snake_case`

```cpp
[[nodiscard]] Aabb bounding_box() const;
[[nodiscard]] Vec3 unit_vector(const Vec3& v);
```

**Rationale.** Consistency with the standard library at the call site, and a
clean contrast against `PascalCase` types: in `Interval::universe.contains(x)`
the type, the object and the operation are visually distinct.

### Names state what the function returns or does, not how it is implemented

`random_scalar()`, not `random_double()`: the return type is `Float`, which is
`double` or `float` depending on the build, so the concrete type name would be
wrong in one of the two configurations. `random_int()` keeps its name because
`int` is a concrete type, not an abstraction over one.

---

## Declaration specifiers

Value types — `Vec3`, `Color`, `Interval`, `Aabb`, `Ray`, `Onb`, `Mat3`, `Affine`,
`Transform`, `Tile` — carry all three specifiers on every member and every free function
operating on them, wherever the language allows:

```cpp
[[nodiscard]] static constexpr Transform translation(const Vec3& offset) noexcept;
```

`constexpr` here is less about compile-time evaluation than about the contract it makes
the compiler enforce: no global state, no allocation, no side effects. That is what a
value type promises, and a later edit that breaks it becomes a build error.

- **`constexpr` implies `inline`.** Never write both.
- **No `[[nodiscard]]` on compound assignment.** `operator+=` returns `*this` so
  assignments chain; marking it would warn on ordinary use.
- **Nothing calling `<cmath>` is `constexpr`.** `sqrt`, `sin`, `cos`, `fabs`, `fmin` and
  `fmax` are not constant expressions before C++26. Such functions still take
  `[[nodiscard]]` and `noexcept`. A compile-time replacement selected with
  `std::is_constant_evaluated()` is deliberately not used: two implementations of one
  formula diverge in the last ulp, and the golden images depend on the runtime one.
- **Guards live with the type.** Each value type header ends with `static_assert`s over
  its constructors, operators and accessors, so a silent loss of `constexpr` fails the
  build. Their values are restricted to integers and powers of two, exact in both
  `float` and `double` builds.

---

## Variables

### Locals and parameters use `snake_case`, with no prefix or suffix

```cpp
const Vec3 outward_normal = (rec.p - center) / radius_;
```

### Public data members use `snake_case` with no suffix

```cpp
class Interval {
public:
    Float min{+infinity};
    Float max{-infinity};
};
```

**Rationale.** A public data member is part of the interface and is read like
one. Decorating it would suggest an implementation detail that it is not.

### Private and protected data members carry a trailing underscore

```cpp
class Sphere final : public Hittable {
private:
    Ray center_;
    Float radius_;
    const Material* mat_ = nullptr;
    Aabb bbox_;
};
```

**Rationale.** Two concrete returns, beyond the visual one:

1. It marks object state at the point of use. `bbox_ = Aabb(box0, box1);`
   carries information that `bbox = ...` does not.
2. It removes the `Type x` / member `x` collision from constructors, so
   `mat_(mat)` needs no shadowing. This is what makes `-Wshadow-all`
   available to the project rather than a source of dozens of unfixable
   diagnostics.

A trailing underscore is used rather than a leading one: identifiers beginning
with an underscore are reserved to the implementation in several contexts, and
the rule "never start an identifier with an underscore" is easier to remember
than the exact list of contexts.

---

## Constants

All constants use `snake_case`, at every scope:

```cpp
inline constexpr Float pi = std::numbers::pi_v<Float>;   // namespace scope
static constexpr int point_count{256};                   // class scope
constexpr Float delta = 0.0001_f;                        // block scope
```

**Rationale.** `constexpr` is a property of the *definition*, not of the name,
and the compiler already enforces it. A `k` prefix (`kPointCount`) or
`SCREAMING_CASE` would encode into the name something the reader can neither
rely on nor verify — `const` and `constexpr` are not the same guarantee, and the
name cannot distinguish them. Reserving `SCREAMING_CASE` exclusively for macros
also keeps one visual signal meaning exactly one thing: *this does not obey
scope*.

Constants that mirror mathematics keep the mathematical name: `pi`, not
`kPi`, not `PI`.

---

## Enumerations

Scoped enumerations only (`enum class`). The type is `PascalCase`; its
enumerators are `snake_case`:

```cpp
enum class ToneMapOperator { none, reinhard, aces };
```

**Rationale.** Most enumerators in this project mirror an external lowercase
token: a tone map operator named in a scene file, a format or log level spelled
on the command line, a key on the keyboard. Keeping the spellings identical
makes the mapping checkable by eye at the point where the two meet. It also
avoids a name collision that `PascalCase` would invite — `None` is a macro in
Xlib, which the viewer links against. Unscoped `enum` is not used: it leaks its
enumerators into the surrounding scope and converts implicitly to `int`.

---

## Templates

Template parameters use `PascalCase`, and are given descriptive names when the
constraint on them is meaningful:

```cpp
template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};
```

**Rationale.** A template parameter names a type, so it follows the rule for
types. Single letters are acceptable when the parameter is genuinely
unconstrained.

---

## Macros

Macros use `PT_` followed by `SCREAMING_SNAKE_CASE`:

```cpp
PT_FLOAT_AS_DOUBLE
```

**Rationale.** Macros ignore namespaces and scope, so their names must be
globally unique — the `PT_` prefix is that uniqueness, and the shouting is the
warning. The same prefix is used for CMake cache variables (`PT_ENABLE_IPO`,
`PT_DOUBLE_PRECISION`), which share the same flat global namespace problem.

New macros are added only where the language offers no alternative:
configuration switches that must be visible to the preprocessor. Nothing that
could be a `constexpr` variable, an `inline` function, or a template becomes a
macro.

---

## Namespaces

All engine code lives in `pt`. The namespace name is short and lowercase, and
the hierarchy is deliberately flat — directory structure is not mirrored in
namespaces.

`pt::detail` is reserved for entities that must appear in a header for
technical reasons but are not part of the public interface.

---

## Files

### File names use `snake_case`, headers `.hpp` and sources `.cpp`

```
include/pt/geometry/bvh.hpp     // class BvhNode
src/geometry/bvh.cpp
```

File names are **not** required to match the name of the type they contain, and
deliberately do not follow the type's capitalization.

**Rationale.** Three reasons, in order of weight:

1. A file frequently holds more than one type — `material.hpp` declares
   `Material`, `ScatterRecord`, `SpecularBounce` and `DiffuseBounce` — so
   "file name equals type name" is a rule with exceptions from the first day.
2. Renaming a file only in case (`vec3.hpp` to `Vec3.hpp`) is unreliable in
   Git across case-insensitive filesystems, which the project would otherwise
   never encounter but a contributor on macOS or Windows would.
3. It is the domain's practice: PBRT, Embree, Cycles and Mitsuba all pair
   `PascalCase` types with lowercase file names.

### Headers use `#pragma once`

No include guard macros, and therefore no naming convention for them.

### Includes are quoted and spelled as a full path from the include root

```cpp
#include "pt/math/vec3.hpp"     // project headers
#include <algorithm>            // standard and external headers
```

---

## Build files

CMake targets use `snake_case` and are exposed to consumers through a
namespaced alias:

```cmake
add_library(pathtracer_core STATIC ...)
add_library(pt::core ALIAS pathtracer_core)
```

**Rationale.** The alias is what other targets link against, and the `::`
makes a missing dependency a configure-time error rather than CMake silently
treating an unknown name as a raw library flag to be resolved at link time.

---

## Tests

A `TEST_CASE` name is a sentence describing the property under test, not the
name of the function being tested. Tags are lowercase, bracketed, and name a
suite:

```cpp
TEST_CASE("engine headers compile and link into the test binary", "[smoke]") { ... }
```

**Rationale.** Catch2 prints the name on failure, and the tag becomes a CTest
label. Both are read by someone who is not looking at the test's source.

---

## Exceptions

These are the only sanctioned departures from the rules above.

### Single-letter mathematical notation

Permitted for **locals and parameters** when the surrounding code is a direct
transcription of a formula, and the letter is the one the formula uses:

```cpp
const Float a = r.direction().length_squared();
const Float h = dot(oc, r.direction());
const Float discriminant = h * h - a * c;
```

For **data members** the letter is kept only when it is the domain's standard name for
the quantity — `u_` and `v_` for a quad's edge vectors — and always in lowercase. A
capital reads as a type, and the member outlives the formula it was transcribed from:

```cpp
class Quad final : public Hittable {
private:
    Point3 q_;      // the corner point, not `Q`
    Float d_;       // the plane constant, not `D`
};
```

### Mirroring a third-party API

A type whose entire purpose is to adapt a third-party interface may take its
name from that interface: `StbiDeleter` is named after `stbi_image_free`, and
naming it `ImageDataDeleter` would hide which library it belongs to.

### Literal operators

User-defined literal suffixes are lowercase and short by necessity: `1.0_f`
constructs a `Float`. The leading underscore is mandated by the language.

### Members forced by the language

A data member cannot share its name with a member function of the same class.
Where an accessor takes the natural name, the member takes the underscore —
which is what the private-member rule already requires:

```cpp
class Color {
public:
    [[nodiscard]] constexpr Float r() const noexcept { return r_; }
private:
    Float r_{};
};
```
