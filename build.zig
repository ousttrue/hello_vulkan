const std = @import("std");
const ndk_build = @import("apk_build/apk_build.zig");
const android = @import("android");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const abi = "arm64-v8a";

    const tools = ndk_build.createAndroidTools(b, .{
        .api_level = .android15,
        .build_tools_version = "35.0.1",
        .ndk_version = "29.0.13113456",
    });

    //
    // run cmake
    //
    const so = ndk_build.cmakeAndroidToolchain(
        b,
        optimize,
        b.path(""),
        tools,
        .{
            .abi = abi,
            .platform = "android-30",
        },
    );

    //
    // apk
    //
    const validationlayers_dep = b.dependency("vulkan-validationlayers", .{});
    build_apk(
        b,
        tools,
        so,
        "corg.khronos.openxr.hello_xr",
        "hello_xr",
        b.path("samples/hello_xr/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/hello_xr/cpp/libhello_xr.so"),
            .dst = "lib/arm64-v8a/libhello_xr.so",
        } },
        b.path("samples/hello_xr/android/res"),
        null,
        &.{
            .{
                .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
            },
            .{
                .src = so.build_dir.path(b, "_deps/openxr-build/src/loader/libopenxr_loader.so"),
                .dst = "lib/arm64-v8a/libopenxr_loader.so",
            },
        },
    );
    build_apk(
        b,
        tools,
        so,
        "com.arm.vulkansdk.hellotriangle",
        "hellotriangle",
        b.path("samples/hellotriangle/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/hellotriangle/cpp/libnative.so"),
            .dst = "lib/arm64-v8a/libnative.so",
        } },
        b.path("samples/hellotriangle/android/res"),
        b.path("samples/hellotriangle/android/assets"),
        &.{.{
            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
        }},
    );
    const shaders_wf = b.addWriteFiles();
    shaders_wf.step.dependOn(so.step);
    _ = shaders_wf.addCopyFile(so.build_dir.path(b, "samples/vulfwk/shader.vert.spv"), "shader.vert.spv");
    _ = shaders_wf.addCopyFile(so.build_dir.path(b, "samples/vulfwk/shader.frag.spv"), "shader.frag.spv");
    build_apk(
        b,
        tools,
        so,
        "com.ousttrue.vulfwk",
        "vulfwk",
        b.path("samples/vulfwk/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/vulfwk/libvulfwk.so"),
            .dst = "lib/arm64-v8a/libvulfwk.so",
        } },
        null,
        shaders_wf.getDirectory(),
        &.{.{
            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
        }},
    );
}

fn build_apk(
    b: *std.Build,
    tools: *android.Tools,
    so: ndk_build.CmakeAndroidToolchainStep,
    package_name: []const u8,
    apk_name: []const u8,
    android_manifest: std.Build.LazyPath,
    entry_point: ndk_build.EntryPoint,
    resource_directory: ?std.Build.LazyPath,
    assets_directory: ?std.Build.LazyPath,
    appends: ?[]const ndk_build.CopyFile,
) void {

    //
    // build apk zip archive
    //
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        so.step,
        android_manifest,
        entry_point,
        resource_directory,
        assets_directory,
        appends,
    );

    // zipalign
    const aligned_apk = ndk_build.runZipalign(
        b,
        tools,
        zip_file.file,
        apk_name,
    );
    aligned_apk.step.dependOn(zip_file.step);

    // apksigner
    const apk = ndk_build.runApksigner(
        b,
        tools,
        aligned_apk.file,
    );

    // install to zig-out
    const install_apk = b.addInstallBinFile(
        apk,
        b.fmt("{s}.apk", .{apk_name}),
    );
    b.step(apk_name, b.fmt("build {s}", .{apk_name})).dependOn(&install_apk.step);
    b.getInstallStep().dependOn(&install_apk.step);

    //
    // zig build run => adb install && adb shell am start
    //
    const adb_start = ndk_build.adbStart(
        b,
        tools,
        &install_apk.step,
        apk_name,
        package_name,
    );
    b.step(b.fmt("run_{s}", .{apk_name}), "adb install... && adb shell am start...").dependOn(&adb_start.step);
}
