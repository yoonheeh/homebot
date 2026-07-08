"""Bazel rule to deploy Python scripts and run uv sync on a remote board."""

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
