const std = @import("std");
const android = @import("android");
const aapt2 = @import("aapt2.zig");
const D8Glob = @import("d8glob.zig");

pub const CopyFile = struct {
    src: std.Build.LazyPath,
    dst: []const u8,
};

pub const AppendFile = union(enum) {
    artifact: *std.Build.Step.Compile,
    path: CopyFile,
};

pub const ApkContents = struct {
    android_manifest: std.Build.LazyPath,
    resource_directory: ?std.Build.LazyPath = null,
    assets_directory: ?std.Build.LazyPath = null,
    java_files: []const std.Build.LazyPath = &.{},
    appends: []const AppendFile = &.{},
};

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

    // NOTE(jae): 2025-03-23 - https://github.com/silbinarywolf/zig-android-sdk/issues/23
    // We apply resources.arsc seperately to the zip file to avoid compressing it, otherwise we get the following
    // error when we "adb install"
    // - "Targeting R+ (version 30 and above) requires the resources.arsc of installed APKs to be stored uncompressed and aligned on a 4-byte boundary"
    //
    // https://developer.android.com/about/versions/11/behavior-changes-11
    resources_arsc_dir: *std.Build.Step.WriteFile,

    pub fn create(
        b: *std.Build,
        tools: *android.Tools,
        dependOn: *std.Build.Step,
        package_name: []const u8,
        contents: ApkContents,
    ) @This() {
        // aapt2 link
        // resource_directory
        // assets_directory
        const resources_apk = aapt2.link(
            b,
            tools,
            package_name,
            contents.android_manifest,
            contents.resource_directory,
            contents.assets_directory,
        );

        // Extract compiled resources.apk and add contents to the folder we'll zip with "jar" below
        // See: https://musteresel.github.io/posts/2019/07/build-android-app-bundle-on-command-line.html
        const jar = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar.setName("jar (unzip resources.apk)");
        jar.step.dependOn(dependOn);
        if (b.verbose) {
            jar.addArg("--verbose");
        }

        // Extract *.apk file created with "aapt2 link"
        jar.addArg("--extract");
        jar.addPrefixedFileArg("--file=", resources_apk.file);

        // NOTE(jae): 2024-09-30
        // Extract to directory of resources_apk and force add that to the overall apk files.
        // This currently has an issue where because we can't use "addOutputDirectoryArg" this
        // step will always be executed.
        const extracted_apk_dir = resources_apk.file.dirname();
        jar.setCwd(extracted_apk_dir);

        const self = @This(){
            .files = b.addWriteFiles(),
            .resources_arsc_dir = b.addWriteFiles(),
        };
        self.files.step.dependOn(&jar.step);
        self.resources_arsc_dir.step.dependOn(&jar.step);

        _ = self.files.addCopyDirectory(extracted_apk_dir, "", .{
            .exclude_extensions = &.{
                // ignore the *.apk that exists in this directory
                ".apk",
                // ignore resources.arsc as Android 30+ APIs does not supporting
                // compressing this in the zip file
                ".arsc",
                // R.java
                ".java",
            },
        });

        if (contents.java_files.len > 0) {
            const java_classes_output_dir = blk: {
                // https://docs.oracle.com/en/java/javase/17/docs/specs/man/javac.html
                const javac_cmd = b.addSystemCommand(&[_][]const u8{
                    tools.java_tools.javac,
                    // NOTE(jae): 2024-09-22
                    // Force encoding to be "utf8", this fixes the following error occuring in Windows:
                    // error: unmappable character (0x8F) for encoding windows-1252
                    // Source: https://github.com/libsdl-org/SDL/blob/release-2.30.7/android-project/app/src/main/java/org/libsdl/app/SDLActivity.java#L2045
                    "-encoding",
                    "utf8",
                    "-cp",
                    tools.root_jar,
                        // NOTE(jae): 2024-09-19
                        // Debug issues with the SDL.java classes
                        // "-Xlint:deprecation",
                });
                javac_cmd.setName("javac");
                javac_cmd.addArg("-d");
                const output_dir = javac_cmd.addOutputDirectoryArg("android_classes");
                for (contents.java_files) |java_file| {
                    javac_cmd.addFileArg(java_file);
                }
                javac_cmd.addFileArg(resources_apk.r_java);

                break :blk output_dir;
            };

            // From d8.bat
            // call "%java_exe%" %javaOpts% -cp "%jarpath%" com.android.tools.r8.D8 %params%
            {
                const d8 = b.addSystemCommand(&[_][]const u8{
                    tools.build_tools.d8,
                });
                d8.setName("d8");

                // ie. android_sdk/platforms/android-{api-level}/android.jar
                d8.addArg("--lib");
                d8.addArg(tools.root_jar);

                d8.addArg("--output");
                const dex_output_dir = d8.addOutputDirectoryArg("android_dex");

                // NOTE(jae): 2024-09-22
                // As per documentation for d8, we may want to specific the minimum API level we want
                // to support. Not sure how to test or expose this yet. See: https://developer.android.com/tools/d8
                // d8.addArg("--min-api");
                // d8.addArg(number_as_string);

                // add each output *.class file
                D8Glob.create(b, d8, java_classes_output_dir);
                const dex_file = dex_output_dir.path(b, "classes.dex");

                // Append classes.dex to apk
                _ = self.files.addCopyFile(dex_file, "classes.dex");
            }
        }
        _ = self.resources_arsc_dir.addCopyFile(extracted_apk_dir.path(b, "resources.arsc"), "resources.arsc");

        return self;
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

    // Create zip via "jar" as it's cross-platform and aapt2 can't zip *.so or *.dex files.
    // - lib/**/*.so
    // - classes.dex
    // - {directory with all resource files like: AndroidManifest.xml, res/values/strings.xml}
    fn build(
        self: *const @This(),
        b: *std.Build,
        tools: *android.Tools,
    ) ZipFileStep {
        const jar_make = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar_make.setName("jar (zip compress apk)");

        const directory_to_zip = self.files.getDirectory();
        jar_make.setCwd(directory_to_zip);
        // NOTE(jae): 2024-09-30
        // Hack to ensure this side-effect re-triggers zipping this up
        jar_make.addFileInput(directory_to_zip.path(b, "AndroidManifest.xml"));

        // Written as-is from running "jar --help"
        // -c, --create      = Create the archive. When the archive file name specified
        // -f, --file=FILE   = The archive file name. When omitted, either stdin or
        // -M, --no-manifest = Do not create a manifest file for the entries
        const compress_zip_arg = "-cfM";
        if (b.verbose) jar_make.addArg(compress_zip_arg ++ "v") else jar_make.addArg(compress_zip_arg);
        const zip_file = jar_make.addOutputFileArg("compiled_code.zip");
        jar_make.addArg(".");

        //
        // Update zip with files that are not compressed (ie. resources.arsc)
        //
        const jar_update = b.addSystemCommand(&[_][]const u8{
            tools.java_tools.jar,
        });
        jar_update.step.dependOn(&jar_make.step);
        jar_update.setName("jar (update zip with uncompressed files)");

        // NOTE(jae): 2025-03-23
        // Hack to ensure this side-effect re-triggers zipping this up
        jar_update.addFileInput(self.resources_arsc_dir.getDirectory().path(b, "resources.arsc"));

        // Written as-is from running "jar --help"
        // -u, --update      = Update an existing jar archive
        // -f, --file=FILE   = The archive file name. When omitted, either stdin or
        // -M, --no-manifest = Do not create a manifest file for the entries
        // -0, --no-compress = Store only; use no ZIP compression
        const update_zip_arg = "-ufM0";
        if (b.verbose) jar_update.addArg(update_zip_arg ++ "v") else jar_update.addArg(update_zip_arg);
        jar_update.addFileArg(zip_file);

        jar_update.setCwd(self.resources_arsc_dir.getDirectory());
        jar_update.addArg(".");

        return .{
            .step = &jar_update.step,
            .file = zip_file,
        };
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

pub const ZipFileStep = struct {
    step: *std.Build.Step,
    file: std.Build.LazyPath,
};

// https://developer.android.com/ndk/guides/abis#native-code-in-app-packages
pub fn makeZipfile(
    b: *std.Build,
    tools: *android.Tools,
    dependOn: *std.Build.Step,
    package_name: []const u8,
    zip: ApkContents,
) ZipFileStep {
    const builder = ApkBuilder.create(
        b,
        tools,
        dependOn,
        package_name,
        zip,
    );

    for (zip.appends) |append| {
        switch (append) {
            .artifact => |a| {
                builder.copyRecursive(b, a);
            },
            .path => |p| {
                builder.copy(p.src, p.dst);
            },
        }
        // builder.copy(append.src, append.dst);
    }

    return builder.build(b, tools);
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
