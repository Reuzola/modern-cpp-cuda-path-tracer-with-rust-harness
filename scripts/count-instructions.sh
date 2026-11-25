#!/usr/bin/env bash
#
# Counts the user-space instructions a single-threaded render of each benchmark
# scene retires, optionally against an earlier count.
#
# A timing needs an idle, pinned machine and still moves by a few percent from
# one link to the next. An instruction count needs neither: a single-threaded
# render under a fixed seed retires the same instruction stream on every run,
# and the counter sees this process alone. That makes it the yardstick for a
# change that removes work without changing the image - the work removed shows
# up exactly, where a timing would lose it in noise. It is blind to how long
# those instructions take: a division, a cache miss or a stall counts as one or
# as nothing. A profile or a timing run covers that; this does not replace them.
#
# Loading and tree building are subtracted: every scene runs once at its
# manifest workload and once as a single sample of a single pixel, which loads
# and builds the same way and renders next to nothing.
#
# The workload is benchmarks/manifest.txt, unchanged, so a count describes the
# same render a benchmark record and a profile describe.
#
# The release binary is built first, so a count always describes the sources it
# sits next to; --no-build skips that, and so does PATHTRACER naming another binary.
#
# perf is taken from PERF if set, else from PATH if it runs, else the newest
# binary under /usr/lib/linux-tools: Ubuntu's perf on PATH is a wrapper that
# refuses to start when no tools package matches the running kernel.
#
# Output, tab-separated, one line per scene: name, render instructions,
# instructions per sample. With --baseline, the baseline's render instructions
# and the change in percent follow. Lines are printed as they are written.
#
# Usage: scripts/count-instructions.sh [--no-build] [--baseline <file>] [--output <file>] [scene-name ...]

set -euo pipefail

# Without it a failure inside $(...) does not stop the script; the caller would
# go on with an empty count.
shopt -s inherit_errexit

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

manifest="benchmarks/manifest.txt"

# The timing build: the instrumented one would count its own counters.
renderer="${PATHTRACER:-build/release/pathtracer}"

baseline=""
output="out/instructions.tsv"
requested=()
build=true

while [[ $# -gt 0 ]]; do
    case "$1" in
    --no-build)
        build=false
        shift
        ;;
    --baseline)
        [[ $# -ge 2 ]] || { echo "error: --baseline needs a file" >&2; exit 1; }
        baseline="$2"
        shift 2
        ;;
    --output)
        [[ $# -ge 2 ]] || { echo "error: --output needs a file" >&2; exit 1; }
        output="$2"
        shift 2
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
        requested+=("$1")
        shift
        ;;
    esac
done

# A binary older than the sources would be counted without a word: the change being measured would read as none.
if [[ "${build}" == true && -z "${PATHTRACER:-}" ]]; then
    cmake --build --preset release --target pathtracer >&2
fi

if [[ ! -x "${renderer}" ]]; then
    echo "error: renderer not found at '${renderer}'" >&2
    echo "hint:  cmake --preset release && cmake --build --preset release" >&2
    exit 1
fi

if [[ -n "${PERF:-}" ]]; then
    perf="${PERF}"
elif perf --version > /dev/null 2>&1; then
    perf="perf"
else
    perf=$(printf '%s\n' /usr/lib/linux-tools/*/perf | sort -V | tail -n 1)
fi

if ! "${perf}" --version > /dev/null 2>&1; then
    echo "error: no working perf found; set PERF to its path" >&2
    exit 1
fi

if [[ -n "${baseline}" && ! -r "${baseline}" ]]; then
    echo "error: cannot read baseline '${baseline}'" >&2
    exit 1
fi

# Names are checked before anything runs: a typo should not cost a full pass first.
declare -A listed=()
while read -r scene _; do
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then continue; fi
    listed["$(basename "${scene}" .json)"]=1
done < "${manifest}"

declare -A wanted=()
for name in "${requested[@]}"; do
    if [[ -z "${listed[${name}]:-}" ]]; then
        echo "error: '${name}' is not in ${manifest}" >&2
        exit 1
    fi
    wanted["${name}"]=1
done

perf_out=$(mktemp)
perf_log=$(mktemp)
trap 'rm -f "${perf_out}" "${perf_log}"' EXIT

# User-space instructions retired by one single-threaded benchmark run. The
# renderer's stdin is closed so that nothing it starts can eat the manifest.
count_instructions() {
    if ! "${perf}" stat -x, -e instructions:u -o "${perf_out}" -- \
        "${renderer}" "$@" --threads 1 --bench --bench-runs 1 --log-level warning < /dev/null > /dev/null 2> "${perf_log}"; then
        echo "error: the counted run failed:" >&2
        cat "${perf_log}" >&2
        exit 1
    fi

    # Summed over every line naming the event: a hybrid CPU reports one line per
    # core type, and the run may have touched both. A core type the process never
    # ran on reads "<not counted>" and is skipped by the numeric test.
    local count
    count=$(awk -F, '$3 ~ /instructions/ && $1 ~ /^[0-9]+$/ { sum += $1; n++ }
                     END { if (n > 0) printf "%.0f\n", sum }' "${perf_out}")

    if [[ ! "${count}" =~ ^[0-9]+$ ]]; then
        echo "error: perf reported no instruction count; its output was:" >&2
        cat "${perf_out}" >&2
        exit 1
    fi
    echo "${count}"
}

mkdir -p "$(dirname -- "${output}")"

header="# scene\trender_instructions\tper_sample"
if [[ -n "${baseline}" ]]; then
    header+="\tbaseline\tchange_percent"
fi
printf '%b\n' "${header}" | tee "${output}"

# The manifest is read on fd 3, so the loop body's own stdin stays free.
while read -r scene width height spp <&3; do
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then continue; fi

    name=$(basename "${scene}" .json)
    if [[ ${#wanted[@]} -gt 0 && -z "${wanted[${name}]:-}" ]]; then continue; fi

    echo "==> ${name}" >&2
    full=$(count_instructions "${scene}" --width "${width}" --height "${height}" --spp "${spp}")
    setup=$(count_instructions "${scene}" --width 1 --height 1 --spp 1)

    render=$((full - setup))
    per_sample=$(awk -v r="${render}" -v n="$((width * height * spp))" 'BEGIN { printf "%.1f", r / n }')

    line="${name}\t${render}\t${per_sample}"
    if [[ -n "${baseline}" ]]; then
        before=$(awk -F'\t' -v s="${name}" '$1 == s { print $2 }' "${baseline}")
        if [[ "${before}" =~ ^[0-9]+$ ]]; then
            change=$(awk -v a="${render}" -v b="${before}" 'BEGIN { printf "%+.3f", 100 * (a - b) / b }')
            line+="\t${before}\t${change}"
        else
            line+="\t-\t-"
        fi
    fi
    printf '%b\n' "${line}" | tee -a "${output}"
done 3< "${manifest}"
