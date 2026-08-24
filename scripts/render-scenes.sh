#!/usr/bin/env bash
#
# Renders every scene in scenes/ with one build preset.
#
# A scene listed in the golden manifest is rendered at the manifest's
# resolution and sample count, so the output is comparable to the reference
# set. A scene that is not listed falls back to its own authored settings.
#
# Usage: scripts/render-scenes.sh <preset> [output-dir]

set -euo pipefail

# Resolve the repository root from the script's own location, so relative paths
# work regardless of the caller's working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

if [[ $# -lt 1 || $# -gt 2 ]]; then
    echo "usage: $(basename -- "${BASH_SOURCE[0]}") <preset> [output-dir]" >&2
    exit 2
fi

preset="$1"
output_dir="${2:-out/${preset}}"

manifest="tests/golden/manifest.txt"

# Preset names the build directory, so the binary follows from it. Overridable
# for out-of-tree builds, matching render-goldens.sh.
renderer="${PATHTRACER:-build/${preset}/pathtracer}"

if [[ ! -x "${renderer}" ]]; then
    echo "error: renderer not found at '${renderer}'" >&2
    echo "hint:  cmake --preset ${preset} && cmake --build --preset ${preset}" >&2
    exit 1
fi

# Manifest rows keyed by scene path, read once. Grepping the manifest per scene
# would be quadratic, and a substring match would confuse two scenes whose
# names share a prefix.
declare -A overrides
while read -r scene width height spp; do
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then
        continue
    fi
    overrides["${scene}"]="${width} ${height} ${spp}"
done < "${manifest}"

# Without nullglob an unmatched pattern is passed through literally, and the
# renderer would be handed the string "scenes/*.json" as a filename.
shopt -s nullglob
scenes=(scenes/*.json)

if [[ ${#scenes[@]} -eq 0 ]]; then
    echo "error: no scene files found under scenes/" >&2
    exit 1
fi

mkdir -p "${output_dir}"

count=0
for scene in "${scenes[@]}"; do
    name=$(basename "${scene}" .json)

    # An array, not a string: every element survives as exactly one argument.
    args=(--format png --output "${output_dir}/${name}.png")

    if [[ -v overrides["${scene}"] ]]; then
        read -r width height spp <<< "${overrides[${scene}]}"
        args+=(--width "${width}" --height "${height}" --spp "${spp}")
        echo "==> ${name}  ${width}x${height}  ${spp} spp"
    else
        echo "==> ${name}  (scene settings)"
    fi

    "${renderer}" "${scene}" "${args[@]}"

    count=$((count + 1))
done

echo "wrote ${count} images to ${output_dir}/"
