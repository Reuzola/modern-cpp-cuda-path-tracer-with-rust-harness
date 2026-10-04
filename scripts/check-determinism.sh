#!/usr/bin/env bash
#
# Renders each scene once on a single thread and again on several thread
# counts, and compares every multi-threaded image with the single-threaded one
# byte for byte.
#
# The golden set answers "is the image right"; this answers "is the image the
# same one every time, however many threads drew it". They were one check while
# the golden threshold was zero, and separating them is what lets that
# threshold be a tolerance.
#
# Every run is a fresh process, so this also covers what an in-process check
# cannot: scene loading, arena allocation and BVH construction happening again
# at different addresses have to produce the same tree. Two runs that each
# match the single-threaded image also match each other, so reproducibility
# from run to run needs no comparison of its own.
#
# Exit status: 0 every scene reproduced, 1 at least one did not, 2 the script
# could not run the comparison.
#
# Usage: scripts/check-determinism.sh [scene ...]

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

# Overridable for out-of-tree build directories, matching the other scripts.
renderer="${PATHTRACER:-build/release/pathtracer}"

# Small and cheap: determinism is structural, so a scene either reproduces or
# it does not. A heavier workload costs time and answers the same question.
# The aspect ratio is deliberately ignored - nothing here looks at the image.
width=160
height=120
spp=9

# One thread is the reference: nothing runs concurrently, so any difference is
# the threads' doing. Then every core, as a render normally runs, and more
# threads than cores, so they also interleave by preemption. Stated rather than
# left to the renderer's default, which on a one-core machine is one thread and
# would compare the reference with itself.
cores=$(nproc)
thread_counts=("${cores}" "$((2 * cores + 1))")

# Default set, chosen by which construction path each one exercises rather
# than by what it looks like:
#   gilded_orrery  - the most trees, meshes, instances and a medium
#   random_spheres - the only scene that generates its geometry at load time
#   cornell_smoke  - volumes, which draw from the sampler inside the integrator
default_scenes=(
    scenes/gilded_orrery.json
    scenes/random_spheres.json
    scenes/cornell_smoke.json
)

if [[ $# -gt 0 ]]; then
    scenes=("$@")
else
    scenes=("${default_scenes[@]}")
fi

if [[ ! -x "${renderer}" ]]; then
    echo "error: renderer not found at '${renderer}'" >&2
    echo "hint:  cmake --preset release && cmake --build --preset release" >&2
    exit 2
fi

status=0

# The worst code wins: a scene that merely differs (1) must not mask one the
# script could not compare at all (2). An if, not `(( )) &&`: under set -e a
# false test as a function's last command would end the script.
raise_status() {
    if (( $1 > status )); then
        status=$1
    fi
}

# mktemp picks a name nothing else owns; a fixed path would collide with a
# second run and compare that run's output instead.
work_dir=$(mktemp -d)

cleanup() {
    # Matching images are deleted as they pass, so anything left is a failure's
    # evidence. An empty directory means nothing failed, or nothing was rendered.
    if [[ -z "$(ls -A "${work_dir}")" ]]; then
        rm -rf "${work_dir}"
    else
        echo "the differing renders are in ${work_dir}" >&2
    fi
}
trap cleanup EXIT

# No --seed: the seed the scene file carries is the one the golden set and
# every benchmark record use, so that is the one whose reproducibility is
# worth asserting. EXR, not PNG: it holds the film's floats losslessly, while a
# PNG is tone mapped and quantized to 8 bits, so a difference in a pixel's low
# bits would only show if it happened to cross a quantization step.
render() {
    local scene=$1 threads=$2 output=$3
    "${renderer}" "${scene}" \
        --width "${width}" --height "${height}" --spp "${spp}" \
        --threads "${threads}" \
        --format exr --output "${output}" \
        --log-level warning
}

for scene in "${scenes[@]}"; do
    name=$(basename "${scene}" .json)

    if [[ ! -f "${scene}" ]]; then
        printf '%-24s %-7s %s\n' "${name}" "ERROR" "no such scene file: ${scene}"
        raise_status 2
        continue
    fi

    # Named by role, not by thread count: on a one-core machine a variant is
    # also one thread, and must not overwrite the image it is compared with.
    reference="${work_dir}/${name}.reference.exr"

    # A renderer that fails is the script failing to compare, not the scene
    # failing to reproduce.
    if ! render "${scene}" 1 "${reference}"; then
        printf '%-24s %-7s %s\n' "${name}" "ERROR" "the single-threaded render failed"
        raise_status 2
        continue
    fi

    reproduced=true

    for threads in "${thread_counts[@]}"; do
        image="${work_dir}/${name}.threads-${threads}.exr"

        if ! render "${scene}" "${threads}" "${image}"; then
            printf '%-24s %-11s %-7s %s\n' "${name}" "${threads} threads" "ERROR" "the render failed"
            raise_status 2
            reproduced=false
            continue
        fi

        # -s: the verdict is printed below, and cmp's own byte offset says nothing
        # useful about a compressed image.
        if cmp -s "${reference}" "${image}"; then
            printf '%-24s %-11s %s\n' "${name}" "${threads} threads" "identical"
            rm -f "${image}"
        else
            printf '%-24s %-11s %-7s %s\n' "${name}" "${threads} threads" "FAILED" "differs from the single-threaded render"
            raise_status 1
            reproduced=false
        fi
    done

    # Kept while anything still differs from it: it is half of the evidence.
    if [[ "${reproduced}" == true ]]; then
        rm -f "${reference}"
    fi
done

echo
if [[ "${status}" -eq 0 ]]; then
    echo "${#scenes[@]} scenes reproduced exactly on 1, ${thread_counts[0]} and ${thread_counts[1]} threads"
fi

exit "${status}"
