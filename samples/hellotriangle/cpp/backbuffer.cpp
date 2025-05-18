#include "backbuffer.hpp"
#include "logger.hpp"
#include <vulkan/vulkan_core.h>

Backbuffer::Backbuffer(uint32_t i, VkDevice device, uint32_t graphicsQueueIndex,
                       VkImage image, VkFormat format, VkExtent2D size,
                       VkRenderPass renderPass)
    : _index(i), _device(device), _fenceManager(device),
      _commandManager(device, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
                      graphicsQueueIndex) {

  // Create an image view which we can render into.
  VkImageViewCreateInfo view = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO,
      .image = image,
      .viewType = VK_IMAGE_VIEW_TYPE_2D,
      .format = format,
      .components = {.r = VK_COMPONENT_SWIZZLE_R,
                     .g = VK_COMPONENT_SWIZZLE_G,
                     .b = VK_COMPONENT_SWIZZLE_B,
                     .a = VK_COMPONENT_SWIZZLE_A},
      .subresourceRange = {.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                           .baseMipLevel = 0,
                           .levelCount = 1,
                           .baseArrayLayer = 0,
                           .layerCount = 1},
  };

  if (vkCreateImageView(_device, &view, nullptr, &_view) != VK_SUCCESS) {
    LOGE("vkCreateImageView");
    abort();
  }

  // Build the framebuffer.
  VkFramebufferCreateInfo fbInfo = {
      .sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO,
      .renderPass = renderPass,
      .attachmentCount = 1,
      .pAttachments = &_view,
      .width = size.width,
      .height = size.height,
      .layers = 1,
  };

  if (vkCreateFramebuffer(_device, &fbInfo, nullptr, &_framebuffer) !=
      VK_SUCCESS) {
    LOGE("vkCreateFramebuffer");
    abort();
  }
}

Backbuffer::~Backbuffer() {
  if (_swapchainAcquireSemaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(_device, _swapchainAcquireSemaphore, nullptr);
  }
  if (_swapchainReleaseSemaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(_device, _swapchainReleaseSemaphore, nullptr);
  }
  vkDestroyFramebuffer(_device, _framebuffer, nullptr);
  vkDestroyImageView(_device, _view, nullptr);
}

std::tuple<VkSemaphore, VkCommandBuffer>
Backbuffer::beginFrame(VkSemaphore acquireSemaphore) {
  VkSemaphore ret = _swapchainAcquireSemaphore;
  _fenceManager.beginFrame();
  _commandManager.beginFrame();
  _swapchainAcquireSemaphore = acquireSemaphore;
  // return ret;
  // Request a fresh command buffer.
  auto cmd = _commandManager.requestCommandBuffer();

  // We will only submit this once before it's recycled.
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &beginInfo);

  // return cmd;
  return {ret, cmd};
}

VkResult Backbuffer::endFrame(VkQueue graphicsQueue, VkCommandBuffer cmd,
                              VkQueue presentationQueue,
                              VkSwapchainKHR swapchain) {
  // For the first frames, we will create a release semaphore.
  // This can be reused every frame. Semaphores are reset when they have been
  // successfully been waited on.
  // If we aren't using acquire semaphores, we aren't using release semaphores
  // either.
  if (_swapchainReleaseSemaphore == VK_NULL_HANDLE &&
      _swapchainAcquireSemaphore != VK_NULL_HANDLE) {
    VkSemaphore releaseSemaphore;
    VkSemaphoreCreateInfo semaphoreInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                          &releaseSemaphore) != VK_SUCCESS) {
      LOGE("vkCreateSemaphore");
      abort();
    }
    // setSwapchainReleaseSemaphore(releaseSemaphore);
    if (_swapchainReleaseSemaphore != VK_NULL_HANDLE) {
      vkDestroySemaphore(_device, _swapchainReleaseSemaphore, nullptr);
    }
    _swapchainReleaseSemaphore = releaseSemaphore;
  }

  // All queue submissions get a fence that CPU will wait
  // on for synchronization purposes.
  VkFence fence =
      // _backbuffers[_swapchainIndex]->
      _fenceManager.requestClearedFence();

  VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmd;

  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  info.waitSemaphoreCount =
      _swapchainAcquireSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pWaitSemaphores = &_swapchainAcquireSemaphore;
  info.pWaitDstStageMask = &waitStage;
  info.signalSemaphoreCount =
      _swapchainReleaseSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pSignalSemaphores = &_swapchainReleaseSemaphore;

  if (vkQueueSubmit(graphicsQueue, 1, &info, fence) != VK_SUCCESS) {
    LOGE("vkQueueSubmit");
    abort();
  }

  VkPresentInfoKHR present = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &_swapchainReleaseSemaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &_index,
      .pResults = nullptr,
  };

  return vkQueuePresentKHR(presentationQueue, &present);
}
