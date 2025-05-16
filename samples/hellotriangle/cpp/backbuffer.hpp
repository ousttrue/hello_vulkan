#pragma once
#include <vulkan/vulkan.h>

struct Backbuffer {
  uint32_t index;
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;

  Backbuffer(uint32_t i) : index(i) {}
};
