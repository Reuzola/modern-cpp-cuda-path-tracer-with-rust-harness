#!/usr/bin/env bash
#
# Builds, renders and compares the golden image set in one pass.
#
# Renders into a scratch directory, so the tracked references are never
# overwritten: this script answers "did anything change", not "make it match".
#
# Each scene carries its own tolerance in the manifest; there is no global
# threshold to pass here. Why the tolerances differ per scene, and how they
# were measured, is in docs/golden-images.md.
#
# Exit status: 0 every reference matched, 1 at least one differed, 2 the
# comparison tool itself failed. This mirrors scene-tool's own contract.
#
# Usage: scripts/check-goldens.sh [--no-build] [--diff-dir <dir>]

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

# Hard-coded, not a parameter: references are only valid from `release`, and a
# run against another preset would report differences that mean nothing.
preset="release"

manifest="tests/golden/manifest.txt"

# Stated rather than inherited from the tool's default: every tolerance in the
# manifest was measured at this block size, so a change to that default must
# not silently reinterpret all of them.
block_size=8

do_build=true
diff_dir=""

while [[ $# -gt 0 ]]; do
    case "$1" in
    --no-build)
        do_build=false
        shift
        ;;
    --diff-dir)
        # An option that takes a value must prove the value is there; without
        # this check a trailing --diff-dir silently consumes the next flag.
        [[ $# -ge 2 ]] || { echo "error: --diff-dir needs a value" >&2; exit 2; }
        diff_dir="$2"
        shift 2
        ;;
    -h | --help)
        sed -n '2,/^$/s/^# \?//p' "${BASH_SOURCE[0]}"
        exit 0
        ;;
    *)
        echo "error: unknown argument '$1'" >&2
        exit 2
        ;;
    esac
done

tool="tools/scene-tool/target/release/scene-tool"

if [[ "${do_build}" == true ]]; then
    echo "==> building the renderer"
    cmake --preset "${preset}"
    cmake --build --preset "${preset}"

    echo "==> building scene-tool"
    # A subshell, so the directory change does not leak into the rest of the run.
    ( cd tools/scene-tool && cargo build --locked --release )
fi

if [[ ! -x "${tool}" ]]; then
    echo "error: scene-tool not found at '${tool}'" >&2
    exit 2
fi

status=0

# mktemp picks a name nothing else owns; a fixed /tmp path would collide with a
# second run and silently compare against the other one's output.
work_dir=$(mktemp -d)

# Kept outside the scratch directory when the caller named one, so CI can
# collect the images after this script has cleaned up after itself.
if [[ -z "${diff_dir}" ]]; then
    diff_dir="${work_dir}/diff"
fi
mkdir -p "${diff_dir}"

cleanup() {
    if [[ "${status}" -eq 0 ]]; then
        rm -rf "${work_dir}"
    else
        echo
        echo "renders kept in ${work_dir}" >&2
        echo "difference images in ${diff_dir}" >&2
    fi
}
# EXIT fires on normal return, on `set -e`, and on Ctrl-C, so the scratch
# directory has exactly one owner and one exit path.
trap cleanup EXIT

echo "==> rendering"
scripts/render-goldens.sh "${work_dir}"

echo
echo "==> comparing (block ${block_size}x${block_size}, per-scene tolerance)"

matched=0
total=0

# Names seen in the manifest, so a reference with no row can be reported below.
declare -A expected=()

# The manifest drives the loop, not the directory listing: the tolerance is a
# manifest column, and a scene with no row has no tolerance to apply.
while read -r scene width height spp tol; do
    # Skip blank lines and comments. The header row starts with '#' too, so
    # its columns never reach the comparison.
    if [[ -z "${scene}" || "${scene}" == \#* ]]; then
        continue
    fi

    name=$(basename "${scene}" .json).png

    # Two rows for one scene would render twice and compare the second render
    # against itself, which reads as a pass whatever the first row said.
    if [[ -n "${expected[${name}]:-}" ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "duplicate manifest row"
        status=2
        continue
    fi

    expected["${name}"]=1
    total=$((total + 1))

    reference="tests/golden/${name}"
    actual="${work_dir}/${name}"

    # Rejected here rather than by the tool: a malformed column would otherwise
    # surface as a confusing argument error, once per scene.
    if [[ ! "${tol}" =~ ^[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?$ ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "invalid tolerance '${tol}' in the manifest"
        status=2
        continue
    fi

    if [[ ! -f "${reference}" ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "no reference image (run scripts/render-goldens.sh)"
        status=1
        continue
    fi

    if [[ ! -f "${actual}" ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "not rendered"
        status=1
        continue
    fi

    if output=$("${tool}" compare "${reference}" "${actual}" \
        --threshold "${tol}" --block-size "${block_size}" \
        --diff "${diff_dir}/${name}" 2>&1); then
        code=0
    else
        code=$?
    fi

    case "${code}" in
    0) verdict="ok" ;;
    1) verdict="FAILED" ;;
    *) verdict="ERROR" ;;
    esac

    # The tool's first line already reads "block rmse ...", so it is placed
    # beside the verdict verbatim. Nothing here parses or reformats it: a
    # wording change in the tool must not be able to break this script.
    first_line=${output%%$'\n'*}
    printf '%-28s %-7s tol %-8s %s\n' "${name}" "${verdict}" "${tol}" "${first_line}"

    # Anything past the first line is detail (a multi-line clap error), indented
    # under the verdict rather than competing with it.
    rest=${output#"${first_line}"}
    if [[ -n "${rest}" ]]; then
        printf '%s\n' "${rest#$'\n'}" | sed 's/^/    /'
    fi

    if [[ "${code}" -eq 0 ]]; then
        matched=$((matched + 1))
    elif [[ "${code}" -gt "${status}" ]]; then
        # The worst code wins: a scene that merely differs (1) must not mask a
        # scene where the tool itself failed (2).
        status="${code}"
    fi
done < "${manifest}"

# A reference nobody renders is a reference nobody checks. It would go stale
# silently, so it is reported rather than ignored.
for reference in tests/golden/*.png; do
    name=$(basename "${reference}")
    if [[ -z "${expected[${name}]:-}" ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "reference has no manifest row"
        status=1
    fi
done

echo
echo "${matched} of ${total} references matched"

exit "${status}"
