#include "swapchain_manager.hpp"
#include "common.hpp"
#include "device_manager.hpp"

SwapchainManager::~SwapchainManager() {
  vkDestroySwapchainKHR(Device, Swapchain, nullptr);
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
  ptr->swapchainDimensions.width = swapchainSize.width;
  ptr->swapchainDimensions.height = swapchainSize.height;
  ptr->swapchainDimensions.format = format.format;

  uint32_t imageCount;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
  ptr->swapchainImages.resize(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                   ptr->swapchainImages.data()));

  // Initialize per-frame resources.
  // Every swapchain image has its own command pool and fence manager.
  // This makes it very easy to keep track of when we can reset command buffers
  // and such.
  ptr->perFrame.clear();
  for (unsigned i = 0; i < ptr->swapchainImages.size(); i++)
    ptr->perFrame.emplace_back(new PerFrame(device, graphicsQueueIndex));
  ptr->setRenderingThreadCount(ptr->renderingThreadCount);

  // In case we're reinitializing the swapchain, terminate the old one first.
  ptr->backbuffers.clear();

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < ptr->swapchainImages.size(); ++i) {
    auto image = ptr->swapchainImages[i];
    auto backbuffer = std::make_shared<Backbuffer>(device, i);
    backbuffer->image = image;

    // Create an image view which we can render into.
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = ptr->swapchainDimensions.format;
    view.image = image;
    view.subresourceRange.baseMipLevel = 0;
    view.subresourceRange.baseArrayLayer = 0;
    view.subresourceRange.levelCount = 1;
    view.subresourceRange.layerCount = 1;
    view.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    view.components.r = VK_COMPONENT_SWIZZLE_R;
    view.components.g = VK_COMPONENT_SWIZZLE_G;
    view.components.b = VK_COMPONENT_SWIZZLE_B;
    view.components.a = VK_COMPONENT_SWIZZLE_A;

    VK_CHECK(vkCreateImageView(device, &view, nullptr, &backbuffer->view));

    // Build the framebuffer.
    VkFramebufferCreateInfo fbInfo = {
        VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
    fbInfo.renderPass = renderPass;
    fbInfo.attachmentCount = 1;
    fbInfo.pAttachments = &backbuffer->view;
    fbInfo.width = ptr->swapchainDimensions.width;
    fbInfo.height = ptr->swapchainDimensions.height;
    fbInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr,
                                 &backbuffer->framebuffer));

    ptr->backbuffers.push_back(backbuffer);
  }

  return ptr;
}

void SwapchainManager::submitCommandBuffer(VkCommandBuffer cmd,
                                           VkSemaphore acquireSemaphore,
                                           VkSemaphore releaseSemaphore) {
  // All queue submissions get a fence that CPU will wait
  // on for synchronization purposes.
  VkFence fence = perFrame[swapchainIndex]->fenceManager.requestClearedFence();

  VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmd;

  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  info.waitSemaphoreCount = acquireSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pWaitSemaphores = &acquireSemaphore;
  info.pWaitDstStageMask = &waitStage;
  info.signalSemaphoreCount = releaseSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pSignalSemaphores = &releaseSemaphore;

  VK_CHECK(vkQueueSubmit(GraphicsQueue, 1, &info, fence));
}

bool SwapchainManager::presentImage(unsigned index) {
  VkPresentInfoKHR present = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &perFrame[swapchainIndex]->swapchainReleaseSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &Swapchain,
      .pImageIndices = &index,
      .pResults = nullptr,
  };

  VkResult res = vkQueuePresentKHR(PresentationQueue, &present);
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
  if (perFrame[swapchainIndex]->swapchainReleaseSemaphore == VK_NULL_HANDLE &&
      perFrame[swapchainIndex]->swapchainAcquireSemaphore != VK_NULL_HANDLE) {
    VkSemaphore releaseSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    VK_CHECK(
        vkCreateSemaphore(Device, &semaphoreInfo, nullptr, &releaseSemaphore));
    perFrame[swapchainIndex]->setSwapchainReleaseSemaphore(releaseSemaphore);
  }

  submitCommandBuffer(cmd, perFrame[swapchainIndex]->swapchainAcquireSemaphore,
                      perFrame[swapchainIndex]->swapchainReleaseSemaphore);
}

VkCommandBuffer
SwapchainManager::beginRender(const std::shared_ptr<Backbuffer> &backbuffer) {
  // Request a fresh command buffer.
  VkCommandBuffer cmd =
      perFrame[swapchainIndex]->commandManager.requestCommandBuffer();

  return cmd;
}

void SwapchainManager::setRenderingThreadCount(unsigned count) {
  // vkQueueWaitIdle(_device->Queue);
  for (auto &pFrame : perFrame)
    pFrame->setSecondaryCommandManagersCount(count);
  renderingThreadCount = count;
}
