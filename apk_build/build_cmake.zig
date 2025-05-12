const std = @import("std");
const android = @import("android");

pub const ToolchainOptions = struct {
    abi: []const u8 = "arm64-v8a",
    platform: []const u8 = "android-30",
};

pub const CmakeAndroidToolchainStep = struct {
    step: *std.Build.Step,
    build_dir: std.Build.LazyPath,
};

pub fn cmakeAndroidToolchain(
    b: *std.Build,
    optimize: std.builtin.OptimizeMode,
    source_dir: std.Build.LazyPath,
    tools: *android.Tools,
    options: ToolchainOptions,
) CmakeAndroidToolchainStep {
    const cmake_configure = b.addSystemCommand(&.{
        "cmake",
        // "--fresh",
        "-G",
        "Ninja",
        b.fmt("-DCMAKE_BUILD_TYPE={s}", .{if (optimize == .Debug) "Debug" else "Release"}),
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=1",
    });
    cmake_configure.setName("cmake configure");
    // -S source
    cmake_configure.addArg("-S");
    cmake_configure.addDirectoryArg(source_dir);
    // -B build
    cmake_configure.addArg("-B");
    const build_dir = cmake_configure.addOutputDirectoryArg("build");

    // ndk toolchain
    cmake_configure.addArgs(
        &.{
            b.fmt("-DANDROID_ABI={s}", .{options.abi}),
            b.fmt("-DANDROID_PLATFORM={s}", .{options.platform}),
            b.fmt("-DANDROID_NDK={s}/ndk/{s}", .{
                tools.android_sdk_path,
                tools.ndk_version,
            }),
            b.fmt("-DCMAKE_TOOLCHAIN_FILE={s}/ndk/{s}/build/cmake/android.toolchain.cmake", .{
                tools.android_sdk_path,
                tools.ndk_version,
            }),
        },
    );

    const cmake_build = b.addSystemCommand(&.{
        "cmake",
    });
    cmake_build.setName("cmake build");
    cmake_build.step.dependOn(&cmake_configure.step);
    // --build build
    cmake_build.addArg("--build");
    cmake_build.addDirectoryArg(build_dir);
    cmake_build.addArgs(&.{
        "--config",
        if (optimize == .Debug) "Debug" else "Release",
        // "--clean-first",
    });

    return .{
        .step = &cmake_build.step,
        .build_dir = build_dir,
    };
}
