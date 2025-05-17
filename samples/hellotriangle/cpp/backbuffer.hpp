#pragma once
#include <vulkan/vulkan.h>

struct Backbuffer {
  VkDevice _device;
  uint32_t index;
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;

  Backbuffer(VkDevice device, uint32_t i);
  ~Backbuffer();
};
