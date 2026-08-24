#!/usr/bin/env bash
#
# Builds, renders and compares the golden image set in one pass.
#
# Renders into a scratch directory, so the tracked references are never
# overwritten: this script answers "did anything change", not "make it match".
#
# Exit status: 0 every reference matched, 1 at least one differed, 2 the
# comparison tool itself failed. This mirrors scene-tool's own contract.
#
# Usage: scripts/check-goldens.sh [--threshold <value>] [--no-build]

set -euo pipefail

script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

# Hard-coded, not a parameter: references are only valid from `release`, and a
# run against another preset would report differences that mean nothing.
preset="release"

threshold="0.0"
do_build=true

while [[ $# -gt 0 ]]; do
    case "$1" in
    --threshold)
        # An option that takes a value must prove the value is there; without
        # this check a trailing --threshold silently consumes the next flag.
        [[ $# -ge 2 ]] || { echo "error: --threshold needs a value" >&2; exit 2; }
        threshold="$2"
        # Rejected here rather than 12 times over by the tool: a leading '-' is
        # also parsed as a flag downstream, so the message would be misleading.
        [[ "${threshold}" =~ ^[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?$ ]] ||
            { echo "error: --threshold must be a non-negative number, got '${threshold}'" >&2; exit 2; }
        shift 2
        ;;
    --no-build)
        do_build=false
        shift
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
diff_dir="${work_dir}/diff"
mkdir -p "${diff_dir}"

cleanup() {
    if [[ "${status}" -eq 0 ]]; then
        rm -rf "${work_dir}"
    else
        echo
        echo "renders and difference images kept in ${work_dir}" >&2
    fi
}
# EXIT fires on normal return, on `set -e`, and on Ctrl-C, so the scratch
# directory has exactly one owner and one exit path.
trap cleanup EXIT

echo "==> rendering"
scripts/render-goldens.sh "${work_dir}"

echo
echo "==> comparing (threshold ${threshold})"

matched=0
total=0

for reference in tests/golden/*.png; do
    name=$(basename "${reference}")
    actual="${work_dir}/${name}"
    total=$((total + 1))

    if [[ ! -f "${actual}" ]]; then
        printf '%-28s %-7s %s\n' "${name}" "ERROR" "not rendered (no manifest row)"
        status=1
        continue
    fi

    if output=$("${tool}" compare "${reference}" "${actual}" \
        --threshold "${threshold}" --diff "${diff_dir}/${name}" 2>&1); then
        code=0
    else
        code=$?
    fi

    case "${code}" in
    0) verdict="ok" ;;
    1) verdict="FAILED" ;;
    *) verdict="ERROR" ;;
    esac

    # The tool's first line already reads "rmse ..., max abs diff ...", so it is
    # placed beside the verdict verbatim. Nothing here parses or reformats it:
    # a wording change in the tool must not be able to break this script.
    first_line=${output%%$'\n'*}
    printf '%-28s %-7s %s\n' "${name}" "${verdict}" "${first_line}"

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
done

echo
echo "${matched} of ${total} references matched"

exit "${status}"
