# Multi-View Rendering

- [ステレオレンダリング - Unity マニュアル](https://docs.unity3d.com/ja/2022.3/Manual/SinglePassStereoRendering.html)
- [ステレオレンダリングについて調べる](https://zenn.dev/katopan/articles/0cdbb2879953e6)

## Multi-pass

右目と左目で2回レンダリングする。

- https://developers.meta.com/horizon/documentation/native/android/mobile-openxr-swapchains/

`XrSwapchainCreateInfo::arraySize=1`

## Single-pass

右目と左目をまとめてレンダリングする。

https://community.khronos.org/t/what-is-the-right-way-to-implement-single-pass-rendering-with-openxr/109157/3

`XrSwapchainCreateInfo::arraySize=2`

### `VK_KHR_multiview`

https://amini-allight.org/post/openxr-tutorial-addendum-2-single-swapchain

https://www.saschawillems.de/blog/2018/06/08/multiview-rendering-in-vulkan-using-vk_khr_multiview/

### `OCULUS_multiview` `OVR_multiview` `OVR_multiview2`  

- https://community.arm.com/arm-community-blogs/b/mobile-graphics-and-gaming-blog/posts/optimizing-virtual-reality-understanding-multiview
- https://arm-software.github.io/opengl-es-sdk-for-android/multiview.html
- [OpenGL マルチビューレンダリング #C++ - Qiita](https://qiita.com/pochi_ky/items/a0e188f578ae80cbbd63)

### `VPAndRTArrayIndexFromAnyShaderFeedingRasterizer`

- https://learn.microsoft.com/en-us/windows/win32/api/d3d11/ns-d3d11-d3d11_feature_data_d3d11_options3

### 

- https://developers.meta.com/horizon/documentation/native/android/mobile-multiview
- https://developers.meta.com/horizon/documentation/web/web-multiview/

## XrSwapchainCreateInfo

https://registry.khronos.org/OpenXR/specs/1.1/man/html/XrSwapchainCreateInfo.html

## note

- https://developer.nvidia.com/vrworks/graphics/multiview

- https://community.arm.com/cfs-file/__key/communityserver-blogs-components-weblogfiles/00-00-00-20-66/5_2D00_mmg_2D00_siggraph2016_2D00_multiview_2D00_cass.pdf

## openxr-vulkan-example

https://github.com/janhsimon/openxr-vulkan-example
