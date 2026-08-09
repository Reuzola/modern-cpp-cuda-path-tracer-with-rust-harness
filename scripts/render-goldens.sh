#!/usr/bin/env bash
#
# Regenerates the golden image reference set listed in tests/golden/manifest.txt.
#
# Reference renders are valid only from the `release` preset.

# -e: stop at the first failed render instead of leaving a half-updated set.
# -u: an unset variable is a bug, not an empty string.
# -o pipefail: a failure anywhere in a pipeline fails the pipeline.
set -euo pipefail

# Resolve the repository root from the script's own location, so paths in the
# manifest work regardless of the caller's working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

manifest="tests/golden/manifest.txt"

# Where to write. Defaults to the tracked set; pass a scratch directory to
# render without overwriting the references.
output_dir="${1:-tests/golden}"

# Overridable for out-of-tree build directories.
renderer="${PATHTRACER:-build/release/pathtracer}"

if [[ ! -x "${renderer}" ]]; then
    echo "error: renderer not found at '${renderer}'" >&2
    echo "hint:  cmake --preset release && cmake --build --preset release" >&2
    exit 1
fi

mkdir -p "${output_dir}"

count=0
while read -r scene width height spp; do
    # Skip blank lines and comments. The header row starts with '#' too, so
    # its columns never reach the renderer.
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then
        continue
    fi

    name=$(basename "${scene}" .json)
    output="${output_dir}/${name}.png"

    echo "==> ${name}  ${width}x${height}  ${spp} spp"

    # --format is stated even though png is the default: the format of the
    # reference set is a decision, not something to inherit silently.
    # --log-level warning drops the progress bar and timing line; this script
    # produces images, not measurements.
    "${renderer}" "${scene}" \
        --width "${width}" --height "${height}" --spp "${spp}" \
        --format png --output "${output}" --log-level warning

    count=$((count + 1))
done < "${manifest}"

echo "wrote ${count} images to ${output_dir}/"
