#!/usr/bin/env bash
#
# Runs every CMake workflow preset in turn - configure, build and test for each
# configuration - and prints one pass/fail table at the end.
#
# A single workflow preset cannot do this: CMake allows one configure step per
# workflow, and a workflow cannot call another. A preset file that tries is
# rejected whole, which would break every other preset in it as well.
#
# The list comes from CMake rather than from this script, so a preset added to
# CMakePresets.json (or to a local CMakeUserPresets.json) is picked up without
# editing anything here. Naming presets on the command line runs only those, in
# the order given.
#
# A failure does not stop the run. Every workflow has its own build directory,
# so one configuration failing says nothing about the next, and a run this long
# should report every failure at once rather than one per attempt.
#
# Each workflow's output goes to the terminal and to out/workflows/<preset>.log.
# A log is deleted once its workflow passes, so the logs left behind are the
# failures' evidence.
#
# Exit status: 0 every workflow passed, 1 at least one failed, 2 the script
# could not run them. An interrupted run still prints the table so far, and
# exits 130 as the shell would.
#
# Usage: scripts/run-workflows.sh [preset ...]

set -euo pipefail

# Same root resolution as the other scripts. It matters more here than in most:
# `cmake --workflow` reads the presets from the working directory.
script_dir=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)
repo_root=$(cd -- "${script_dir}/.." && pwd)
cd "${repo_root}"

log_dir="out/workflows"

selected=()

while [[ $# -gt 0 ]]; do
    case "$1" in
    -h | --help)
        sed -n '2,/^$/s/^# \?//p' "${BASH_SOURCE[0]}"
        exit 0
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

if ! command -v cmake >/dev/null 2>&1; then
    echo "error: 'cmake' not found on PATH" >&2
    exit 2
fi

# Every preset resolves its toolchain file through this variable, and CMake's
# error when it is unset does not say so. Left to CMake, it would surface as
# one baffling configure failure per workflow.
if [[ -z "${VCPKG_ROOT:-}" ]]; then
    echo "error: VCPKG_ROOT is not set" >&2
    echo "hint:  export VCPKG_ROOT=/path/to/vcpkg  (see docs/building.md)" >&2
    exit 2
fi

# CMake's own listing rather than a parse of the JSON: it resolves includes and
# CMakeUserPresets.json, leaves hidden presets out, and rejects an invalid
# preset file here, before anything has been built.
if ! listing=$(cmake --workflow --list-presets 2>&1); then
    echo "error: cmake could not read the workflow presets" >&2
    printf '%s\n' "${listing}" | sed 's/^/    /' >&2
    exit 2
fi

# Each preset is listed as `  "name"`, followed by ` - display name` when it has
# one. This is CMake's human-readable wording, and there is no machine-readable
# form of it; if the wording ever changes, nothing matches and the check below
# fails loudly rather than running an empty set.
available=()
while IFS= read -r line; do
    if [[ "${line}" =~ ^\ +\"([^\"]+)\" ]]; then
        available+=("${BASH_REMATCH[1]}")
    fi
done <<< "${listing}"

if [[ ${#available[@]} -eq 0 ]]; then
    echo "error: cmake listed no workflow presets" >&2
    exit 2
fi

if [[ ${#selected[@]} -eq 0 ]]; then
    selected=("${available[@]}")
else
    declare -A known=()
    declare -A seen=()
    for preset in "${available[@]}"; do
        known["${preset}"]=1
    done

    # Every name is checked before anything runs: a typo in the last name should
    # not surface an hour into the run. A name given twice is a typo too, rather
    # than a request to build the same directory again.
    invalid=false
    for preset in "${selected[@]}"; do
        if [[ -z "${preset}" || -z "${known[${preset}]:-}" ]]; then
            echo "error: no workflow preset named '${preset}'" >&2
            invalid=true
        elif [[ -n "${seen[${preset}]:-}" ]]; then
            echo "error: workflow preset '${preset}' given twice" >&2
            invalid=true
        else
            seen["${preset}"]=1
        fi
    done

    if [[ "${invalid}" == true ]]; then
        echo "hint:  cmake --workflow --list-presets" >&2
        exit 2
    fi
fi

total=${#selected[@]}

# Wide enough for the longest name, so the table lines up whatever the presets
# are called.
name_width=0
for preset in "${selected[@]}"; do
    if (( ${#preset} > name_width )); then
        name_width=${#preset}
    fi
done

format_duration() {
    local seconds=$1
    if (( seconds >= 3600 )); then
        printf '%dh%02dm%02ds' $((seconds / 3600)) $((seconds % 3600 / 60)) $((seconds % 60))
    else
        printf '%dm%02ds' $((seconds / 60)) $((seconds % 60))
    fi
}

# One entry per workflow that ran, in run order. Indices line up with
# `selected`; a preset past the end of these never started.
verdicts=()
durations=()
failed_steps=()

# Ctrl-C reaches cmake and its children directly, since they share the
# terminal's process group; trapping it here only keeps this script alive long
# enough to print the table. Children start with the default disposition, so
# the trap does not make them ignore it.
interrupted=false
trap 'interrupted=true' INT

mkdir -p "${log_dir}"

echo "==> ${total} workflows: ${selected[*]}"

run_start=${SECONDS}
index=0

for preset in "${selected[@]}"; do
    index=$((index + 1))
    log="${log_dir}/${preset}.log"

    echo
    echo "==> [${index}/${total}] ${preset}"

    start=${SECONDS}

    # Piped through tee, so Ninja is not writing to a terminal: it prints one
    # line per step instead of redrawing a status line, which is also what keeps
    # the log readable. pipefail makes the status cmake's, not tee's.
    if cmake --workflow --preset "${preset}" 2>&1 | tee "${log}"; then
        code=0
    else
        code=$?
    fi

    durations+=($((SECONDS - start)))

    if [[ "${code}" -eq 0 ]]; then
        verdicts+=("ok")
        failed_steps+=("")
        rm -f "${log}"
    elif [[ "${interrupted}" == true ]]; then
        verdicts+=("STOPPED")
        failed_steps+=("")
    else
        verdicts+=("FAILED")

        # The step CMake announced last is the one that failed. Informational
        # only - the verdict above comes from the exit status - so a change in
        # CMake's wording costs this column and nothing else.
        step=$(sed -n 's/^Executing workflow step [0-9]* of [0-9]*: \([a-z]*\) preset .*/\1/p' "${log}" | tail -n 1)
        failed_steps+=("${step:-?}")
    fi

    if [[ "${interrupted}" == true ]]; then
        break
    fi
done

echo
echo "==> summary"
echo

passed=0
for i in "${!selected[@]}"; do
    preset=${selected[$i]}

    if (( i >= ${#verdicts[@]} )); then
        printf '%-*s  %-7s\n' "${name_width}" "${preset}" "not run"
        continue
    fi

    verdict=${verdicts[$i]}
    duration=$(format_duration "${durations[$i]}")

    case "${verdict}" in
    ok)
        passed=$((passed + 1))
        printf '%-*s  %-7s  %8s\n' "${name_width}" "${preset}" "${verdict}" "${duration}"
        ;;
    FAILED)
        printf '%-*s  %-7s  %8s  %-14s %s\n' "${name_width}" "${preset}" "${verdict}" "${duration}" \
            "at ${failed_steps[$i]}" "${log_dir}/${preset}.log"
        ;;
    *)
        printf '%-*s  %-7s  %8s\n' "${name_width}" "${preset}" "${verdict}" "${duration}"
        ;;
    esac
done

elapsed=$(format_duration $((SECONDS - run_start)))

echo
if [[ "${interrupted}" == true ]]; then
    echo "interrupted: ${passed} of ${total} workflows passed before it, in ${elapsed}"
    exit 130
fi

echo "${passed} of ${total} workflows passed in ${elapsed}"

if [[ "${passed}" -ne "${total}" ]]; then
    exit 1
fi
