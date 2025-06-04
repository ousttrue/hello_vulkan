# VkInstance

使用する layer と instance extension を決める必用がある。

## dependencies

:::tip PLATFORM 共通
- `VK_KHR_surface`
:::

:::tip PLATFORM android
- `#define VK_USE_PLATFORM_ANDROID_KHR 1`
- `VK_KHR_android_surface`
:::

:::tip PLATFORM windows
- `#define VK_USE_PLATFORM_WIN32_KHR 1`
- `VK_KHR_win32_surface`
:::

:::tip Validation

Debug / Release などに応じて。

- `VK_LAYER_KHRONOS_validation`
- `VK_EXT_debug_utils` もしくは `VK_EXT_debug_report`(旧: callback の様式が違う)

https://developer.android.com/ndk/guides/graphics/validation-layer?hl=ja

> 1.1.106.0 Vulkan SDK リリース以降では、アプリで単一の検証レイヤ VK_LAYER_KHRONOS_validation を有効にするだけで、旧バージョンの検証レイヤの機能をすべて取得できます。

:::

## code

```cpp
  std::vector<const char*> instanceLayers = {"VK_LAYER_KHRONOS_validation"};
  std::vector<const char*> instanceExtensions = {
    "VK_KHR_surface", "VK_KHR_android_surface"
    "VK_EXT_debug_utils", // VK_LAYER_KHRONOS_validation の print
  };

  VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app,
      .enabledLayerCount =
          static_cast<uint32_t>(instanceLayers.size()),
      .ppEnabledLayerNames = instanceLayers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(instanceExtensions.size()),
      .ppEnabledExtensionNames = instanceExtensions.data(),
  };

  VkInstance instance;
  VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);

  // ~~~

  vkDestroyInstance(Instance, nullptr);
```
