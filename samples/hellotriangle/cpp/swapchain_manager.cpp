#include "swapchain_manager.hpp"
#include "logger.hpp"
#include "device_manager.hpp"
#include "semaphore_manager.hpp"
#include <vulkan/vulkan_core.h>

SwapchainManager::SwapchainManager(VkDevice device, VkQueue presentaionQueue,
                                   VkSwapchainKHR swapchain)
    : _device(device), _presentationQueue(presentaionQueue),
      _swapchain(swapchain) {
  _semaphoreManager = std::make_shared<SemaphoreManager>(_device);
}

SwapchainManager::~SwapchainManager() {
  vkDestroySwapchainKHR(_device, _swapchain, nullptr);
}

std::shared_ptr<SwapchainManager>
SwapchainManager::create(VkPhysicalDevice gpu, VkSurfaceKHR surface,
                         VkDevice device, uint32_t graphicsQueueIndex,
                         VkQueue presentaionQueue, VkRenderPass renderPass,
                         VkSwapchainKHR oldSwapchain) {
  VkSurfaceFormatKHR format = DeviceManager::getSurfaceFormat(gpu, surface);
  if (format.format == VK_FORMAT_UNDEFINED) {
    LOGE("VK_FORMAT_UNDEFINED");
    abort();
  }

  VkSurfaceCapabilitiesKHR surfaceProperties;
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          gpu, surface, &surfaceProperties) != VK_SUCCESS) {
    LOGE("vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    abort();
  }
  if (surfaceProperties.currentExtent.width == -1u) {
    // -1u is a magic value (in Vulkan specification) which means there's no
    // fixed size.
    LOGE("-1 size");
    abort();
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
  if (vkCreateSwapchainKHR(device, &info, nullptr, &swapchain) != VK_SUCCESS) {
    LOGE("vkCreateSwapchainKHR");
    abort();
  }

  auto ptr = std::shared_ptr<SwapchainManager>(
      new SwapchainManager(device, presentaionQueue, swapchain));
  ptr->_size = swapchainSize;
  ptr->_format = format.format;

  uint32_t imageCount;
  if (vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr) !=
      VK_SUCCESS) {
    LOGE("vkGetSwapchainImagesKHR");
    abort();
  }
  ptr->_swapchainImages.resize(imageCount);
  if (vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                              ptr->_swapchainImages.data()) != VK_SUCCESS) {
    LOGE("vkGetSwapchainImagesKHR");
    abort();
  }

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < ptr->_swapchainImages.size(); ++i) {
    auto image = ptr->_swapchainImages[i];
    auto p = new Backbuffer(i, device, graphicsQueueIndex, image, format.format,
                            swapchainSize, renderPass);
    auto backbuffer = std::shared_ptr<Backbuffer>(p);
    ptr->_backbuffers.push_back(backbuffer);
  }

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

void SwapchainManager::addClearedSemaphore(VkSemaphore semaphore) {
  // Recycle the old semaphore back into the semaphore manager.
  if (semaphore != VK_NULL_HANDLE) {
    _semaphoreManager->addClearedSemaphore(semaphore);
  }
}
