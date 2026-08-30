# Contributing

Solo project. These are the conventions it follows, not rules it is policed
against.

## Branching

Trunk-based: work lands directly on `main`. No pull requests — a self-approved
PR is a ceremony, not a control. Nothing gates `main`, so verification happens
locally before pushing, and a broken commit is fixed forward rather than by
rewriting published history.

## Commits

[Conventional Commits](https://www.conventionalcommits.org/en/v1.0.0/), with
the usual types: `feat` `fix` `perf` `refactor` `style` `test` `build` `ci`
`docs` `chore`.

A scope is optional and names the area touched — `math`, `bvh`, `scene`,
`viewer`, `cli`, `scene-tool`, `cmake` and so on. Imperative mood, lowercase,
no trailing period, subject short enough to read in a log. The subject says
*what*, the body says *why*.

### Refactors that move pixels

`refactor:` and `style:` mean behaviour is unchanged, which needs defining in a
Monte Carlo renderer: floating-point addition is not associative and the
sampler is a single stream, so reordering arithmetic or reordering random draws
both change the image without changing the logic.

Bit-identical output is a plain `refactor:`. Output that is statistically
equivalent but not bit-identical is a `refactor:` with a note in the body
saying so — without it, a failing image comparison cannot be triaged later.

## Hygiene

Each commit should build on its own and pass whatever checks exist at that
point in history; `git bisect` is useless otherwise. Bulk formatting, renames
and file moves belong in their own commit, separate from any behavioural
change.
