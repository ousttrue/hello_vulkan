# VkSurfaceKHR

OS の Windows システムに依存して作成する。

## dependencies

:::tip Vulkan
- `VkInstance Instance`
:::

:::tip android
- `VK_KHR_android_surface`
- `ANativeWindow *window`
:::

:::tip windows
- `VK_KHR_win32_surface`
- `HINSTANCE`
- `HWND`
:::

:::tip glfw
glfwGetRequiredInstanceExtensions 
:::

### code

android 例

```cpp
  ANativeWindow *window;

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = 0,
      .flags = 0,
      .window = window,
  };
  if (vkCreateAndroidSurfaceKHR(Instance, &info, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("vkCreateAndroidSurfaceKHR");
    return false;
  }

  // ~~~

  vkDestroySurfaceKHR(Instance, Surface, nullptr);
```

