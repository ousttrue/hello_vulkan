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

## windows

```sh
> cmake -S . -B builddir -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

## android

```sh
> cmake -S . -B build_android -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_NDK="${env:ANDROID_HOME}\ndk\29.0.13113456" -DCMAKE_TOOLCHAIN_FILE="${env:ANDROID_HOME}\ndk\29.0.13113456/build/cmake/android.toolchain.cmake" -G Ninja -DPLATFORM=android -DCMAKE_BUILD_TYPE=Debug
```
