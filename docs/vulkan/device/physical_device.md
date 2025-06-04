# VkDevice

## depencencies

- VkInstance
- VkPhysicalDevice(gpu): VkInstance から列挙して選ぶ
- GraphicsQueueFamilyIndex: VkPhysicalDevice から列挙して選ぶ
- VkSurfaceKHR
- PresentaionQueueFamilyIndex: VkPhysicalDevice から列挙して VkSurfaceKHR に対して present できるものを選ぶ
- device layer
- device extension

## code

```cpp
  VkDeviceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(deviceExtensionNames.size()),
      .ppEnabledExtensionNames = deviceExtensionNames.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    LOGE("failed to create logical device!");
    return false;
  }
```
