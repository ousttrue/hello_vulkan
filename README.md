# hello_vulkan

202505 作り直し

## windows

```sh
> cmake -S . -B builddir -G Ninja -DCMAKE_BUILD_TYPE=Debug
```

## android

```sh
> cmake -S . -B build_android -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-30 -DANDROID_NDK="${env:ANDROID_HOME}\ndk\29.0.13113456" -DCMAKE_TOOLCHAIN_FILE="${env:ANDROID_HOME}\ndk\29.0.13113456/build/cmake/android.toolchain.cmake" -G Ninja -DPLATFORM=android -DCMAKE_BUILD_TYPE=Debug
```
