#!/usr/bin/env bash
#
# Runs the benchmark scene set and appends one NDJSON record per measurement.
#
# Two passes per scene: the release build supplies timing, the release-stats
# build supplies BVH traversal counters. They are separate because the counters
# sit in the hot loop, so a build that carries them cannot also be timed.

set -euo pipefail

# Same root resolution as the other scripts: paths in the manifest are relative
# to the repository, not to the caller's working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

manifest="benchmarks/manifest.txt"

# Records are appended as NDJSON: one self-describing object per line, so a run
# can be concatenated with an older one and still be parsed.
output="${1:-out/benchmarks.ndjson}"

# Timed repeats. The record keeps every run; the minimum is the reported figure,
# since a slow run means interference, never a faster renderer.
runs="${BENCH_RUNS:-3}"

# Both builds are required. Overridable for out-of-tree builds, matching
# render-goldens.sh.
timing_renderer="${PATHTRACER:-build/release/pathtracer}"
stats_renderer="${PATHTRACER_STATS:-build/release-stats/pathtracer}"

for renderer in "${timing_renderer}" "${stats_renderer}"; do
    if [[ ! -x "${renderer}" ]]; then
        echo "error: renderer not found at '${renderer}'" >&2
        echo "hint:  cmake --preset release       && cmake --build --preset release" >&2
        echo "hint:  cmake --preset release-stats && cmake --build --preset release-stats" >&2
        exit 1
    fi
done

mkdir -p "$(dirname -- "${output}")"

# Truncated once here, appended to below: a partial run should not silently
# extend the previous one.
: > "${output}"

count=0
while read -r scene width height spp; do
    # Skips blank lines and comments; the header row starts with '#' too.
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then
        continue
    fi

    name=$(basename "${scene}" .json)
    echo "==> ${name}  ${width}x${height}  ${spp} spp  (${runs} timed runs)"

    # Shared arguments. An array, not a string: every element stays one argument.
    args=(--width "${width}" --height "${height}" --spp "${spp}" --log-level warning)

    # Timing pass. stdout carries the record, stderr carries diagnostics, so the
    # redirection needs no filtering.
    "${timing_renderer}" "${scene}" "${args[@]}" --bench --bench-runs "${runs}" >> "${output}"

    # Counter pass. One run: the counters are deterministic under a fixed seed,
    # so repeating them adds time and no information.
    "${stats_renderer}" "${scene}" "${args[@]}" --bench --bench-runs 1 >> "${output}"

    count=$((count + 1))
done < "${manifest}"

echo "wrote $((count * 2)) records to ${output}"
