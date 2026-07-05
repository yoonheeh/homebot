"""Bazel rule to build a binary and run it on a remote board via ssh/scp."""

def _remote_run_impl(ctx):
    binary = ctx.attr.binary
    binary_default = binary[DefaultInfo]
    binary_executable = binary_default.files_to_run.executable

    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    # The binary's Bazel package/name, used to locate it inside runfiles.
    binary_package = binary.label.package
    binary_name = binary.label.name

    script_content = """#!/usr/bin/env bash
set -euo pipefail

REMOTE_DIR="{remote_dir}"
BINARY_PACKAGE="{binary_package}"
BINARY_NAME="{binary_name}"

# Locate runfiles root.
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

REMOTE_BIN="${{REMOTE_DIR}}/${{BINARY_NAME}}"

echo "== Ensuring ${{REMOTE_DIR}} exists on board =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "mkdir -p ${{REMOTE_DIR}}"

echo "== If needed, remove existing binary =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "cd ${{REMOTE_DIR}} && rm -rf ${{BINARY_NAME}}"

echo "== Deploying ${{BINARY_PACKAGE}}/${{BINARY_NAME}} to ${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_BIN}} =="
scp "$BINARY" "${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_BIN}}"

echo "== Running on board =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "chmod +x ${{REMOTE_BIN}} && ${{REMOTE_BIN}} \"$@\""
""".format(
        remote_dir = ctx.attr.remote_dir,
        binary_package = binary_package,
        binary_name = binary_name,
    )

    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [binary_executable])
    runfiles = runfiles.merge(binary_default.default_runfiles)

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

remote_run = rule(
    implementation = _remote_run_impl,
    attrs = {
        "binary": attr.label(
            mandatory = True,
            executable = True,
            cfg = "target",
            doc = "Executable target to deploy and run on the remote board.",
        ),
        "remote_dir": attr.string(
            default = "~/homebot/bin",
            doc = "Directory on the remote board where the binary will be copied.",
        ),
    },
    executable = True,
    doc = "Builds a binary and runs it on a remote board via scp/ssh.",
)
