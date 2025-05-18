# c++

## 指示付き初期化 `c++20`

https://cpprefjp.github.io/lang/cpp20/designated_initialization.html

vulkan で struct を初期化するときに使いたい。

```cpp
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
```
