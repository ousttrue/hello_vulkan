#include "swapchain_manager.hpp"
#include "common.hpp"
#include "device_manager.hpp"
#include "semaphore_manager.hpp"

SwapchainManager::SwapchainManager(VkDevice device, VkQueue graphicsQueue,
                                   VkQueue presentaionQueue,
                                   VkSwapchainKHR swapchain)
    : _device(device), _graphicsQueue(graphicsQueue),
      _presentationQueue(presentaionQueue), _swapchain(swapchain) {
  _semaphoreManager = std::make_shared<SemaphoreManager>(_device);
}

SwapchainManager::~SwapchainManager() {
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

// VkSwapchainKHR oldSwapchain = swapchain;
std::shared_ptr<SwapchainManager>
SwapchainManager::create(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                         VkDevice device, uint32_t graphicsQueueIndex,
                         VkQueue graphicsQueue, VkQueue presentaionQueue,
                         VkRenderPass renderPass, VkSwapchainKHR oldSwapchain) {
  VkSurfaceFormatKHR format = DeviceManager::getSurfaceFormat(gpu, surface);
  if (format.format == VK_FORMAT_UNDEFINED) {
    LOGE("VK_FORMAT_UNDEFINED");
    abort();
  }

  VkSurfaceCapabilitiesKHR surfaceProperties;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface,
                                                     &surfaceProperties));
  if (surfaceProperties.currentExtent.width == -1u) {
    // -1u is a magic value (in Vulkan specification) which means there's no
    // fixed size.
    LOGE("-1 size");
    abort();
    // swapchainSize.width = dim.width;
    // swapchainSize.height = dim.height;
  }
  auto swapchainSize = surfaceProperties.currentExtent;

  // FIFO must be supported by all implementations.
  VkPresentModeKHR swapchainPresentMode = VK_PRESENT_MODE_FIFO_KHR;

  // Determine the number of VkImage's to use in the swapchain.
  // Ideally, we desire to own 1 image at a time, the rest of the images can
  // either be rendered to and/or
  // being queued up for display.
  uint32_t desiredSwapchainImages = surfaceProperties.minImageCount + 1;
  if ((surfaceProperties.maxImageCount > 0) &&
      (desiredSwapchainImages > surfaceProperties.maxImageCount)) {
    // Application must settle for fewer images than desired.
    desiredSwapchainImages = surfaceProperties.maxImageCount;
  }

  // Figure out a suitable surface transform.
  VkSurfaceTransformFlagBitsKHR preTransform;
  if (surfaceProperties.supportedTransforms &
      VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR)
    preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  else
    preTransform = surfaceProperties.currentTransform;

  // Find a supported composite type.
  VkCompositeAlphaFlagBitsKHR composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  if (surfaceProperties.supportedCompositeAlpha &
      VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR;
  else if (surfaceProperties.supportedCompositeAlpha &
           VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR)
    composite = VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR;

  VkSwapchainCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = desiredSwapchainImages,
      .imageFormat = format.format,
      .imageColorSpace = format.colorSpace,
      .imageExtent = {.width = swapchainSize.width,
                      .height = swapchainSize.height},
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .imageSharingMode = VK_SHARING_MODE_EXCLUSIVE,
      .preTransform = preTransform,
      .compositeAlpha = composite,
      .presentMode = swapchainPresentMode,
      .clipped = true,
      .oldSwapchain = oldSwapchain,
  };

  VkSwapchainKHR swapchain;
  VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));

  // if (oldSwapchain != VK_NULL_HANDLE){
  //   vkDestroySwapchainKHR(device, oldSwapchain, nullptr);
  // }

  auto ptr = std::shared_ptr<SwapchainManager>(
      new SwapchainManager(device, graphicsQueue, presentaionQueue, swapchain));
  ptr->_size = swapchainSize;
  ptr->_format = format.format;

  uint32_t imageCount;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
  ptr->_swapchainImages.resize(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                   ptr->_swapchainImages.data()));

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < ptr->_swapchainImages.size(); ++i) {
    auto image = ptr->_swapchainImages[i];
    auto p = new Backbuffer(i, device, graphicsQueueIndex, image, format.format,
                            swapchainSize, renderPass);
    auto backbuffer = std::shared_ptr<Backbuffer>(p);
    ptr->_backbuffers.push_back(backbuffer);
  }

  ptr->setRenderingThreadCount(ptr->_renderingThreadCount);

  return ptr;
}

std::tuple<VkResult, VkSemaphore, std::shared_ptr<Backbuffer>>
SwapchainManager::AcquireNext() {
  auto acquireSemaphore = _semaphoreManager->getClearedSemaphore();
  uint32_t index;
  VkResult res =
      vkAcquireNextImageKHR(_device, _swapchain, UINT64_MAX, acquireSemaphore,
                            VK_NULL_HANDLE, &index);
  if (res == VK_SUCCESS) {
    _swapchainIndex = index;
    return {res, acquireSemaphore, _backbuffers[_swapchainIndex]};
  } else {
    return {res, acquireSemaphore, {}};
  }
}

void SwapchainManager::sync(VkQueue queue, VkSemaphore acquireSemaphore) {
  vkQueueWaitIdle(queue);
  _semaphoreManager->addClearedSemaphore(acquireSemaphore);
}

bool SwapchainManager::presentImage(unsigned index) {
  VkPresentInfoKHR present = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores =
          &_backbuffers[_swapchainIndex]->_swapchainReleaseSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &_swapchain,
      .pImageIndices = &index,
      .pResults = nullptr,
  };

  VkResult res = vkQueuePresentKHR(_presentationQueue, &present);
  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
  else if (res != VK_SUCCESS)
    return MaliSDK::RESULT_ERROR_GENERIC;
  else
    return MaliSDK::RESULT_SUCCESS;
}

void SwapchainManager::submitSwapchain(VkCommandBuffer cmd) {
  // For the first frames, we will create a release semaphore.
  // This can be reused every frame. Semaphores are reset when they have been
  // successfully been waited on.
  // If we aren't using acquire semaphores, we aren't using release semaphores
  // either.
  if (_backbuffers[_swapchainIndex]->_swapchainReleaseSemaphore ==
          VK_NULL_HANDLE &&
      _backbuffers[_swapchainIndex]->_swapchainAcquireSemaphore !=
          VK_NULL_HANDLE) {
    VkSemaphore releaseSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(
        vkCreateSemaphore(_device, &semaphoreInfo, nullptr, &releaseSemaphore));
    _backbuffers[_swapchainIndex]->setSwapchainReleaseSemaphore(
        releaseSemaphore);
  }

  _backbuffers[_swapchainIndex]->submitCommandBuffer(_graphicsQueue, cmd);
}

VkCommandBuffer
SwapchainManager::beginRender(const std::shared_ptr<Backbuffer> &backbuffer) {
  // Request a fresh command buffer.
  VkCommandBuffer cmd =
      _backbuffers[_swapchainIndex]->_commandManager.requestCommandBuffer();

  return cmd;
}

void SwapchainManager::setRenderingThreadCount(unsigned count) {
  // vkQueueWaitIdle(_device->Queue);
  for (auto &pFrame : _backbuffers)
    pFrame->setSecondaryCommandManagersCount(count);
  _renderingThreadCount = count;
}

void SwapchainManager::addClearedSemaphore(VkSemaphore semaphore) {
  // Recycle the old semaphore back into the semaphore manager.
  if (semaphore != VK_NULL_HANDLE) {
    _semaphoreManager->addClearedSemaphore(semaphore);
  }
}
