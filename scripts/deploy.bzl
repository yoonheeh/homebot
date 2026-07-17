"""Bazel rule to build a binary and deploy it on a remote board via ssh/scp."""

def _deploy_impl(ctx):
    binary = ctx.attr.binary
    binary_default = binary[DefaultInfo]
    binary_executable = binary_default.files_to_run.executable

    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    binary_name = binary.label.name
    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    # Use expand_template instead of write
    ctx.actions.expand_template(
        template = ctx.file._template,
        output = script,
        substitutions = {
            "@@REMOTE_DIR@@": ctx.attr.remote_dir,
            "@@TARGET_NAME@@": binary_name,
            "@@DATA_FILES@@": data_files_str,
            "@@RUN_CMD@@": "",
        },
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
        "_template": attr.label(
            default = Label("//scripts:deploy_runner.sh.tpl"),
            allow_single_file = True,
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
    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    # Use expand_template instead of write
    ctx.actions.expand_template(
        template = ctx.file._template,
        output = script,
        substitutions = {
            "@@REMOTE_DIR@@": ctx.attr.remote_dir,
            "@@TARGET_NAME@@": script.basename,
            "@@DATA_FILES@@": data_files_str,
            "@@RUN_CMD@@": "",
        },
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = ctx.files.srcs + ctx.files.data)

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
        "data": attr.label_list(allow_files = True, doc = "Files to deploy alongside the binary"),
        "remote_dir": attr.string(
            default = "~/homebot/python_app",
            doc = "Directory on the remote board.",
        ),
    },
    executable = True,
    doc = "Packages Python files, deploys via scp, and runs 'uv sync' remotely.",
)
