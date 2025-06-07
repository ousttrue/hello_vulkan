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
  vkDestroyFramebuffer(_device, _framebuffer, nullptr);
  vkDestroyImageView(_device, _view, nullptr);
}
