# OpenXR samples

## Introduction to OpenXR

- https://playdeck.net/blog/introduction-to-openxr
- https://github.com/maluoi/OpenXRSamples

```cpp
#define XR_USE_PLATFORM_WIN32
#define XR_USE_GRAPHICS_API_D3D11
arraySize = 1
```

## openxr-simple-example

- https://github.com/KHeresy/openxr-simple-example

```cpp
#define XR_USE_GRAPHICS_API_OPENGL
#define XR_USE_PLATFORM_WIN32
arraySize = 1
```

## OpenXR-SDK-Source

`Platform(WIN32, ANDROID...etc)` x `Graphics(D3D11, D3D12, OpenGL, OpenGLES, Vulkan...etc)`

- https://github.com/KhronosGroup/OpenXR-SDK-Source/tree/main/src/tests/hello_xr

cross platform(win, android...) x cross gpu(vulkan, opengl, d3d...) で難読。
Swapchain 周りが難読。

## android_openxr_gles

openxr + GLES2 ? + imgui

TODO: vao を使うように改造。

https://github.com/terryky/android_openxr_gles

## Meta OpenXR SDK

`OVR` ライブラリーが難読。OpenXR => OVR => OpenXR のような処理あり。

https://github.com/meta-quest/Meta-OpenXR-SDK

- https://developers.meta.com/horizon/documentation/native/android/mobile-openxr-sample

## Others

- https://github.com/janhsimon/openxr-vulkan-example
- https://github.com/donalshortt/mujoco_vr
- https://github.com/cmbruns/pyopenxr_examples
- https://github.com/StereoKit/StereoKit
