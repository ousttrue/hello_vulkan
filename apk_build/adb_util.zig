const std = @import("std");
const android = @import("android");

pub fn adbStart(
    b: *std.Build,
    tools: *android.Tools,
    dependStep: *std.Build.Step,
    apk_name: []const u8,
    package_name: []const u8,
    activity_class_name: []const u8,
) *std.Build.Step.Run {
    const adb = b.fmt("{s}/platform-tools/adb", .{tools.android_sdk_path});
    const adb_install = b.addSystemCommand(&.{
        adb,
        "install",
        "-r",
        "-d",
        b.fmt("zig-out/bin/{s}.apk", .{apk_name}),
    });
    adb_install.step.dependOn(dependStep);

    const adb_start = b.addSystemCommand(&.{
        adb,
        "shell",
        "am",
        "start",
        "-a",
        "android.intent.action.MAIN",
        "-n",
        b.fmt("{s}/{s}", .{ package_name, activity_class_name }),
    });
    adb_start.step.dependOn(&adb_install.step);
    return adb_start;
}
