# VkSwapchain

## vkAcquireNextImageKHR / vkQueuePresentKHR

- https://registry.khronos.org/vulkan/specs/latest/man/html/vkAcquireNextImageKHR.html

Acquire して Present する。その間に image を更新する。

```cpp
  uint32_t imageIndex;
  VkResult result = vkAcquireNextImageKHR(
      _device, _swapchain, UINT64_MAX, _imageAvailableSemaphores[_currentFrame],
      VK_NULL_HANDLE,
      &imageIndex); // imageIndex = index of image available
                    // from swap chain array
```

```cpp
  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &signalSemaphor,
      .swapchainCount = 1,
      .pSwapchains = &_swapchain,
      .pImageIndices = &imageIndex,
      .pResults = nullptr,
  };
  auto result = vkQueuePresentKHR(presentQueue, &presentInfo);
```

### semaphore

## 初期化

## VkSwapchain

### dependency

- VkDevice
- VkQueue(graphics)
- VkQueue(presentation)

#### graphics queue と presentation queue が同一の場合

#### graphics queue と presentation queue が異なる場合

今のところ遭遇したこと無し。

## Backbuffer

Swapchain から VkImage (複数) を得て VkFramebuffer を作成し、 VkSemaphore などで同期システムを構築する。

### dependency

- VkSwapchain
- VkRenderPass
