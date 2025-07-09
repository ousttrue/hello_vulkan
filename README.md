# hello_vulkan

202505 作り直し

| platform                 | graphics   | status | note       |
| ------------------------ | ---------- | ------ | ---------- |
| windows + glfw3          | OpenGL-4.3 | ok     | desktop    |
| windows + glfw3          | Vulkan-1.3 | ok     | desktop    |
| windows + openxr         | OpenGL-4.3 | ok     | quest link |
| windows + openxr         | Vulkan-1.3 | ok     | quest link |
| android + NativeActivity | OpenGLES3  | ok     | pixel3 etc |
| android + NativeActivity | Vulkan     | ok     | pixel3 etc |
| android + openxr         | OpenGLES3  | ok     | quest3     |
| android + openxr         | Vulkan     | ok     | quest3     |

note

- clang-20(for `#embed` macro)
- TODO: scene compatibility OpenGL betwenn Vulkan

## build windows

### 1. deps

- build_deps/x86_64/debug
- build_deps/x86_64/release
- build_deps/arm64-v8a/debug
- build_deps/arm64-v8a/release
- prefix/x86_64/debug
- prefix/x86_64/release
- prefix/arm64-v8a/debug
- prefix/arm64-v8a/release

```sh
> $env:CMAKE_BUILD_TYPE=Debug
> cmake -S deps -B build_deps -G Ninja
> cmake --build build_deps
> cmake --install build_deps
```

### 2. exe

- build/x86_64/debug
- build/x86_64/release
- build/arm64-v8a/debug
- build/arm64-v8a/release

```sh
> $env:CMAKE_BUILD_TYPE=Debug
> cmake -S . -B build -G Ninja
> cmake --build build
```

## build android

### 1. deps

```sh
> cmake -S deps -B android_deps -G Ninja -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_NDK="${env:ANDROID_HOME}\ndk\29.0.13113456" -DCMAKE_TOOLCHAIN_FILE="${env:ANDROID_HOME}\ndk\29.0.13113456/build/cmake/android.toolchain.cmake" -G Ninja -DPLATFORM=android
> cmake --build android_deps
> cmake --install android_deps
```

### 2. so

```sh
> cmake -S . -B build_android -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_NDK="${env:ANDROID_HOME}\ndk\29.0.13113456" -DCMAKE_TOOLCHAIN_FILE="${env:ANDROID_HOME}\ndk\29.0.13113456/build/cmake/android.toolchain.cmake" -G Ninja -DPLATFORM=android
> cmake --build build_android
```
