#!/usr/bin/env bash
#
# Runs the benchmark scene set and appends one NDJSON record per measurement.
#
# Two passes per scene: the release build supplies timing, the release-stats
# build supplies BVH traversal counters. They are separate because the counters
# sit in the hot loop, so a build that carries them cannot also be timed.
#
# --stats-only drops the timing pass and requires only the instrumented build.
# The counters are a property of the source and the workload rather than of the
# machine, so that pass is the only one a shared host can produce a meaningful
# record from.
#
# Usage: scripts/run-benchmarks.sh [--stats-only] [output]

set -euo pipefail

# Same root resolution as the other scripts: paths in the manifest are relative
# to the repository, not to the caller's working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

manifest="benchmarks/manifest.txt"

stats_only=false

# Records are appended as NDJSON: one self-describing object per line, so a run
# can be concatenated with an older one and still be parsed.
output=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --stats-only)
        stats_only=true
        shift
        ;;
    -h | --help)
        sed -n '2,/^$/s/^# \?//p' "${BASH_SOURCE[0]}"
        exit 0
        ;;
    -*)
        echo "error: unknown option '$1'" >&2
        exit 1
        ;;
    *)
        # One positional only. A second one is a typo rather than a second
        # output file, and accepting it would discard the first run.
        if [[ -n "${output}" ]]; then
            echo "error: unexpected argument '$1'" >&2
            exit 1
        fi
        output="$1"
        shift
        ;;
    esac
done

output="${output:-out/benchmarks.ndjson}"

# Timed repeats. The record keeps every run; the minimum is the reported figure,
# since a slow run means interference, never a faster renderer.
runs="${BENCH_RUNS:-3}"

# Overridable for out-of-tree builds, matching render-goldens.sh.
timing_renderer="${PATHTRACER:-build/release/pathtracer}"
stats_renderer="${PATHTRACER_STATS:-build/release-stats/pathtracer}"

# Only the builds this run invokes are required: --stats-only has to work on a
# host that never configured the timing preset.
required=("${timing_renderer}" "${stats_renderer}")
if [[ "${stats_only}" == true ]]; then
    required=("${stats_renderer}")
fi

for renderer in "${required[@]}"; do
    if [[ ! -x "${renderer}" ]]; then
        echo "error: renderer not found at '${renderer}'" >&2
        if [[ "${stats_only}" == false ]]; then
            echo "hint:  cmake --preset release       && cmake --build --preset release" >&2
        fi
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

    # Shared arguments. An array, not a string: every element stays one argument.
    args=(--width "${width}" --height "${height}" --spp "${spp}" --log-level warning)

    # Timing pass. stdout carries the record, stderr carries diagnostics, so the
    # redirection needs no filtering.
    if [[ "${stats_only}" == false ]]; then
        echo "==> ${name}  ${width}x${height}  ${spp} spp  (${runs} timed runs)"
        "${timing_renderer}" "${scene}" "${args[@]}" --bench --bench-runs "${runs}" >> "${output}"
    else
        echo "==> ${name}  ${width}x${height}  ${spp} spp  (counters only)"
    fi

    # Counter pass. One run: the counters are deterministic under a fixed seed,
    # so repeating them adds time and no information.
    "${stats_renderer}" "${scene}" "${args[@]}" --bench --bench-runs 1 >> "${output}"

    count=$((count + 1))
done < "${manifest}"

records_per_scene=2
if [[ "${stats_only}" == true ]]; then
    records_per_scene=1
fi

echo "wrote $((count * records_per_scene)) records to ${output}"
