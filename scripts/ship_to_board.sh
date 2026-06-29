#!/usr/bin/env bash
# Ship pico_interface host binaries to a target board via scp.
#
# Usage:
#   scripts/ship_to_board.sh <user>@<host> [dest_dir]
#   bazel run --config=arm64 //scripts:ship_to_board -- <user>@<host> [dest_dir]
#
# Environment overrides:
#   BOARD_USER  - destination username  (no default; required)
#   BOARD_HOST  - destination host      (no default; required)
#   BOARD_DIR   - destination directory (default: /home/$BOARD_USER/homebot/bin)

set -euo pipefail

# Resolve workspace root. Under `bazel run`, BUILD_WORKSPACE_DIRECTORY is set.
WORKSPACE_ROOT="${BUILD_WORKSPACE_DIRECTORY:-$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)}"
cd "${WORKSPACE_ROOT}"

BAZEL_CONFIG="${BAZEL_CONFIG:-arm64}"
BINARY_DIR="${WORKSPACE_ROOT}/bazel-bin/pico_interface"

usage() {
    echo "Usage: $0 <user@host> [dest_dir]" >&2
    echo "   or: BOARD_USER=<user> BOARD_HOST=<host> [BOARD_DIR=<dir>] $0" >&2
    echo "   or: bazel run --config=arm64 //scripts:ship_to_board -- <user@host> [dest_dir]" >&2
    exit 1
}

# Determine BOARD_USER and BOARD_HOST from args or environment.
if [[ $# -ge 1 ]]; then
    if [[ "$1" == *@* ]]; then
        BOARD_USER="${1%%@*}"
        BOARD_HOST="${1#*@}"
        echo "User: $BOARD_USER"
        echo "Host: $BOARD_HOST"
    else
        echo "ERROR: destination must be in user@host format" >&2
        usage
    fi
else
    if [[ -z "${BOARD_USER:-}" || -z "${BOARD_HOST:-}" ]]; then
        echo "ERROR: destination user and host are required" >&2
        usage
    fi
fi

BOARD_DIR="${BOARD_DIR:-/home/${BOARD_USER}/homebot/bin}"

if [[ $# -ge 2 ]]; then
    BOARD_DIR="$2"
fi

BINARIES=(
    pico_interface
    test_suite
    control
    calibrate_encoders
)

# When invoked via `bazel run --config=arm64 //scripts:ship_to_board`, the
# cc_binary data dependencies are already built for the target board.
if [[ -z "${BUILD_WORKSPACE_DIRECTORY:-}" ]]; then
    echo "== Building pico_interface for aarch64 (config=${BAZEL_CONFIG}) =="
    bazel build --config="${BAZEL_CONFIG}" "//pico_interface/..."
fi

echo "== Verifying built binaries =="
for binary in "${BINARIES[@]}"; do
    src="${BINARY_DIR}/${binary}"
    if [[ ! -x "${src}" ]]; then
        echo "ERROR: expected executable not found: ${src}" >&2
        echo "If running via 'bazel run', use: bazel run --config=arm64 //scripts:ship_to_board -- ..." >&2
        exit 1
    fi
done

echo "== Ensuring destination directory exists =="
ssh "${BOARD_USER}@${BOARD_HOST}" "mkdir -p ${BOARD_DIR}"

echo "== Shipping binaries to ${BOARD_USER}@${BOARD_HOST}:${BOARD_DIR} =="
for binary in "${BINARIES[@]}"; do
    scp "${BINARY_DIR}/${binary}" "${BOARD_USER}@${BOARD_HOST}:${BOARD_DIR}/"
done

echo "== Done. Deployed pico_interface binaries to board. =="
