"""Bazel rule to run remote hardware test."""

def _remote_hardware_check_impl(ctx):
    test_script = ctx.file.test_script
    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    ctx.actions.expand_template(
        template = ctx.file._template,
        output = script,
        substitutions = {
            "@@REMOTE_DIR@@": ctx.attr.remote_dir,
            "@@TARGET_NAME@@": test_script.basename,
            "@@DATA_FILES@@": data_files_str,
            "@@RUN_CMD@@": "bash -lc 'uv run {} $@'".format(test_script.basename),
        },
        is_executable = True,
    )

    runfiles = ctx.runfiles(files = [test_script])
    runfiles = ctx.runfiles(files = [test_script] + ctx.files.data)

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
        "data": attr.label_list(allow_files = True, doc = "Files to deploy alongside the binary"),
        "_template": attr.label(
            default = Label("//scripts:deploy_runner.sh.tpl"),
            allow_single_file = True,
        ),
    },
    executable = True,
)

def _remote_cc_check_impl(ctx):
    binary = ctx.attr.binary
    binary_default = binary[DefaultInfo]
    binary_executable = binary_default.files_to_run.executable
    binary_name = binary.label.name

    # Get the paths of all data files
    data_files = [f.short_path for f in ctx.files.data]
    data_files_str = " ".join(data_files)

    script = ctx.actions.declare_file(ctx.attr.name + "_runner.sh")

    ctx.actions.expand_template(
        template = ctx.file._template,
        output = script,
        substitutions = {
            "@@REMOTE_DIR@@": ctx.attr.remote_dir,
            "@@TARGET_NAME@@": binary_name,
            "@@DATA_FILES@@": data_files_str,
            "@@RUN_CMD@@": "./{} $@".format(binary_name),
        },
        is_executable = True,
    )

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
        "_template": attr.label(
            default = Label("//scripts:deploy_runner.sh.tpl"),
            allow_single_file = True,
        ),
    },
    executable = True,
)
