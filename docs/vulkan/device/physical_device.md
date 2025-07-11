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

```sh
INFO: [NVIDIA GeForce RTX 3060 Ti] DISCRETE_GPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] ooooo____
INFO:   [01] ___oo____
INFO:   [02] o_ooo____
INFO:   [03] ___oo_o__
INFO:   [04] ___oo__o_
INFO:   [05] ___oo___o

INFO: [NVIDIA GeForce RTX 4060 Laptop GPU] DISCRETE_GPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] ooooo____
INFO:   [01] ___oo____
INFO:   [02] o_ooo____
INFO:   [03] ___oo_o__
INFO:   [04] ___oo__o_
INFO:   [05] ___oo___o
INFO: [Microsoft Direct3D12 (NVIDIA GeForce RTX 4060 Laptop GPU)] DISCRETE_GPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] oooo_____
INFO:   [01] o_oo_____
INFO: [Intel(R) Iris(R) Xe Graphics] INTEGRATED_GPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] ooooo____
INFO: [Microsoft Direct3D12 (Intel(R) Iris(R) Xe Graphics)] INTEGRATED_GPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] oooo_____
INFO:   [01] o_oo_____
INFO: [Microsoft Direct3D12 (Microsoft Basic Render Driver)] CPU
INFO:   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
INFO:   [00] oooo_____
INFO:   [01] o_oo_____

# Pixel3a
I vko     : [Adreno (TM) 615] INTEGRATED_GPU
I vko     :   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
I vko     :   [00] ooo__o___

# Quest3
I vko     : [Adreno (TM) 740] INTEGRATED_GPU
I vko     :   queue info: present,graphics,compute,transfer,sparse,protected,video_de,video_en,optical
I vko     :   [00] oooooo__o
I vko     :   [01] o___o____
```
