const std = @import("std");
pub const android = @import("android");
pub const native_activity = @import("native_activity.zig");
pub const apk_builder = @import("apk_builder.zig");
pub const build_cmake = @import("build_cmake.zig");
pub const adb_util = @import("adb_util.zig");

pub const link_native_app_glue = native_activity.link_native_app_glue;

pub const createAndroidTools = android.Tools.create;
pub const setup = native_activity.setup;

pub const makeZipfile = apk_builder.makeZipfile;
pub const runZipalign = apk_builder.runZipalign;
pub const runApksigner = apk_builder.runApksigner;

pub const cmakeAndroidToolchain = build_cmake.cmakeAndroidToolchain;
pub const CmakeAndroidToolchainStep = build_cmake.CmakeAndroidToolchainStep;

pub const adbStart = adb_util.adbStart;

pub fn build(_: *std.Build) void {}
