#include "platform.hpp"
#include "common.hpp"
#include "device_manager.hpp"
#include <string.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

Backbuffer::~Backbuffer() {
  vkDestroyFramebuffer(_device, framebuffer, nullptr);
  vkDestroyImageView(_device, view, nullptr);
}

MaliSDK::Result Platform::initVulkan(ANativeWindow *window) {
  std::vector<const char *> layers;
#ifdef NDEBUG
#else
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  _device = DeviceManager::create("Mali SDK", "Mali SDK", layers);
  if (!_device) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createSurfaceFromAndroid(window)) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  auto gpu = _device->selectGpu();
  if (!gpu) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createLogicalDevice(layers)) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }

  VkSurfaceCapabilitiesKHR surfaceProperties;
  VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
      _device->Selected.Gpu, _device->Surface, &surfaceProperties));
  auto _res = initSwapchain(surfaceProperties);
  if (_res != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to init swapchain.");
    return _res;
  }

  _res = onPlatformUpdate();
  if (_res != MaliSDK::RESULT_SUCCESS) {
    return _res;
  }

  semaphoreManager = new MaliSDK::SemaphoreManager(_device->Device);
  return MaliSDK::RESULT_SUCCESS;
}

std::shared_ptr<Platform> Platform::create(ANativeWindow *window) {
  auto ptr = std::shared_ptr<Platform>(new Platform);
  if (ptr->initVulkan(window) != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to create Vulkan window.\n");
    abort();
  }
  return ptr;
}

VkDevice Platform::getDevice() const { return _device->Device; }

Platform::~Platform() {
  // Don't release anything until the GPU is completely idle.
  vkDeviceWaitIdle(_device->Device);

  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.
  // vkQueueWaitIdle(getGraphicsQueue());

  delete semaphoreManager;
  semaphoreManager = nullptr;

  destroySwapchain();

  // if (debug_callback) {
  //   vkDestroyDebugReportCallbackEXT(instance, debug_callback, nullptr);
  //   debug_callback = VK_NULL_HANDLE;
  // }
}

void Platform::destroySwapchain() {
  if (swapchain) {
    vkDestroySwapchainKHR(_device->Device, swapchain, nullptr);
    swapchain = VK_NULL_HANDLE;
  }
}

const VkPhysicalDeviceMemoryProperties &Platform::getMemoryProperties() const {
  return _device->Selected.MemoryProperties;
}

MaliSDK::Result
Platform::initSwapchain(const VkSurfaceCapabilitiesKHR &surfaceProperties) {
  VkSurfaceFormatKHR format = _device->getSurfaceFormat();
  if (format.format == VK_FORMAT_UNDEFINED) {
    LOGE("VK_FORMAT_UNDEFINED");
    abort();
  }

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

  VkSwapchainKHR oldSwapchain = swapchain;

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
      .surface = _device->Surface,
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

  auto device = _device->Device;
  VK_CHECK(vkCreateSwapchainKHR(device, &info, nullptr, &swapchain));

  if (oldSwapchain != VK_NULL_HANDLE)
    vkDestroySwapchainKHR(device, oldSwapchain, nullptr);

  swapchainDimensions.width = swapchainSize.width;
  swapchainDimensions.height = swapchainSize.height;
  swapchainDimensions.format = format.format;

  uint32_t imageCount;
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr));
  swapchainImages.resize(imageCount);
  VK_CHECK(vkGetSwapchainImagesKHR(device, swapchain, &imageCount,
                                   swapchainImages.data()));

  return MaliSDK::RESULT_SUCCESS;
}

MaliSDK::Result Platform::acquireNextImage(unsigned *image) {
  if (swapchain == VK_NULL_HANDLE) {
    // Recreate swapchain.
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        _device->Selected.Gpu, _device->Surface, &surfaceProperties));
    if (initSwapchain(surfaceProperties) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  }

  auto device = _device->Device;
  auto queue = _device->Queue;
  auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
  VkResult res = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX,
                                       acquireSemaphore, VK_NULL_HANDLE, image);

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);

    // Recreate swapchain.
    VkSurfaceCapabilitiesKHR surfaceProperties;
    VK_CHECK(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
        _device->Selected.Gpu, _device->Surface, &surfaceProperties));
    if (initSwapchain(surfaceProperties) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  } else if (res != VK_SUCCESS) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);
    return MaliSDK::RESULT_ERROR_GENERIC;
  } else {
    // Signal the underlying context that we're using this backbuffer now.
    // This will also wait for all fences associated with this swapchain image
    // to complete first.
    // When submitting command buffer that writes to swapchain, we need to wait
    // for this semaphore first.
    // Also, delete the older semaphore.
    auto oldSemaphore = beginFrame(*image, acquireSemaphore);

    // Recycle the old semaphore back into the semaphore manager.
    if (oldSemaphore != VK_NULL_HANDLE)
      semaphoreManager->addClearedSemaphore(oldSemaphore);

    return MaliSDK::RESULT_SUCCESS;
  }
}

MaliSDK::Result Platform::presentImage(unsigned index) {
  VkPresentInfoKHR present = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &perFrame[swapchainIndex]->swapchainReleaseSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &index,
      .pResults = nullptr,
  };

  VkResult res = vkQueuePresentKHR(_device->Queue, &present);
  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR)
    return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
  else if (res != VK_SUCCESS)
    return MaliSDK::RESULT_ERROR_GENERIC;
  else
    return MaliSDK::RESULT_SUCCESS;
}

void Platform::submitSwapchain(VkCommandBuffer cmd) {
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
    VK_CHECK(vkCreateSemaphore(_device->Device, &semaphoreInfo, nullptr,
                               &releaseSemaphore));
    perFrame[swapchainIndex]->setSwapchainReleaseSemaphore(releaseSemaphore);
  }

  submitCommandBuffer(cmd, perFrame[swapchainIndex]->swapchainAcquireSemaphore,
                      perFrame[swapchainIndex]->swapchainReleaseSemaphore);
}

void Platform::submitCommandBuffer(VkCommandBuffer cmd,
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

  VK_CHECK(vkQueueSubmit(_device->Queue, 1, &info, fence));
}

MaliSDK::Result Platform::onPlatformUpdate() {
  // waitIdle();

  // Initialize per-frame resources.
  // Every swapchain image has its own command pool and fence manager.
  // This makes it very easy to keep track of when we can reset command buffers
  // and such.
  perFrame.clear();
  for (unsigned i = 0; i < swapchainImages.size(); i++)
    perFrame.emplace_back(new MaliSDK::PerFrame(
        _device->Device, _device->Selected.SelectedQueueFamilyIndex));

  setRenderingThreadCount(renderingThreadCount);

  return MaliSDK::RESULT_SUCCESS;
}

VkCommandBuffer
Platform::beginRender(const std::shared_ptr<Backbuffer> &backbuffer) {
  // Request a fresh command buffer.
  VkCommandBuffer cmd =
      perFrame[swapchainIndex]->commandManager.requestCommandBuffer();

  return cmd;
}

void Platform::updateSwapchain(VkRenderPass renderPass) {
  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.
  vkQueueWaitIdle(_device->Queue);

  // In case we're reinitializing the swapchain, terminate the old one first.
  backbuffers.clear();

  auto device = _device->Device;

  // For all backbuffers in the swapchain ...
  for (uint32_t i = 0; i < swapchainImages.size(); ++i) {
    auto image = swapchainImages[i];
    auto backbuffer = std::make_shared<Backbuffer>(device, i);
    backbuffer->image = image;

    // Create an image view which we can render into.
    VkImageViewCreateInfo view = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
    view.viewType = VK_IMAGE_VIEW_TYPE_2D;
    view.format = swapchainDimensions.format;
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
    fbInfo.width = swapchainDimensions.width;
    fbInfo.height = swapchainDimensions.height;
    fbInfo.layers = 1;

    VK_CHECK(vkCreateFramebuffer(device, &fbInfo, nullptr,
                                 &backbuffer->framebuffer));

    backbuffers.push_back(backbuffer);
  }
}

void Platform::setRenderingThreadCount(unsigned count) {
  vkQueueWaitIdle(_device->Queue);
  for (auto &pFrame : perFrame)
    pFrame->setSecondaryCommandManagersCount(count);
  renderingThreadCount = count;
}
