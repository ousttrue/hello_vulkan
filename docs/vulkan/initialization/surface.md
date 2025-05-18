# VkSurfaceKHR

## Android

### dependencies

:::tip Vulkan
- `VkInstance Instance`
- `VK_KHR_android_surface`
:::

:::tip OS
- `ANativeWindow *window`
:::

### code

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

## Windows

### dependencies

:::tip Vulkan
- `VkInstance Instance`
- `VK_KHR_win32_surface`
:::

:::tip OS
- `HINSTANCE`
- `HWND`
:::

- glfwGetRequiredInstanceExtensions 

### code


