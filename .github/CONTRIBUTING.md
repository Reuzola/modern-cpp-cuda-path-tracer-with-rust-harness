# Contributing

Solo project. These conventions are followed on every commit.

## Branching

Trunk-based: all work lands directly on `main`. No pull requests — a
self-approved PR is a ceremony, not a control. Nothing gates `main`, so
verification happens locally before pushing. Broken commits are fixed
forward, never by rewriting published history.

Phase milestones get annotated tags (`git tag -a v0.2.0 -m "..."`).

## Commits

[Conventional Commits 1.0.0](https://www.conventionalcommits.org/en/v1.0.0/).

Types: `feat` `fix` `perf` `refactor` `style` `test` `build` `ci` `docs`
`chore`.

Scopes are optional but come from a fixed vocabulary: `math` `geom` `bvh`
`material` `texture` `sampling` `camera` `integrator` `film` `renderer`
`scene` `io` `viewer` `cli` `scene-tool` `cmake` `deps`.

`!` marks a breaking change and is reserved for the two public contracts:
the scene file format and the command-line interface. Internal C++ API
changes are not breaking changes.

Imperative mood, lowercase, no trailing period. Subject ≤50 chars preferred,
72 hard. Body wrapped at 72, separated by a blank line. Subject says *what*,
body says *why*.

### Refactors that move pixels

`refactor:` and `style:` mean behaviour is unchanged. In a Monte Carlo
renderer that needs defining: floating-point addition is not associative, and
the RNG is a single stream, so reordering arithmetic or reordering random
draws both change the image without changing the logic.

- Bit-identical output → plain `refactor:`.
- Statistically equivalent but not bit-identical → `refactor:` plus a
  mandatory note in the body saying so.

Without that note, a failing image comparison cannot be triaged later.

## Commit hygiene

Each commit builds on its own and passes whatever checks exist at that point
in history — `git bisect` is useless otherwise.

Mechanical noise never shares a commit with behavioural change. Bulk
formatting, renames and file moves go in their own `style:`/`refactor:`
commit.
