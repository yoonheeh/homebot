#!/usr/bin/env bash
set -euo pipefail

REMOTE_DIR="@@REMOTE_DIR@@"
TARGET_NAME="@@TARGET_NAME@@"
DATA_FILES="@@DATA_FILES@@"
RUN_CMD="@@RUN_CMD@@"

# The board parsing you already wrote (perfectly handles bazel test stripping args!)
if [[ $# -ge 1 && "$1" == *@* ]]; then
    BOARD_USER="${1%%@*}"
    BOARD_HOST="${1#*@}"
    shift
else
    if [[ -z "${BOARD_USER:-}" || -z "${BOARD_HOST:-}" ]]; then
        echo "Usage: $0 <user>@<host> [remote_args...]" >&2
        echo "   or: BOARD_USER=<user> BOARD_HOST=<host> $0 [remote_args...]" >&2
        exit 1
    fi
fi

# Locate runfiles
if [[ -n "${RUNFILES_DIR:-}" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${BASH_SOURCE[0]}.runfiles" ]]; then
    RUNFILES="${BASH_SOURCE[0]}.runfiles"
else
    echo "ERROR: cannot find Bazel runfiles directory" >&2
    exit 1
fi

# 1. Unified search for the target file (works for C++ binaries AND python scripts)
LOCAL_TARGET=$(find -L "$RUNFILES" -type f -name "$TARGET_NAME" | head -n 1)

if [[ -z "$LOCAL_TARGET" ]]; then
    echo "ERROR: could not find $TARGET_NAME in runfiles" >&2
    exit 1
fi

echo "== Deploying ${TARGET_NAME} to ${BOARD_USER}@${BOARD_HOST}:${REMOTE_DIR} =="
ssh "${BOARD_USER}@${BOARD_HOST}" -- "mkdir -p ${REMOTE_DIR} && rm -f ${REMOTE_DIR}/${TARGET_NAME}"
scp "$LOCAL_TARGET" "${BOARD_USER}@${BOARD_HOST}:${REMOTE_DIR}/${TARGET_NAME}"
ssh "${BOARD_USER}@${BOARD_HOST}" -- "chmod +x ${REMOTE_DIR}/${TARGET_NAME}"

# 2. Copy data files
if [[ -n "$DATA_FILES" ]]; then
    echo "== Copying data files =="
    for data_file in $DATA_FILES; do
        DATA_FILE_BASENAME=$(basename ${data_file})
        LOCAL_DATA=$(find -L "$RUNFILES" -type f -name ${DATA_FILE_BASENAME} | head -n 1)
        if [[ -n "$LOCAL_DATA" ]]; then
            scp "$LOCAL_DATA" "${BOARD_USER}@${BOARD_HOST}:${REMOTE_DIR}/"
        fi
    done
fi

# 3. Dynamic Execution
if [[ -n "$RUN_CMD" ]]; then
    echo "== Running Target on Hardware =="
    # $RUN_CMD is expanded locally by bash, which is exactly what we want 
    # so that "$@" forwards Bazel's test flags to the remote machine.
    ssh "${BOARD_USER}@${BOARD_HOST}" -- "cd ${REMOTE_DIR} && ${RUN_CMD}"
fi
