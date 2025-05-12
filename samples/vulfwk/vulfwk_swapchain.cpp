#include "vulfwk_swapchain.h"
#include "vulfwk_queuefamily.h"
#include "logger.h"

static VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
}

bool SwapChainSupportDetails ::querySwapChainSupport(VkPhysicalDevice device,
                                                     VkSurfaceKHR surface) {
  if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
          device, surface, &this->capabilities) != VK_SUCCESS) {
    LOGE("failed: vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
    return false;
  }

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    this->formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         this->formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);

  if (presentModeCount != 0) {
    this->presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, this->presentModes.data());
  }

  return true;
}

SwapchainImpl::SwapchainImpl(VkDevice device, VkSwapchainKHR swapchain,
                             VkExtent2D imageExtent, VkFormat format)
    : Device(device), Swapchain(swapchain), SwapchainExtent(imageExtent) {

  uint32_t imageCount;
  vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
  SwapchainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount,
                          SwapchainImages.data());

  // }
  // bool VulkanFramework::createSyncObjects() {
  ImageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  RenderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
  InFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

  VkSemaphoreCreateInfo semaphoreInfo{};
  semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

  VkFenceCreateInfo fenceInfo{};
  fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
  fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    if (vkCreateSemaphore(Device, &semaphoreInfo, nullptr,
                          &ImageAvailableSemaphores[i]) != VK_SUCCESS ||
        vkCreateSemaphore(Device, &semaphoreInfo, nullptr,
                          &RenderFinishedSemaphores[i]) != VK_SUCCESS ||
        vkCreateFence(Device, &fenceInfo, nullptr, &InFlightFences[i]) !=
            VK_SUCCESS) {
      LOGE("failed to create synchronization objects for a frame!");
      return;
    }
  }
}

SwapchainImpl::~SwapchainImpl() {
  for (size_t i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    vkDestroySemaphore(Device, RenderFinishedSemaphores[i], nullptr);
    vkDestroySemaphore(Device, ImageAvailableSemaphores[i], nullptr);
    vkDestroyFence(Device, InFlightFences[i], nullptr);
  }

  for (auto framebuffer : SwapchainFramebuffers) {
    vkDestroyFramebuffer(Device, framebuffer, nullptr);
  }
  for (auto imageView : SwapchainImageViews) {
    vkDestroyImageView(Device, imageView, nullptr);
  }
  if (Swapchain) {
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
  }
}

bool SwapchainImpl::createImageViews(VkFormat imageFormat) {
  SwapchainImageViews.resize(SwapchainImages.size());
  for (size_t i = 0; i < SwapchainImages.size(); i++) {
    VkImageViewCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    createInfo.image = SwapchainImages[i];
    createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    createInfo.format = imageFormat;
    createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
    createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    createInfo.subresourceRange.baseMipLevel = 0;
    createInfo.subresourceRange.levelCount = 1;
    createInfo.subresourceRange.baseArrayLayer = 0;
    createInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(Device, &createInfo, nullptr,
                          &SwapchainImageViews[i]) != VK_SUCCESS) {
      LOGE("failed to create image views!");
      return false;
    }
  }
  return true;
}

bool SwapchainImpl::createFramebuffers(VkRenderPass renderPass) {
  SwapchainFramebuffers.resize(SwapchainImageViews.size());
  for (size_t i = 0; i < SwapchainImageViews.size(); i++) {
    VkImageView attachments[] = {SwapchainImageViews[i]};

    VkFramebufferCreateInfo framebufferInfo{};
    framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
    framebufferInfo.renderPass = renderPass;
    framebufferInfo.attachmentCount = 1;
    framebufferInfo.pAttachments = attachments;
    framebufferInfo.width = SwapchainExtent.width;
    framebufferInfo.height = SwapchainExtent.height;
    framebufferInfo.layers = 1;

    if (vkCreateFramebuffer(Device, &framebufferInfo, nullptr,
                            &SwapchainFramebuffers[i]) != VK_SUCCESS) {
      LOGE("failed to create framebuffer!");
      return false;
    }
  }
  return true;
}

std::shared_ptr<SwapchainImpl>
SwapchainImpl::create(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface,
                      VkDevice device, VkExtent2D imageExtent,
                      VkFormat imageFormat, VkRenderPass renderPass) {
  SwapChainSupportDetails swapChainSupport;
  if (!swapChainSupport.querySwapChainSupport(physicalDevice, surface)) {
    return {};
  }

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
      chooseSwapPresentMode(swapChainSupport.presentModes);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = imageExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  };

  auto indices = QueueFamilyIndices::findQueueFamilies(physicalDevice, surface);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  VkSwapchainKHR swapchain;
  if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapchain) !=
      VK_SUCCESS) {
    LOGE("failed to create swap chain!");
    return {};
  }

  auto impl = new SwapchainImpl(device, swapchain, imageExtent, imageFormat);
  auto ptr = std::shared_ptr<SwapchainImpl>(impl);

  if (!ptr->createImageViews(imageFormat)) {
    return {};
  }
  if (!ptr->createFramebuffers(renderPass)) {
    return {};
  }
  return ptr;
}

VkSurfaceFormatKHR SwapchainImpl::chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

std::tuple<uint32_t, uint32_t> SwapchainImpl::begin() {
  vkWaitForFences(Device, 1, &InFlightFences[CurrentFrame], VK_TRUE,
                  UINT64_MAX);
  vkResetFences(Device, 1, &InFlightFences[CurrentFrame]);

  uint32_t imageIndex;
  vkAcquireNextImageKHR(Device, Swapchain, UINT64_MAX,
                        ImageAvailableSemaphores[CurrentFrame], VK_NULL_HANDLE,
                        &imageIndex);
  return {imageIndex, CurrentFrame};
}

bool SwapchainImpl::end(uint32_t imageIndex, VkCommandBuffer commandBuffer,
                        VkQueue graphicsQueue, VkQueue presentQueue) {
  VkSemaphore waitSemaphores[] = {ImageAvailableSemaphores[CurrentFrame]};
  VkPipelineStageFlags waitStages[] = {
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
  VkSemaphore signalSemaphores[] = {RenderFinishedSemaphores[CurrentFrame]};
  VkSubmitInfo submitInfo{
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = waitSemaphores,
      .pWaitDstStageMask = waitStages,
      .commandBufferCount = 1,
      .pCommandBuffers = &commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = signalSemaphores,
  };

  if (vkQueueSubmit(graphicsQueue, 1, &submitInfo,
                    InFlightFences[CurrentFrame]) != VK_SUCCESS) {
    LOGE("failed to submit draw command buffer!");
    return false;
  }

  VkSwapchainKHR swapChains[] = {Swapchain};
  VkPresentInfoKHR presentInfo{
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = signalSemaphores,
      .swapchainCount = 1,
      .pSwapchains = swapChains,
      .pImageIndices = &imageIndex,
  };
  vkQueuePresentKHR(presentQueue, &presentInfo);

  CurrentFrame = (CurrentFrame + 1) % MAX_FRAMES_IN_FLIGHT;

  return true;
}
