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
        .{
            .package_name = "corg.khronos.openxr.hello_xr",
            .apk_name = "hello_xr",
            .contents = .{
                .android_manifest = b.path("samples/hello_xr/AndroidManifest.xml"),
                .resource_directory = b.path("samples/hello_xr/res"),
                .appends = &.{
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "samples/hello_xr/libhello_xr.so"),
                            .dst = "lib/arm64-v8a/libhello_xr.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "_deps/openxr-build/src/loader/libopenxr_loader.so"),
                            .dst = "lib/arm64-v8a/libopenxr_loader.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "_deps/glslang-build/SPIRV/libSPIRV.so"),
                            .dst = "lib/arm64-v8a/libSPIRV.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "_deps/glslang-build/glslang/libglslang.so"),
                            .dst = "lib/arm64-v8a/libglslang.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "_deps/spirv-tools-build/source/libSPIRV-Tools-shared.so"),
                            .dst = "lib/arm64-v8a/libSPIRV-Tools-shared.so",
                        },
                    },
                },
            },
        },
    );

    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "ousttrue.simplehello",
            .activity_class_name = "ousttrue.simplehello.MainActivity",
            .apk_name = "SimpleHello",
            .contents = .{
                .android_manifest = b.path("samples/simple_activity/AndroidManifest.xml"),
                .resource_directory = b.path("samples/simple_activity/res"),
                .java_files = &.{b.path("samples/simple_activity/java/ousttrue/simplehello/MainActivity.java")},
            },
        },
    );

    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "com.ousttrue.vko_01_clear",
            .apk_name = "vko_01_clear",
            .contents = .{
                .android_manifest = b.path("samples/vuloxr/01_clear/AndroidManifest.xml"),
                // .assets_directory = shaders_wf.getDirectory(),
                .appends = &.{
                    .{ .path = .{
                        .src = so.build_dir.path(b, "samples/vuloxr/lib01_clear.so"),
                        .dst = "lib/arm64-v8a/lib01_clear.so",
                    } },
                    .{
                        .path = .{
                            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
                        },
                    },
                },
            },
        },
    );
    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "com.ousttrue.vko02_triangle",
            .apk_name = "vko02_triangle",
            .contents = .{
                .android_manifest = b.path("samples/vko/02_triangle/AndroidManifest.xml"),
                // .assets_directory = shaders_wf.getDirectory(),
                .appends = &.{
                    .{ .path = .{
                        .src = so.build_dir.path(b, "samples/vko/lib02_triangle.so"),
                        .dst = "lib/arm64-v8a/lib02_triangle.so",
                    } },
                    .{
                        .path = .{
                            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
                        },
                    },
                },
            },
        },
    );
    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "com.arm.vulkansdk.hellotriangle",
            .apk_name = "hellotriangle",
            .contents = .{
                .android_manifest = b.path("samples/vuloxr/02_hellotriangle/AndroidManifest.xml"),
                .resource_directory = b.path("samples/vuloxr/02_hellotriangle/res"),
                // .assets_directory = b.path("samples/hellotriangle/android/assets"),
                .appends = &.{
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "samples/vuloxr/lib02_hellotriangle.so"),
                            .dst = "lib/arm64-v8a/lib02_hellotriangle.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
                        },
                    },
                },
            },
        },
    );

    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "com.ousttrue.vko03_texture",
            .apk_name = "vko03_texture",
            .contents = .{
                .android_manifest = b.path("samples/vko/03_texture/AndroidManifest.xml"),
                // .assets_directory = shaders_wf.getDirectory(),
                .appends = &.{
                    .{ .path = .{
                        .src = so.build_dir.path(b, "samples/vko/lib03_texture.so"),
                        .dst = "lib/arm64-v8a/lib03_texture.so",
                    } },
                    .{
                        .path = .{
                            .src = validationlayers_dep.path("arm64-v8a/libVkLayer_khronos_validation.so"),
                            .dst = "lib/arm64-v8a/libVkLayer_khronos_validation.so",
                        },
                    },
                },
            },
        },
    );

    // E:\repos\github.com\ousttrue\hello_vulkan\samples\android_openxr_gles\gl2triOXR
    build_apk(
        b,
        tools,
        so,
        .{
            .package_name = "com.example.gl2triOXR",
            .apk_name = "gl2triOXR",
            .contents = .{
                .android_manifest = b.path("samples/android_openxr_gles/gl2triOXR/AndroidManifest.xml"),
                // .assets_directory = shaders_wf.getDirectory(),
                .appends = &.{
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "samples/android_openxr_gles/gl2triOXR/libgl2triOXR.so"),
                            .dst = "lib/arm64-v8a/libgl2triOXR.so",
                        },
                    },
                    .{
                        .path = .{
                            .src = so.build_dir.path(b, "_deps/openxr-build/src/loader/libopenxr_loader.so"),
                            .dst = "lib/arm64-v8a/libopenxr_loader.so",
                        },
                    },
                },
            },
        },
    );
}

const ApkOpts = struct {
    package_name: []const u8,
    activity_class_name: []const u8 = "android.app.NativeActivity",
    apk_name: []const u8,
    contents: ndk_build.ApkContents,
};

fn build_apk(
    b: *std.Build,
    tools: *android.Tools,
    so: ndk_build.CmakeAndroidToolchainStep,
    opts: ApkOpts,
) void {

    //
    // build apk zip archive
    //
    const zip_file = ndk_build.makeZipfile(
        b,
        tools,
        so.step,
        opts.package_name,
        opts.contents,
    );

    // zipalign
    const aligned_apk = ndk_build.runZipalign(
        b,
        tools,
        zip_file.file,
        opts.apk_name,
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
        b.fmt("{s}.apk", .{opts.apk_name}),
    );
    b.step(opts.apk_name, b.fmt("build {s}", .{opts.apk_name})).dependOn(&install_apk.step);
    b.getInstallStep().dependOn(&install_apk.step);

    //
    // zig build run => adb install && adb shell am start
    //
    const adb_start = ndk_build.adbStart(
        b,
        tools,
        &install_apk.step,
        opts.apk_name,
        opts.package_name,
        opts.activity_class_name,
    );
    b.step(b.fmt("run_{s}", .{opts.apk_name}), "adb install... && adb shell am start...").dependOn(&adb_start.step);
}
