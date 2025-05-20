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

  XrSystemGetInfo systemInfo{
      .type = XR_TYPE_SYSTEM_GET_INFO,
      .formFactor = m_options.Parsed.FormFactor,
  };
  CHECK_XRCMD(xrGetSystem(m_instance, &systemInfo, &m_systemId));

  // ?
  xrGetVulkanGraphicsRequirements2KHR

  // OpenXR 側で VkInstance を作成する
  xrCreateVulkanInstanceKHR

  // OpenXR 側で VkPhysicalDevice
  xrGetVulkanGraphicsDevice2KHR

  // OpenXR 側で VkDevice
  xrCreateVulkanDeviceKHR
```

## XrSession

```cpp
xrCreateSession
```

## Swapchain

- https://registry.khronos.org/OpenXR/specs/1.0/man/html/XrSwapchainImageVulkan2KHR.html

- https://developers.meta.com/horizon/documentation/native/android/mobile-openxr-swapchains/

```
VK_FORMAT_B8G8R8A8_SRGB
```

## Render

```cpp
  XrFrameWaitInfo frameWaitInfo{.type = XR_TYPE_FRAME_WAIT_INFO};
  XrFrameState frameState{.type = XR_TYPE_FRAME_STATE};
  CHECK_XRCMD(xrWaitFrame(m_session, &frameWaitInfo, &frameState));
```

```cpp
  XrFrameBeginInfo frameBeginInfo{.type = XR_TYPE_FRAME_BEGIN_INFO};
  CHECK_XRCMD(xrBeginFrame(m_session, &frameBeginInfo));

  // layer に描画
  std::vector<XrCompositionLayerBaseHeader *> layers;

  XrCompositionLayerProjection layer{XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  std::vector<XrCompositionLayerProjectionView> projectionLayerViews;
  if (frameState.shouldRender == XR_TRUE) {
    // if (RenderLayer(frameState.predictedDisplayTime, projectionLayerViews,
    //                 layer)) {
    //   layers.push_back(
    //       reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
    // }
    XrViewLocateInfo viewLocateInfo{
        .type = XR_TYPE_VIEW_LOCATE_INFO,
        .viewConfigurationType = m_options.Parsed.ViewConfigType,
        .displayTime = predictedDisplayTime,
        .space = m_appSpace,
    };
    auto res = xrLocateViews(m_session, &viewLocateInfo, &viewState,
                             viewCapacityInput, &viewCountOutput, m_views.data());

    xrAcquireSwapchainImage

    layers.push_back(
        reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
  }

  XrFrameEndInfo frameEndInfo{
      .type = XR_TYPE_FRAME_END_INFO,
      .displayTime = frameState.predictedDisplayTime,
      .environmentBlendMode = m_options.Parsed.EnvironmentBlendMode,
      .layerCount = (uint32_t)layers.size(),
      .layers = layers.data(),
  };
  CHECK_XRCMD(xrEndFrame(m_session, &frameEndInfo));
```

### sequence

- views L + R ?
- format
- graphicsPlugin->AllocateSwapchainImageStructs

