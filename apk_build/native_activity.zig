const std = @import("std");
const android = @import("android");

pub fn setup(
    b: *std.Build,
    tools: *android.Tools,
    so: *std.Build.Step.Compile,
) void {

    //
    // setup for NDK
    //
    updateSharedLibraryOptions(so);

    // update linked libraries that use C or C++ to:
    // - use Android LibC file
    // - add Android NDK library paths. (libandroid, liblog, etc)
    // updateLinkObjectsRecursive(
    //     b,
    //     tools,
    //     so,
    // );

    // update artifact to:
    // - Be configured to work correctly on Android
    // - To know where C header /lib files are via setLibCFile and linkLibC
    // - Provide path to additional libraries to link to
    setLibCFile(tools, so);
    addLibraryPaths(b, tools, so.root_module, tools.api_level);

    // Apply workaround for Zig 0.14.0 stable
    //
    // This *must* occur after "apk.updateLinkObjects" for the root package otherwise
    // you may get an error like: "unable to find dynamic system library 'c++abi_zig_workaround'"
    // applyLibLinkCppWorkaroundIssue19(b, tools, so);
}

pub fn link_native_app_glue(
    b: *std.Build,
    artifact: *std.Build.Step.Compile,
    tools: *android.Tools,
) void {
    const include = std.Build.LazyPath{
        .cwd_relative = b.fmt("{s}/ndk/{s}/sources/android/native_app_glue", .{
            tools.android_sdk_path, tools.ndk_version,
        }),
    };
    artifact.addIncludePath(include);

    const src = std.Build.LazyPath{
        .cwd_relative = b.fmt("{s}/ndk/{s}/sources/android/native_app_glue/android_native_app_glue.c", .{
            tools.android_sdk_path, tools.ndk_version,
        }),
    };
    artifact.addCSourceFile(.{
        .file = src,
    });
}

fn updateLinkObjectsRecursive(
    b: *std.Build,
    tools: *android.Tools,
    root_artifact: *std.Build.Step.Compile,
) void {
    for (root_artifact.root_module.link_objects.items) |link_object| {
        switch (link_object) {
            .other_step => |artifact| {
                switch (artifact.kind) {
                    .lib => {
                        // If you have a library that is being built as an *.so then install it
                        // alongside your library.

                        // If library is built using C or C++ then setLibCFile
                        if (artifact.root_module.link_libc == true or
                            artifact.root_module.link_libcpp == true)
                        {
                            setLibCFile(tools, artifact);
                        }

                        // Add library paths to find "android", "log", etc
                        addLibraryPaths(
                            b,
                            tools,
                            artifact.root_module,
                            tools.api_level,
                        );

                        // Update libraries linked to this library
                        updateLinkObjectsRecursive(
                            b,
                            tools,
                            artifact,
                        );

                        // Apply workaround for Zig 0.14.0
                        applyLibLinkCppWorkaroundIssue19(b, tools, artifact);
                    },
                    else => continue,
                }
            },
            else => {},
        }
    }
}

/// Hack/Workaround issue #19 where Zig has issues with "linkLibCpp()" in Zig 0.14.0 stable
/// - Zig Android SDK: https://github.com/silbinarywolf/zig-android-sdk/issues/19
/// - Zig: https://github.com/ziglang/zig/issues/23302
///
/// This applies in two cases:
/// - If the artifact has "link_libcpp = true"
/// - If the artifact is linking to another artifact that has "link_libcpp = true", ie. artifact.dependsOnSystemLibrary("c++abi_zig_workaround")
fn applyLibLinkCppWorkaroundIssue19(
    b: *std.Build,
    tools: *android.Tools,
    artifact: *std.Build.Step.Compile,
) void {
    const should_apply_fix = (artifact.root_module.link_libcpp == true or
        artifact.dependsOnSystemLibrary("c++abi_zig_workaround"));
    if (!should_apply_fix) {
        return;
    }

    const system_target = getAndroidTriple(artifact.root_module.resolved_target.?) catch |err| @panic(@errorName(err));
    const lib_path = std.Build.LazyPath{
        .cwd_relative = b.pathJoin(&.{ tools.ndk_sysroot_path, "usr", "lib", system_target, "libc++abi.a" }),
    };
    const libcpp_workaround = b.addWriteFiles();
    const libcppabi_dir = libcpp_workaround.addCopyFile(
        lib_path,
        "libc++abi_zig_workaround.a",
    ).dirname();

    artifact.root_module.addLibraryPath(libcppabi_dir);

    if (artifact.root_module.link_libcpp == true) {
        // NOTE(jae): 2025-04-06
        // Don't explicitly linkLibCpp
        artifact.root_module.link_libcpp = null;

        // NOTE(jae): 2025-04-06 - https://github.com/silbinarywolf/zig-android-sdk/issues/28
        // Resolve issue where in Zig 0.14.0 stable that '__gxx_personality_v0' is missing when starting
        // an SDL2 or SDL3 application.
        //
        // Tested on x86_64 with:
        // - build_tools_version = "35.0.1"
        // - ndk_version = "29.0.13113456"
        artifact.root_module.linkSystemLibrary("c++abi_zig_workaround", .{});

        // NOTE(jae): 2025-04-06
        // unresolved symbol "_Unwind_Resume" error occurs when SDL2 is loaded,
        // so we link the "unwind" library
        artifact.root_module.linkSystemLibrary("unwind", .{});
    }
}

fn getAndroidTriple(target: std.Build.ResolvedTarget) error{InvalidAndroidTarget}![]const u8 {
    if (!target.result.abi.isAndroid()) return error.InvalidAndroidTarget;
    return switch (target.result.cpu.arch) {
        .x86 => "i686-linux-android",
        .x86_64 => "x86_64-linux-android",
        .arm => "arm-linux-androideabi",
        .aarch64 => "aarch64-linux-android",
        .riscv64 => "riscv64-linux-android",
        else => error.InvalidAndroidTarget,
    };
}

fn addLibraryPaths(
    b: *std.Build,
    tools: *android.Tools,
    module: *std.Build.Module,
    api_level: android.APILevel,
) void {
    // const android_ndk_sysroot = apk.tools.ndk_sysroot_path;

    // get target
    const target = module.resolved_target orelse {
        @panic(b.fmt("no 'target' set on Android module", .{}));
    };
    const system_target = getAndroidTriple(target) catch |err| @panic(@errorName(err));

    // NOTE(jae): 2024-09-11
    // These *must* be in order of API version, then architecture, then non-arch specific otherwise
    // when starting an *.so from Android or an emulator you can get an error message like this:
    // - "java.lang.UnsatisfiedLinkError: dlopen failed: TLS symbol "_ZZN8gwp_asan15getThreadLocalsEvE6Locals" in dlopened"
    const android_api_version: u32 = @intFromEnum(api_level);

    // NOTE(jae): 2025-03-09
    // Resolve issue where building SDL2 gets the following error for 'arm-linux-androideabi'
    // SDL2-2.32.2/src/cpuinfo/SDL_cpuinfo.c:93:10: error: 'cpu-features.h' file not found
    //
    // This include is specifically needed for: #if defined(__ANDROID__) && defined(__arm__) && !defined(HAVE_GETAUXVAL)
    //
    // ie. $ANDROID_HOME/ndk/{ndk_version}/sources/android/cpufeatures
    if (target.result.cpu.arch == .arm) {
        module.addIncludePath(.{
            .cwd_relative = b.fmt("{s}/ndk/{s}/sources/android/cpufeatures", .{
                tools.android_sdk_path,
                tools.ndk_version,
            }),
        });
    }

    // ie. $ANDROID_HOME/ndk/{ndk_version}/toolchains/llvm/prebuilt/{host_os_and_arch}/sysroot ++ usr/lib/aarch64-linux-android/35
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/usr/lib/{s}/{d}", .{
        tools.ndk_sysroot_path,
        system_target,
        android_api_version,
    }) });
    // ie. $ANDROID_HOME/ndk/{ndk_version}/toolchains/llvm/prebuilt/{host_os_and_arch}/sysroot ++ /usr/lib/aarch64-linux-android
    module.addLibraryPath(.{ .cwd_relative = b.fmt("{s}/usr/lib/{s}", .{ tools.ndk_sysroot_path, system_target }) });
}

// TODO: Consider making this be setup on "create" and then we just pass in the "android_libc_writefile"
// anytime setLibCFile is called
fn setLibCFile(tools: *android.Tools, compile: *std.Build.Step.Compile) void {
    const b = tools.b;

    const target = compile.root_module.resolved_target orelse @panic("no 'target' set on Android module");
    const system_target = getAndroidTriple(target) catch |err| @panic(@errorName(err));

    const android_libc_path = createLibC(
        b,
        system_target,
        tools.api_level,
        tools.ndk_sysroot_path,
        tools.ndk_version,
    );
    android_libc_path.addStepDependencies(&compile.step);
    compile.setLibCFile(android_libc_path);
}

fn createLibC(
    b: *std.Build,
    system_target: []const u8,
    android_version: android.APILevel,
    ndk_sysroot_path: []const u8,
    ndk_version: []const u8,
) std.Build.LazyPath {
    const libc_file_format =
        \\# Generated by zig-android-sdk. DO NOT EDIT.
        \\
        \\# The directory that contains `stdlib.h`.
        \\# On POSIX-like systems, include directories be found with: `cc -E -Wp,-v -xc /dev/null`
        \\include_dir={[include_dir]s}
        \\
        \\# The system-specific include directory. May be the same as `include_dir`.
        \\# On Windows it's the directory that includes `vcruntime.h`.
        \\# On POSIX it's the directory that includes `sys/errno.h`.
        \\sys_include_dir={[sys_include_dir]s}
        \\
        \\# The directory that contains `crt1.o`.
        \\# On POSIX, can be found with `cc -print-file-name=crt1.o`.
        \\# Not needed when targeting MacOS.
        \\crt_dir={[crt_dir]s}
        \\
        \\# The directory that contains `vcruntime.lib`.
        \\# Only needed when targeting MSVC on Windows.
        \\msvc_lib_dir=
        \\
        \\# The directory that contains `kernel32.lib`.
        \\# Only needed when targeting MSVC on Windows.
        \\kernel32_lib_dir=
        \\
        \\gcc_dir=
    ;

    const include_dir = b.fmt("{s}/usr/include", .{ndk_sysroot_path});
    const sys_include_dir = b.fmt("{s}/usr/include/{s}", .{ ndk_sysroot_path, system_target });
    const crt_dir = b.fmt("{s}/usr/lib/{s}/{d}", .{ ndk_sysroot_path, system_target, @intFromEnum(android_version) });

    const libc_file_contents = b.fmt(libc_file_format, .{
        .include_dir = include_dir,
        .sys_include_dir = sys_include_dir,
        .crt_dir = crt_dir,
    });

    const filename = b.fmt("android-libc_target-{s}_version-{}_ndk-{s}.conf", .{ system_target, @intFromEnum(android_version), ndk_version });

    const write_file = b.addWriteFiles();
    const android_libc_path = write_file.add(filename, libc_file_contents);
    return android_libc_path;
}

fn updateSharedLibraryOptions(artifact: *std.Build.Step.Compile) void {
    if (artifact.linkage) |linkage| {
        if (linkage != .dynamic) {
            @panic("can only call updateSharedLibraryOptions if linkage is dynamic");
        }
    } else {
        @panic("can only call updateSharedLibraryOptions if linkage is dynamic");
    }

    // NOTE(jae): 2024-09-22
    // Need compiler_rt even for C code, for example aarch64 can fail to load on Android when compiling SDL2
    // because it's missing "__aarch64_cas8_acq_rel"
    if (artifact.bundle_compiler_rt == null) {
        artifact.bundle_compiler_rt = true;
    }

    if (artifact.root_module.optimize) |optimize| {
        // NOTE(jae): ZigAndroidTemplate used: (optimize == .ReleaseSmall);
        artifact.root_module.strip = optimize == .ReleaseSmall;
    }

    // TODO(jae): 2024-09-19 - Copy-pasted from https://github.com/ikskuh/ZigAndroidTemplate/blob/master/Sdk.zig
    // Remove when https://github.com/ziglang/zig/issues/7935 is resolved.
    if (artifact.root_module.resolved_target) |target| {
        if (target.result.cpu.arch == .x86) {
            const use_link_z_notext_workaround: bool = if (artifact.bundle_compiler_rt) |bcr| bcr else false;
            if (use_link_z_notext_workaround) {
                // NOTE(jae): 2024-09-22
                // This workaround can prevent your libmain.so from loading. At least in my testing with running Android 10 (Q, API Level 29)
                //
                // This is due to:
                // "Text Relocations Enforced for API Level 23"
                // See: https://android.googlesource.com/platform/bionic/+/refs/tags/ndk-r14/android-changes-for-ndk-developers.md
                artifact.link_z_notext = true;
            }
        }
    }

    // NOTE(jae): 2024-09-01
    // Copy-pasted from https://github.com/ikskuh/ZigAndroidTemplate/blob/master/Sdk.zig
    // Do we need all these?
    //
    // NOTE(jae): 2025-03-10
    // Seemingly not "needed" anymore, at least for x86_64 Android builds
    // Keeping this code here incase I want to backport to Zig 0.13.0 and it's needed.

    if (artifact.root_module.pic == null) {
        artifact.root_module.pic = true;
    }
    artifact.link_emit_relocs = true; // Retains all relocations in the executable file. This results in larger executable files
    artifact.link_eh_frame_hdr = true;
    artifact.link_function_sections = true;
    // Seemingly not "needed" anymore, at least for x86_64 Android builds
    artifact.export_table = true;
}
