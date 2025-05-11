const std = @import("std");
const ndk_build = @import("apk_build/apk_build.zig");

pub fn build(b: *std.Build) void {
    const optimize = b.standardOptimizeOption(.{});
    const package_name = "com.ousttrue.vulfwk";
    const apk_name = "vulfwk";
    const so_name = b.fmt("lib{s}.so", .{apk_name});
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
    // build apk zip archive
    //
    const validationlayers_dep = b.dependency("vulkan-validationlayers", .{});
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        b.path("samples/vulfwk/android/AndroidManifest.xml"),
        .{ .bin = .{
            .src = so.build_dir.path(b, "samples/vulfwk/libvulfwk.so"),
            .dst = b.fmt("lib/{s}/{s}", .{ abi, so_name }),
        } },
        null,
        &.{.{
            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
        }},
    );
    zip_file.step.dependOn(so.step);

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
    b.step("run", "adb install... && adb shell am start...").dependOn(&adb_start.step);
}
