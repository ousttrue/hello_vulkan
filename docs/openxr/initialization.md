# OpenXR初期化

- `framework方式` https://openxr-tutorial.com/index.html
  - https://openxr-tutorial.com/windows/vulkan/index.html
  - https://openxr-tutorial.com/android/vulkan/index.html

- https://developers.meta.com/horizon/documentation/native/android/mobile-intro

- [OpenXR API編 #OpenGL - Qiita](https://qiita.com/ousttrue/items/8f0cc2727c55fcfd02e1)

## XrInstance

```cpp
  XrInstanceCreateInfo createInfo{
      .type = XR_TYPE_INSTANCE_CREATE_INFO,
      .next = m_platformPlugin->GetInstanceCreateExtension(),
      .createFlags = 0,
      .applicationInfo =
          {.applicationName = "HelloXR",
           // Current version is 1.1.x, but hello_xr only requires 1.0.x
           .applicationVersion = {},
           .engineName = {},
           .engineVersion = {},
           .apiVersion = XR_API_VERSION_1_0},
      .enabledApiLayerCount = 0,
      .enabledApiLayerNames = nullptr,
      .enabledExtensionCount = (uint32_t)extensions.size(),
      .enabledExtensionNames = extensions.data(),
  };
  CHECK_XRCMD(xrCreateInstance(&createInfo, &m_instance));
```

## VkDevice

## XrSession

## Swapchain

- https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSwapchainImageVulkan2KHR.html

- https://developers.meta.com/horizon/documentation/native/android/mobile-openxr-swapchains/


### sequence

- views L + R ?
- format
- graphicsPlugin->AllocateSwapchainImageStructs

