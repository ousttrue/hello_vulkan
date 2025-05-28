const std = @import("std");
const android = @import("android");

// Make resources.apk from:
// - resources.flat.zip (created from "aapt2 compile")
//    - res/values/strings.xml -> values_strings.arsc.flat
// - AndroidManifest.xml
// - assets
//
// This also validates your AndroidManifest.xml and can catch configuration errors
// which "aapt" was not capable of.
// See: https://developer.android.com/tools/aapt2#aapt2_element_hierarchy
// Snapshot: http://web.archive.org/web/20241001070128/https://developer.android.com/tools/aapt2#aapt2_element_hierarchy
pub fn link(
    b: *std.Build,
    tools: *android.Tools,
    android_manifest: std.Build.LazyPath,
    resource_directory: ?std.Build.LazyPath,
    assets_directory: ?std.Build.LazyPath,
) struct {
    step: *std.Build.Step.Run,
    file: std.Build.LazyPath,
} {
    const aapt2link = b.addSystemCommand(&[_][]const u8{
        tools.build_tools.aapt2,
        "link",
        "-I", // add an existing package to base include set
        tools.root_jar,
    });
    aapt2link.setName("aapt2 link");
    if (b.verbose) {
        aapt2link.addArg("-v");
    }
    aapt2link.addArgs(&[_][]const u8{
        "--target-sdk-version",
        b.fmt("{d}", .{@intFromEnum(tools.api_level)}),
    });

    // manifest
    aapt2link.addArg("--manifest");
    aapt2link.addFileArg(android_manifest);

    // resources
    if (make_resources_flat_zip(b, tools, resource_directory)) |file| {
        aapt2link.addFileArg(file);
    }

    // assets
    if (assets_directory) |dir| {
        aapt2link.addArg("-A");
        aapt2link.addDirectoryArg(dir);
    }

    aapt2link.addArg("-o");
    const resources_apk = aapt2link.addOutputFileArg("resources.apk");
    return .{
        .step = aapt2link,
        .file = resources_apk,
    };
}

fn make_resources_flat_zip(
    b: *std.Build,
    tools: *android.Tools,
    resource_directory: ?std.Build.LazyPath,
) ?std.Build.LazyPath {
    // Add resource files
    if (resource_directory) |dir| {
        // Make zip of compiled resource files, ie.
        // - res/values/strings.xml -> values_strings.arsc.flat
        // - mipmap/ic_launcher.png -> mipmap-ic_launcher.png.flat

        const aapt2compile = b.addSystemCommand(&[_][]const u8{
            tools.build_tools.aapt2,
            "compile",
        });
        aapt2compile.setName("aapt2 compile [dir]");

        // add directory
        aapt2compile.addArg("--dir");
        aapt2compile.addDirectoryArg(dir);

        aapt2compile.addArg("-o");
        const resources_flat_zip_file = aapt2compile.addOutputFileArg("resource_dir.flat.zip");

        return resources_flat_zip_file;
    } else {
        return null;
    }
}
