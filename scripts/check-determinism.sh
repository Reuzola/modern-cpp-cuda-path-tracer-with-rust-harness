#!/usr/bin/env bash
#
# Renders the same scene twice from the same binary and compares the two
# images byte for byte.
#
# The golden set answers "is the image right"; this answers "is the image the
# same one every time". They were one check while the golden threshold was
# zero, and separating them is what lets that threshold be a tolerance.
#
# Every run is a fresh process, so this also covers what an in-process check
# cannot: scene loading, arena allocation and BVH construction happening twice
# at different addresses have to produce the same tree.
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

# mktemp picks a name nothing else owns; a fixed path would collide with a
# second run and compare that run's output instead.
work_dir=$(mktemp -d)

cleanup() {
    # Matching pairs are deleted as they pass, so anything left is a failure's
    # evidence. An empty directory means the run never got as far as rendering.
    if [[ -z "$(ls -A "${work_dir}")" ]]; then
        rm -rf "${work_dir}"
    else
        echo "the differing renders are in ${work_dir}" >&2
    fi
}
trap cleanup EXIT

for scene in "${scenes[@]}"; do
    name=$(basename "${scene}" .json)

    if [[ ! -f "${scene}" ]]; then
        printf '%-24s %-7s %s\n' "${name}" "ERROR" "no such scene file: ${scene}"
        status=2
        continue
    fi

    # No --seed: the seed the scene file carries is the one the golden set and
    # every benchmark record use, so that is the one whose reproducibility is
    # worth asserting.
    for pass in first second; do
        "${renderer}" "${scene}" \
            --width "${width}" --height "${height}" --spp "${spp}" \
            --format png --output "${work_dir}/${name}.${pass}.png" \
            --log-level warning
    done

    # -s: the verdict is printed below, and cmp's own byte offset says nothing
    # useful about a Monte Carlo image.
    if cmp -s "${work_dir}/${name}.first.png" "${work_dir}/${name}.second.png"; then
        printf '%-24s %s\n' "${name}" "identical"
        rm -f "${work_dir}/${name}.first.png" "${work_dir}/${name}.second.png"
    else
        printf '%-24s %-7s %s\n' "${name}" "FAILED" "two runs of the same binary disagree"
        status=1
    fi
done

echo
if [[ "${status}" -eq 0 ]]; then
    echo "${#scenes[@]} scenes reproduced exactly"
fi

exit "${status}"
