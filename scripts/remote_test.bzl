"""Bazel rule to run remote hardware test."""

def _remote_hardware_check_impl(ctx):
    test_script = ctx.file.test_script
    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    # The bash script that Bazel's test runner will execute locally
    script_content = """#!/usr/bin/env bash
set -euo pipefail

# In 'bazel test', positional arguments are stripped, so we must use environment variables
if [[ -z "${BOARD_USER:-}" || -z "${BOARD_HOST:-}" ]]; then
    echo "ERROR: BOARD_USER and BOARD_HOST test variables must be set." >&2
    echo "Usage: bazel test --test_env=BOARD_USER=firefly --test_env=BOARD_HOST=192.168.x.x //..." >&2
    exit 1
fi

REMOTE_DIR="{remote_dir}"
TEST_SCRIPT_NAME="{test_script_name}"

if [[ -n "${RUNFILES_DIR:-}" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${BASH_SOURCE[0]}.runfiles" ]]; then
    RUNFILES="${BASH_SOURCE[0]}.runfiles"
else
    echo "ERROR: cannot find Bazel runfiles directory" >&2
    exit 1
fi

# Locate the Python test script in the runfiles tree
TEST_SCRIPT=$(find "$RUNFILES" -type f -name "$TEST_SCRIPT_NAME" | head -n 1)

if [[ -z "$TEST_SCRIPT" ]]; then
    echo "ERROR: could not find test script in runfiles" >&2
    exit 1
fi

echo "== Deploying Test Script to Board =="
ssh "${BOARD_USER}@${BOARD_HOST}" -- "mkdir -p ${REMOTE_DIR}"

echo "== Running Test on Hardware =="
# The exit code of 'ssh' perfectly mirrors the remote Python script's exit code.
# Bazel captures this exit code to determine PASS/FAIL.
ssh "${BOARD_USER}@${BOARD_HOST}" -- "cd ${REMOTE_DIR} && uv run ${TEST_SCRIPT_NAME}"
""".format(
        remote_dir = ctx.attr.remote_dir,
        test_script_name = test_script.basename,
    )

    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [test_script])

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

remote_hardware_check = rule(
    implementation = _remote_hardware_check_impl,
    attrs = {
        "test_script": attr.label(
            mandatory = True,
            allow_single_file = [".py"],
            doc = "The Python test script to deploy and run on the board.",
        ),
        "remote_dir": attr.string(
            default = "~/homebot/bin",
            doc = "Directory on the board where the test runs.",
        ),
    },
    executable = True,
)

def _remote_cc_check_impl(ctx):
    binary = ctx.attr.binary
    binary_default = binary[DefaultInfo]
    binary_executable = binary_default.files_to_run.executable
    binary_package = binary.label.package
    binary_name = binary.label.name

    # Get the paths of all data files
    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    script_content = """#!/usr/bin/env bash
set -euo pipefail

# Parse board destination.
if [[ $# -ge 1 && "$1" == *@* ]]; then
    BOARD_USER="${{1%%@*}}"
    BOARD_HOST="${{1#*@}}"
    shift
else
    if [[ -z "${{BOARD_USER:-}}" || -z "${{BOARD_HOST:-}}" ]]; then
        echo "Usage: $0 <user>@<host> [remote_args...]" >&2
        echo "   or: BOARD_USER=<user> BOARD_HOST=<host> $0 [remote_args...]" >&2
        exit 1
    fi
fi

REMOTE_DIR="{remote_dir}"
BINARY_PACKAGE="{binary_package}"
BINARY_NAME="{binary_name}"
DATA_FILES="{data_files_str}"

if [[ -n "${{RUNFILES_DIR:-}}" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${{BASH_SOURCE[0]}}.runfiles" ]]; then
    RUNFILES="${{BASH_SOURCE[0]}}.runfiles"
else
    echo "ERROR: cannot find Bazel runfiles directory" >&2
    exit 1
fi

# Bzlmod main repo is usually _main; legacy workspace name is homebot.
BINARY_CANDIDATES=(
    "${{RUNFILES}}/_main/${{BINARY_PACKAGE}}/${{BINARY_NAME}}"
    "${{RUNFILES}}/homebot/${{BINARY_PACKAGE}}/${{BINARY_NAME}}"
)

BINARY=""
for candidate in "${{BINARY_CANDIDATES[@]}}"; do
    if [[ -x "$candidate" ]]; then
        BINARY="$candidate"
        break
    fi
done

if [[ -z "$BINARY" ]]; then
    echo "ERROR: could not find executable ${{BINARY_PACKAGE}}/${{BINARY_NAME}} in runfiles" >&2
    exit 1
fi

echo "== Deploying Benchmark Binary to Board =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "mkdir -p ${{REMOTE_DIR}}"

echo "== Delete existing binary ==="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "rm -rf ${{REMOTE_DIR}}/${{BINARY_NAME}}"

scp "$BINARY" "${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_DIR}}/${{BINARY_NAME}}" > /dev/null

# Copy data files (if any exist)
if [[ -n "$DATA_FILES" ]]; then
    echo "== Copying data files =="
    for data_file in $DATA_FILES; do
        # Find the file in the local runfiles tree
        DATA_FILE_BASENAME=$(basename ${{data_file}})
        LOCAL_DATA=$(find "$RUNFILES" -name ${{DATA_FILE_BASENAME}} | head -n 1)
        if [[ -n "$LOCAL_DATA" ]]; then
            # SCP it to the same directory as the binary
            scp "$LOCAL_DATA" "${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_DIR}}/"
        fi
    done
fi

echo "== Running Google Benchmark on Hardware =="
# We pass $@ to forward Bazel's internal test flags (like --benchmark_filter) to the binary
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "cd ${{REMOTE_DIR}} && ./${{BINARY_NAME}} $@"
""".format(
        remote_dir = ctx.attr.remote_dir,
        binary_name = binary_name,
        binary_package = binary_package,
        data_files_str = data_files_str,
    )

    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    #runfiles = ctx.runfiles(files = ctx.files.srcs)
    runfiles = ctx.runfiles(files = [binary_executable])
    runfiles = runfiles.merge(binary.default_runfiles)

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

remote_cc_check = rule(
    implementation = _remote_cc_check_impl,
    attrs = {
        "binary": attr.label(
            mandatory = True,
            executable = True,
            cfg = "target",
            doc = "The cc_test or cc_binary target to deploy and run.",
        ),
        "data": attr.label_list(allow_files = True, doc = "Files to deploy alongside the binary"),
        "remote_dir": attr.string(
            default = "~/homebot/bin",
        ),
    },
    executable = True,
)
