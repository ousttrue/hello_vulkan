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

    build_vulfwk_apk(b, tools, abi, so);
    build_hellotriangle_apk(b, tools, abi, so);
    build_helloxr_apk(b, tools, abi, so);
}

fn build_helloxr_apk(
    b: *std.Build,
    tools: *android.Tools,
    abi: []const u8,
    so: ndk_build.CmakeAndroidToolchainStep,
) void {
    const package_name = "corg.khronos.openxr.hello_xr";
    const apk_name = "hello_xr";
    const so_name = "libhello_xr.so";

    //
    // build apk zip archive
    //
    const validationlayers_dep = b.dependency("vulkan-validationlayers", .{});
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        so.step,
        b.path("samples/hello_xr/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/hello_xr/cpp/libhello_xr.so"),
            .dst = b.fmt("lib/{s}/{s}", .{ abi, so_name }),
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
    b.step("helloxr", "adb install... && adb shell am start...").dependOn(&adb_start.step);
}

fn build_hellotriangle_apk(
    b: *std.Build,
    tools: *android.Tools,
    abi: []const u8,
    so: ndk_build.CmakeAndroidToolchainStep,
) void {
    const package_name = "com.arm.vulkansdk.hellotriangle";
    const apk_name = "hellotriangle";
    const so_name = "libnative.so";

    //
    // build apk zip archive
    //
    const validationlayers_dep = b.dependency("vulkan-validationlayers", .{});
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        so.step,
        b.path("samples/hellotriangle/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/hellotriangle/cpp/libnative.so"),
            .dst = b.fmt("lib/{s}/{s}", .{ abi, so_name }),
        } },
        b.path("samples/hellotriangle/android/res"),
        b.path("samples/hellotriangle/android/assets"),
        &.{.{
            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
        }},
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
    b.step("hellotriangle", "adb install... && adb shell am start...").dependOn(&adb_start.step);
}

fn build_vulfwk_apk(
    b: *std.Build,
    tools: *android.Tools,
    abi: []const u8,
    so: ndk_build.CmakeAndroidToolchainStep,
) void {
    const package_name = "com.ousttrue.vulfwk";
    const apk_name = "vulfwk";
    const so_name = b.fmt("lib{s}.so", .{apk_name});
    const shaders_wf = b.addWriteFiles();
    shaders_wf.step.dependOn(so.step);
    _ = shaders_wf.addCopyFile(so.build_dir.path(b, "samples/vulfwk/shader.vert.spv"), "shader.vert.spv");
    _ = shaders_wf.addCopyFile(so.build_dir.path(b, "samples/vulfwk/shader.frag.spv"), "shader.frag.spv");

    //
    // build apk zip archive
    //
    const validationlayers_dep = b.dependency("vulkan-validationlayers", .{});
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        &shaders_wf.step,
        b.path("samples/vulfwk/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/vulfwk/libvulfwk.so"),
            .dst = b.fmt("lib/{s}/{s}", .{ abi, so_name }),
        } },
        null,
        shaders_wf.getDirectory(),
        &.{.{
            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
        }},
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
    b.step("vulfwk", "adb install... && adb shell am start...").dependOn(&adb_start.step);
}
