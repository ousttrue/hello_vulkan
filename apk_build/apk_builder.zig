const std = @import("std");
const android = @import("android");

// We could also use that information to create easy to use Zig step like
// - zig build adb-uninstall (adb uninstall "com.zig.sdl2")
// - zig build adb-logcat
//    - Works if process isn't running anymore/crashed: Powershell: adb logcat | Select-String com.zig.sdl2:
//    - Only works if process is running: adb logcat --pid=`adb shell pidof -s com.zig.sdl2`
const ApkBuilder = struct {
    // These are files that belong in root like:
    // - lib/x86_64/libmain.so
    // - lib/x86_64/libSDL2.so
    // - lib/x86/libmain.so
    // - classes.dex
    files: *std.Build.Step.WriteFile,

    // These files belong in root and *must not* be compressed
    // - resources.arsc
    files_not_compressed: *std.Build.Step.WriteFile,

    pub fn create(
        b: *std.Build,
        tools: *android.Tools,
        dependOn: *std.Build.Step,
        android_manifest_file: std.Build.LazyPath,
        resource_directory: ?std.Build.LazyPath,
        assets_directory: ?std.Build.LazyPath,
    ) @This() {
        const self = @This(){
            .files = b.addWriteFiles(),
            .files_not_compressed = b.addWriteFiles(),
        };
        self.files.step.dependOn(dependOn);

        self.run_jar(
            b,
            tools,
            android_manifest_file,
            resource_directory,
            assets_directory,
        );

        return self;
    }

    // Extract compiled resources.apk and add contents to the folder we'll zip with "jar" below
    // See: https://musteresel.github.io/posts/2019/07/build-android-app-bundle-on-command-line.html
    fn run_jar(
        self: *const @This(),
        b: *std.Build,
        tools: *android.Tools,
        android_manifest_file: std.Build.LazyPath,
        resource_directory: ?std.Build.LazyPath,
        assets_directory: ?std.Build.LazyPath,
    ) void {
        const jar = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar.setName("jar (unzip resources.apk)");
        if (b.verbose) {
            jar.addArg("--verbose");
        }

        // Extract *.apk file created with "aapt2 link"
        jar.addArg("--extract");

        const resources_apk = aapt2_link(
            b,
            tools,
            android_manifest_file,
            resource_directory,
            assets_directory,
        );
        jar.addPrefixedFileArg("--file=", resources_apk.file);

        // NOTE(jae): 2024-09-30
        // Extract to directory of resources_apk and force add that to the overall apk files.
        // This currently has an issue where because we can't use "addOutputDirectoryArg" this
        // step will always be executed.
        const extracted_apk_dir = resources_apk.file.dirname();
        jar.setCwd(extracted_apk_dir);

        _ = self.files.addCopyDirectory(extracted_apk_dir, "", .{
            .exclude_extensions = &.{
                // ignore the *.apk that exists in this directory
                ".apk",
                // ignore resources.arsc as Android 30+ APIs does not supporting
                // compressing this in the zip file
                ".arsc",
            },
        });
        self.files.step.dependOn(&jar.step);

        // Setup directory of additional files that should not be compressed

        // NOTE(jae): 2025-03-23 - https://github.com/silbinarywolf/zig-android-sdk/issues/23
        // We apply resources.arsc seperately to the zip file to avoid compressing it, otherwise we get the following
        // error when we "adb install"
        // - "Targeting R+ (version 30 and above) requires the resources.arsc of installed APKs to be stored uncompressed and aligned on a 4-byte boundary"
        _ = self.files_not_compressed.addCopyFile(extracted_apk_dir.path(b, "resources.arsc"), "resources.arsc");
        self.files_not_compressed.step.dependOn(&jar.step);
    }

    pub fn copy(
        self: *const @This(),
        src: std.Build.LazyPath,
        dst: []const u8,
    ) void {
        _ = self.files.addCopyFile(
            src,
            dst,
        );
    }

    pub fn copyRecursive(
        self: *const @This(),
        b: *std.Build,
        artifact: *std.Build.Step.Compile,
    ) void {
        // If you have a library that is being built as an *.so then install it
        // alongside your library.
        //
        // This was initially added to support building SDL2 with Zig.
        if (artifact.linkage) |linkage| {
            if (linkage == .dynamic) {
                _ = self.files.addCopyFile(
                    artifact.getEmittedBin(),
                    b.fmt(
                        "lib/{s}/lib{s}.so",
                        .{ getSoDir(artifact), artifact.name },
                    ),
                );
            }
        }

        for (artifact.root_module.link_objects.items) |link_object| {
            switch (link_object) {
                .other_step => |child_artifact| {
                    switch (child_artifact.kind) {
                        .lib => {
                            // Update libraries linked to this library
                            self.copyRecursive(
                                b,
                                child_artifact,
                            );
                        },
                        else => continue,
                    }
                },
                else => {},
            }
        }
    }
};

fn getSoDir(artifact: *std.Build.Step.Compile) []const u8 {
    return switch (artifact.rootModuleTarget().cpu.arch) {
        .aarch64 => "arm64-v8a",
        .arm => "armeabi-v7a",
        .x86_64 => "x86_64",
        .x86 => "x86",
        else => std.debug.panic("unsupported or unhandled arch: {s}", .{@tagName(artifact.rootModuleTarget().cpu.arch)}),
    };
}

// Make resources.apk from:
// - resources.flat.zip (created from "aapt2 compile")
//    - res/values/strings.xml -> values_strings.arsc.flat
// - AndroidManifest.xml
//
// This also validates your AndroidManifest.xml and can catch configuration errors
// which "aapt" was not capable of.
// See: https://developer.android.com/tools/aapt2#aapt2_element_hierarchy
// Snapshot: http://web.archive.org/web/20241001070128/https://developer.android.com/tools/aapt2#aapt2_element_hierarchy
fn aapt2_link(
    b: *std.Build,
    tools: *android.Tools,
    android_manifest_file: std.Build.LazyPath,
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

    // full path to AndroidManifest.xml to include in APK
    // ie. --manifest AndroidManifest.xml
    aapt2link.addArg("--manifest");
    aapt2link.addFileArg(android_manifest_file);

    aapt2link.addArgs(&[_][]const u8{
        "--target-sdk-version",
        b.fmt("{d}", .{@intFromEnum(tools.api_level)}),
    });

    // NOTE(jae): 2024-10-02
    // Explored just outputting to dir but it gets errors like:
    //  - error: failed to write res/mipmap-mdpi-v4/ic_launcher.png to archive:
    //      The system cannot find the file specified. (2).
    //
    // So... I'll stick with the creating an APK and extracting it approach.
    // aapt2link.addArg("---to-dir"); // Requires: Android SDK Build Tools 28.0.0 or higher
    // aapt2link.addArg("-o");
    // const resources_apk_dir = aapt2link.addOutputDirectoryArg("resources");

    aapt2link.addArg("-o");
    const resources_apk = aapt2link.addOutputFileArg("resources.apk");

    // Add resource files
    if (resource_directory) |dir| {
        const resources_flat_zip = resblk: {
            // Make zip of compiled resource files, ie.
            // - res/values/strings.xml -> values_strings.arsc.flat
            // - mipmap/ic_launcher.png -> mipmap-ic_launcher.png.flat

            {
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

                break :resblk resources_flat_zip_file;
            }
        };

        // Add resources.flat.zip
        aapt2link.addFileArg(resources_flat_zip);
    }

    if (assets_directory) |dir| {
        aapt2link.addArg("-A");
        aapt2link.addDirectoryArg(dir);
    }

    return .{
        .step = aapt2link,
        .file = resources_apk,
    };
}

pub const ZipFile = struct {
    step: *std.Build.Step,
    file: std.Build.LazyPath,
};

pub const CopyFile = struct {
    src: std.Build.LazyPath,
    dst: []const u8,
};

pub const EntryPoint = union(enum) {
    artifact: *std.Build.Step.Compile,
    bin: CopyFile,
};

// https://developer.android.com/ndk/guides/abis#native-code-in-app-packages
pub fn makeZipfile(
    b: *std.Build,
    tools: *android.Tools,
    dependOn: *std.Build.Step,
    android_manifest_file: std.Build.LazyPath,
    entrypoint: EntryPoint,
    resource_directory: ?std.Build.LazyPath,
    assets_directory: ?std.Build.LazyPath,
    appends: ?[]const CopyFile,
) ZipFile {
    const builder = ApkBuilder.create(
        b,
        tools,
        dependOn,
        android_manifest_file,
        resource_directory,
        assets_directory,
    );

    switch (entrypoint) {
        .artifact => |so| {
            builder.copyRecursive(b, so);
        },
        .bin => |so| {
            builder.copy(so.src, so.dst);
        },
    }

    if (appends) |_copies| {
        for (_copies) |copy| {
            builder.copy(copy.src, copy.dst);
        }
    }

    // Create zip via "jar" as it's cross-platform and aapt2 can't zip *.so or *.dex files.
    // - lib/**/*.so
    // - classes.dex
    // - {directory with all resource files like: AndroidManifest.xml, res/values/strings.xml}
    const zip_file = blk: {
        const jar = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar.setName("jar (zip compress apk)");

        const directory_to_zip = builder.files.getDirectory();
        jar.setCwd(directory_to_zip);
        // NOTE(jae): 2024-09-30
        // Hack to ensure this side-effect re-triggers zipping this up
        jar.addFileInput(directory_to_zip.path(b, "AndroidManifest.xml"));

        // Written as-is from running "jar --help"
        // -c, --create      = Create the archive. When the archive file name specified
        // -u, --update      = Update an existing jar archive
        // -f, --file=FILE   = The archive file name. When omitted, either stdin or
        // -M, --no-manifest = Do not create a manifest file for the entries
        // -0, --no-compress = Store only; use no ZIP compression
        const compress_zip_arg = "-cfM";
        if (b.verbose) jar.addArg(compress_zip_arg ++ "v") else jar.addArg(compress_zip_arg);
        const output_zip_file = jar.addOutputFileArg("compiled_code.zip");
        jar.addArg(".");

        break :blk output_zip_file;
    };

    // Update zip with files that are not compressed (ie. resources.arsc)
    const update_zip_step = blk: {
        const jar = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar.setName("jar (update zip with uncompressed files)");

        const directory_to_zip = builder.files_not_compressed.getDirectory();
        jar.setCwd(directory_to_zip);
        // NOTE(jae): 2025-03-23
        // Hack to ensure this side-effect re-triggers zipping this up
        jar.addFileInput(builder.files_not_compressed.getDirectory().path(b, "resources.arsc"));

        // Written as-is from running "jar --help"
        // -c, --create      = Create the archive. When the archive file name specified
        // -u, --update      = Update an existing jar archive
        // -f, --file=FILE   = The archive file name. When omitted, either stdin or
        // -M, --no-manifest = Do not create a manifest file for the entries
        // -0, --no-compress = Store only; use no ZIP compression
        const update_zip_arg = "-ufM0";
        if (b.verbose) jar.addArg(update_zip_arg ++ "v") else jar.addArg(update_zip_arg);
        jar.addFileArg(zip_file);
        jar.addArg(".");
        break :blk &jar.step;
    };

    return .{
        .step = update_zip_step,
        .file = zip_file,
    };
}

// Align contents of .apk (zip)
pub fn runZipalign(
    b: *std.Build,
    tools: *android.Tools,
    zip_file: std.Build.LazyPath,
    artifact_name: []const u8,
) struct {
    step: *std.Build.Step,
    file: std.Build.LazyPath,
} {
    var zipalign = b.addSystemCommand(&.{
        tools.build_tools.zipalign,
    });
    zipalign.setName("zipalign");

    // If you use apksigner, zipalign must be used before the APK file has been signed.
    // If you sign your APK using apksigner and make further changes to the APK, its signature is invalidated.
    // Source: https://developer.android.com/tools/zipalign (10th Sept, 2024)
    //
    // Example: "zipalign -P 16 -f -v 4 infile.apk outfile.apk"
    if (b.verbose) {
        zipalign.addArg("-v");
    }
    zipalign.addArgs(&.{
        "-P", // aligns uncompressed .so files to the specified page size in KiB...
        "16", // ... align to 16kb
        "-f", // overwrite existing files
        // "-z", // recompresses using Zopfli. (very very slow)
        "4",
    });

    // Depend on zip file and the additional update to it
    zipalign.addFileArg(zip_file);

    return .{
        .step = &zipalign.step,
        .file = zipalign.addOutputFileArg(b.fmt("aligned-{s}.apk", .{artifact_name})),
    };
}

pub fn runApksigner(
    b: *std.Build,
    tools: *android.Tools,
    aligned_apk_file: std.Build.LazyPath,
) std.Build.LazyPath {
    // const key_store: KeyStore = apk.key_store orelse .{
    //     .file = .{ .cwd_relative = "" },
    //     .password = "",
    // };
    const key_store = tools.createKeyStore(android.CreateKey.example());

    const apksigner = b.addSystemCommand(&.{
        tools.build_tools.apksigner,
        "sign",
    });
    apksigner.setName("apksigner");
    apksigner.addArg("--ks"); // ks = keystore
    apksigner.addFileArg(key_store.file);
    apksigner.addArgs(&.{ "--ks-pass", b.fmt("pass:{s}", .{key_store.password}) });
    apksigner.addArg("--out");
    const signed_output_apk_file = apksigner.addOutputFileArg("signed-and-aligned-apk.apk");
    apksigner.addFileArg(aligned_apk_file);
    // break :blk signed_output_apk_file;
    return signed_output_apk_file;
}
