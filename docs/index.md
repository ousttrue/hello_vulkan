[samples](https://github.com/ousttrue/hello_vulkan/tree/master/samples)

| sample                                      | platform          | window              | api      | src                              | note                       |
| ------------------------------------------- | ----------------- | ------------------- | -------- | -------------------------------- | -------------------------- |
| simple_activity                             | android+java      | Activity            | no gpu   |                                  |                            |
| vulkan_tutorial                             | win32             | GLFW                | Vulkan   | `Vulkan Tutorial`                | GLFW                       |
| hellotriangle                               | android+NDK       | NativeActivity      | Vulkan   | `ARM-software/vulkan-sdkandroid` | NativeActivity swapchain   |
| vulfwk                                      | win32/android+NDK | GLFW/NativeActivity | Vulkan   |                                  | WIP                        |
| hello_xr                                    | win32/android+NDK | OpenXR              | Vulkan   | `KhronosGroup/OpenXR-SDK-Source` | cross platform x cross gpu |
| ovr_openxr_mobile_sdk/XrHandsFB             | android+NDK       | OpenXR              | OpenGLES | `Meta OpenXR SDK`                |                            |
| ovr_openxr_mobile_sdk/XrPassthrough         | android+NDK       | OpenXR              | OpenGLES | `Meta OpenXR SDK`                |                            |
| ovr_openxr_mobile_sdk/XrVirtualKeyboard     | android+NDK       | OpenXR              | OpenGLES | `Meta OpenXR SDK`                |                            |
| ovr_openxr_mobile_sdk/XrHandsAndControllers | android+NDK       | OpenXR              | OpenGLES | `Meta OpenXR SDK`                |                            |

::: tip vulkan
Win32/NDK 組み込みは簡単。

- NDK には vulkan が含まれている (/usr/include/vulkan/vulkan.h) ので特別な準備は不用。

Swapchain など難解。
:::

::: tip OpenGL
glew などによる OpenGL4 の準備が繁雑。
OpenGL4 と OpenGLES の違いへの対応。
:::

::: tip OpenXR
cmake FetchContent により Win32/NDK ともに問題なく組み込める。
:::

