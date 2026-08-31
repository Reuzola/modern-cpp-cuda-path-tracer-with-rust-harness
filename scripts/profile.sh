#!/usr/bin/env bash
#
# Records a sampling profile per benchmark scene and renders a flame graph.
#
# The workload is benchmarks/manifest.txt, unchanged: a profile exists to
# explain the timings in docs/benchmarks.md, so it has to measure the same work.
#
# Output per scene: a raw perf record, a folded stack file, a flame graph, and a
# flat self-time report. The raw record is kept so a profile can be re-examined
# without re-running it.
#
# Usage: scripts/profile.sh [--out <dir>] [scene-name ...]

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

manifest="benchmarks/manifest.txt"
output_dir="out/profiles"

# The profiling build, never the timing one: release carries no debug info and
# omits frame pointers, so its stacks cannot be unwound.
renderer="${PATHTRACER:-build/release-profiling/pathtracer}"

# Software timer event. Hardware cycles are available on this host, but the
# question this profile answers is where wall time goes, not where cycles go.
# 999 rather than 1000: an even multiple would phase-lock with kernel timers and
# resample the same point.
event="cpu-clock"
frequency=999

selected=()

while [[ $# -gt 0 ]]; do
    case "$1" in
    --out)
        [[ $# -ge 2 ]] || { echo "error: --out needs a value" >&2; exit 2; }
        output_dir="$2"
        shift 2
        ;;
    -*)
        echo "error: unknown option '$1'" >&2
        exit 2
        ;;
    *)
        selected+=("$1")
        shift
        ;;
    esac
done

for tool in perf inferno-collapse-perf inferno-flamegraph; do
    if ! command -v "${tool}" >/dev/null 2>&1; then
        echo "error: '${tool}' not found on PATH" >&2
        echo "hint:  see docs/profiling.md for the setup this script expects" >&2
        exit 2
    fi
done

if [[ ! -x "${renderer}" ]]; then
    echo "error: renderer not found at '${renderer}'" >&2
    echo "hint:  cmake --preset release-profiling && cmake --build --preset release-profiling" >&2
    exit 1
fi

mkdir -p "${output_dir}"

count=0
while read -r scene width height spp; do
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then
        continue
    fi

    name=$(basename "${scene}" .json)

    # An empty selection means the whole manifest; otherwise only the named scenes.
    if [[ ${#selected[@]} -gt 0 ]]; then
        wanted=false
        for entry in "${selected[@]}"; do
            [[ "${entry}" == "${name}" ]] && wanted=true
        done
        [[ "${wanted}" == true ]] || continue
    fi

    echo "==> ${name}  ${width}x${height}  ${spp} spp"

    data="${output_dir}/${name}.data"

    # --bench renders without writing an image, so the profile is the render
    # loop and not the image encoder. One run: a second one would double the
    # sample count without changing the distribution.
    # Records go to stdout and are discarded here; this script measures, it does
    # not report timings.
    perf record \
        --event "${event}" \
        --freq "${frequency}" \
        --call-graph fp \
        --output "${data}" \
        -- "${renderer}" "${scene}" \
        --width "${width}" --height "${height}" --spp "${spp}" \
        --bench --bench-runs 1 --log-level error >/dev/null

    # perf script expands each sample into a stack, inferno folds identical
    # stacks into counts, and the flame graph is drawn from those counts.
    # Piped rather than staged through files: the intermediate text is large and
    # of no use once folded. The folded file is kept, since it is small and is
    # what a later comparison would diff.
    perf script --input "${data}" \
        | inferno-collapse-perf \
        > "${output_dir}/${name}.folded"

    inferno-flamegraph \
        --title "${name}" \
        --subtitle "release-profiling · ${width}x${height} · ${spp} spp · ${event} @ ${frequency} Hz" \
        "${output_dir}/${name}.folded" \
        > "${output_dir}/${name}.svg"

    # Reversed: stacks are merged from the leaf up, so a function reached at
    # many recursion depths appears once. PathIntegrator::trace recurses to the
    # scene's maximum depth, which scatters its callees across that many stripes
    # in the forward graph.
    inferno-flamegraph \
        --reverse \
        --title "${name} (reversed)" \
        --subtitle "merged from the leaf up · release-profiling · ${width}x${height} · ${spp} spp" \
        "${output_dir}/${name}.folded" \
        > "${output_dir}/${name}-reversed.svg"

    # Flat self-time ranking. The flame graph shows structure, this shows which
    # single function to open first; the numbers quoted in docs come from here.
    perf report --input "${data}" --stdio --no-children -g none --percent-limit 0.5 \
        > "${output_dir}/${name}.txt"

    count=$((count + 1))
done < "${manifest}"

if [[ ${count} -eq 0 ]]; then
    echo "error: no scene matched" >&2
    exit 1
fi

echo "profiled ${count} scenes into ${output_dir}/"
