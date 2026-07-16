"""Bazel rule to build a binary and deploy it on a remote board via ssh/scp."""

def _deploy_impl(ctx):
    binary = ctx.attr.binary
    binary_default = binary[DefaultInfo]
    binary_executable = binary_default.files_to_run.executable

    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    # The binary's Bazel package/name, used to locate it inside runfiles.
    binary_package = binary.label.package
    binary_name = binary.label.name

    # Get the paths of all data files
    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    script_content = """#!/usr/bin/env bash
set -euo pipefail

REMOTE_DIR="{remote_dir}"
BINARY_PACKAGE="{binary_package}"
BINARY_NAME="{binary_name}"
DATA_FILES="{data_files_str}"
echo $DATA_FILES

# Locate runfiles root.
if [[ -n "${{RUNFILES_DIR:-}}" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${{BASH_SOURCE[0]}}.runfiles" ]]; then
    RUNFILES="${{BASH_SOURCE[0]}}.runfiles"
else
    echo "ERROR: cannot find Bazel runfiles directory" >&2
    exit 1
fi
echo "runfiles: $RUNFILES"

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
scp "$BINARY" "${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_BIN}} && chmod +x ${{REMOTE_BIN}}"
echo $BINARY

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

#echo "== Running on board =="
#ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "chmod +x ${{REMOTE_BIN}} && ${{REMOTE_BIN}} \"$@\""
""".format(
        remote_dir = ctx.attr.remote_dir,
        binary_package = binary_package,
        binary_name = binary_name,
        data_files_str = data_files_str,
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

deploy = rule(
    implementation = _deploy_impl,
    attrs = {
        "binary": attr.label(
            mandatory = True,
            executable = True,
            cfg = "target",
            doc = "Executable target to deploy on the remote board.",
        ),
        "data": attr.label_list(allow_files = True, doc = "Files to deploy alongside the binary"),
        "remote_dir": attr.string(
            default = "~/homebot/bin",
            doc = "Directory on the remote board where the binary will be copied.",
        ),
    },
    executable = True,
    doc = "Builds a binary and deploys it on a remote board via scp/ssh.",
)

def _deploy_uv_impl(ctx):
    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    # Gather the relative paths of all source/config files
    src_paths = [f.short_path for f in ctx.files.srcs]
    files_str = " ".join(["'{}'".format(p) for p in src_paths])

    script_content = """#!/usr/bin/env bash
set -euo pipefail

REMOTE_DIR="{remote_dir}"

# Locate runfiles root
if [[ -n "${{RUNFILES_DIR:-}}" ]]; then
    RUNFILES="$RUNFILES_DIR"
elif [[ -d "${{BASH_SOURCE[0]}}.runfiles" ]]; then
    RUNFILES="${{BASH_SOURCE[0]}}.runfiles"
else
    echo "ERROR: cannot find Bazel runfiles directory" >&2
    exit 1
fi

# Parse board destination
if [[ $# -ge 1 && "$1" == *@* ]]; then
    BOARD_USER="${{1%%@*}}"
    BOARD_HOST="${{1#*@}}"
    shift
else
    if [[ -z "${{BOARD_USER:-}}" || -z "${{BOARD_HOST:-}}" ]]; then
        echo "Usage: $0 <user>@<host> [remote_args...]" >&2
        exit 1
    fi
fi

# Move into the runfiles workspace directory so tar paths are relative
WORKSPACE_DIR="${{RUNFILES}}/{workspace_name}"
cd "$WORKSPACE_DIR"

# 1. Package the Python files into a temporary tarball
TARBALL=$(mktemp /tmp/deploy_uv_payload_XXXXXX.tar.gz)
echo "== Packaging Python payload =="
tar -czhf "$TARBALL" {files}

# 2. Prepare remote directory
echo "== Ensuring ${{REMOTE_DIR}} exists on board =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "mkdir -p ${{REMOTE_DIR}}"

# 3. SCP the tarball
echo "== Deploying payload to ${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_DIR}} =="
scp "$TARBALL" "${{BOARD_USER}}@${{BOARD_HOST}}:${{REMOTE_DIR}}/payload.tar.gz"

# 4. Extract and run 'uv sync' remotely
echo "== Syncing uv environment on board =="
ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "export PATH=\"\\$PATH:\\$HOME/.local/bin\" && cd ${{REMOTE_DIR}} && tar -xzf payload.tar.gz && uv sync"

# Clean up local tarball
rm "$TARBALL"

echo "== Deployment and Sync Complete! =="
# Uncomment to automatically run the main script
# ssh "${{BOARD_USER}}@${{BOARD_HOST}}" -- "cd ${{REMOTE_DIR}} && uv run {main_script}"

""".format(
        remote_dir = ctx.attr.remote_dir,
        workspace_name = ctx.workspace_name,
        files = files_str,
        main_script = ctx.attr.main_script,
    )

    ctx.actions.write(
        output = script,
        content = script_content,
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = ctx.files.srcs)

    return [DefaultInfo(
        executable = script,
        runfiles = runfiles,
    )]

deploy_uv = rule(
    implementation = _deploy_uv_impl,
    attrs = {
        "srcs": attr.label_list(
            allow_files = True,
            mandatory = True,
            doc = "Python files, pyproject.toml, and uv.lock to deploy.",
        ),
        "main_script": attr.string(
            mandatory = False,
            default = "main.py",
            doc = "The main entrypoint script to run on the board.",
        ),
        "remote_dir": attr.string(
            default = "~/homebot/python_app",
            doc = "Directory on the remote board.",
        ),
    },
    executable = True,
    doc = "Packages Python files, deploys via scp, and runs 'uv sync' remotely.",
)
