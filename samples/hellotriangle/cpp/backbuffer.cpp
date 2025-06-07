#include "backbuffer.hpp"
#include <vko.h>
#include <vulkan/vulkan_core.h>

Backbuffer::Backbuffer(uint32_t i, VkDevice device, VkImage image,
                       VkFormat format, VkExtent2D size,
                       VkRenderPass renderPass)
    : _index(i), _device(device) {

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
  vkDestroyFramebuffer(_device, _framebuffer, nullptr);
  vkDestroyImageView(_device, _view, nullptr);
}

VkSemaphore Backbuffer::beginFrame(VkCommandBuffer cmd,
                                   VkSemaphore acquireSemaphore) {
  VkSemaphore ret = _swapchainAcquireSemaphore;
  _swapchainAcquireSemaphore = acquireSemaphore;

  // We will only submit this once before it's recycled.
  VkCommandBufferBeginInfo beginInfo = {
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  };
  vkBeginCommandBuffer(cmd, &beginInfo);

  return ret;
}

VkResult Backbuffer::submit(VkQueue graphicsQueue, VkCommandBuffer cmd,
                            VkSemaphore semaphore, VkFence fence,
                            VkQueue presentationQueue,
                            VkSwapchainKHR swapchain) {

  VkSubmitInfo info = {VK_STRUCTURE_TYPE_SUBMIT_INFO};
  info.commandBufferCount = 1;
  info.pCommandBuffers = &cmd;

  const VkPipelineStageFlags waitStage =
      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  info.waitSemaphoreCount =
      _swapchainAcquireSemaphore != VK_NULL_HANDLE ? 1 : 0;
  info.pWaitSemaphores = &_swapchainAcquireSemaphore;
  info.pWaitDstStageMask = &waitStage;
  info.signalSemaphoreCount = 1;
  info.pSignalSemaphores = &semaphore;

  if (vkQueueSubmit(graphicsQueue, 1, &info, fence) != VK_SUCCESS) {
    LOGE("vkQueueSubmit");
    abort();
  }

  VkPresentInfoKHR present = {
      .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &semaphore,
      .swapchainCount = 1,
      .pSwapchains = &swapchain,
      .pImageIndices = &_index,
      .pResults = nullptr,
  };

  return vkQueuePresentKHR(presentationQueue, &present);
}
