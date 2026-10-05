#!/usr/bin/env bash
#
# Measures the interactive viewer's frame time on a fixed scene set and writes
# one NDJSON record per scene and thread count.
#
# The viewer opens a real window, so this needs a display and cannot run in CI.
# Each run drives the camera itself and exits on its own; leave the window alone
# while it runs. Scenes render at their own resolution and sample count.
#
# Thread counts alternate within each scene instead of running as separate
# sweeps: the host drifts over minutes, and pairing keeps that drift out of
# the comparison.
#
# --frames N is the frame count of each phase (moving, then still). A
# one-thread run of a heavy scene takes seconds per frame; lower N for it.
#
# Usage: scripts/measure-viewer.sh [--frames N] [--threads "1 16"] [output]

set -euo pipefail

# Same root resolution as the other scripts: scene paths are relative to the
# repository, not to the caller's working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

# Cheap, medium, heavy-material and heavy-geometry, in that order.
scenes=(quads cornell_box gilded_orrery argent_weave)

frames=120

# Space-separated. The default pairs the serial path with every hardware thread.
thread_counts="1 $(nproc)"

output=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --frames)
        [[ $# -ge 2 ]] || { echo "error: --frames needs a value" >&2; exit 1; }
        frames="$2"
        shift 2
        ;;
    --threads)
        [[ $# -ge 2 ]] || { echo "error: --threads needs a value" >&2; exit 1; }
        thread_counts="$2"
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
        # One positional only: a second one is a typo, not a second file.
        if [[ -n "${output}" ]]; then
            echo "error: unexpected argument '$1'" >&2
            exit 1
        fi
        output="$1"
        shift
        ;;
    esac
done

output="${output:-out/viewer-frames.ndjson}"

# Overridable for out-of-tree builds, like the other scripts' binaries.
viewer="${PATHTRACER_VIEWER:-build/release-viewer/pathtracer_viewer}"
if [[ ! -x "${viewer}" ]]; then
    echo "error: viewer not found at '${viewer}'" >&2
    echo "hint:  cmake --preset release-viewer && cmake --build --preset release-viewer" >&2
    exit 1
fi

mkdir -p "$(dirname -- "${output}")"

# Truncated once, appended to below: a partial run must not extend an old one.
: > "${output}"

count=0
for scene in "${scenes[@]}"; do
    # Unquoted on purpose: word splitting yields one thread count per word.
    for threads in ${thread_counts}; do
        echo "==> ${scene}  threads: ${threads}  ${frames} frames per phase"
        "${viewer}" "scenes/${scene}.json" --threads "${threads}" \
            --measure-frames "${frames}" --log-level warning >> "${output}"
        count=$((count + 1))
    done
done

echo "wrote ${count} records to ${output}"
