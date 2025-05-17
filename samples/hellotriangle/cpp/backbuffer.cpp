#include "backbuffer.hpp"

Backbuffer::Backbuffer(VkDevice device, uint32_t i)
    : _device(device), index(i) {}

Backbuffer::~Backbuffer() {
  vkDestroyFramebuffer(_device, framebuffer, nullptr);
  vkDestroyImageView(_device, view, nullptr);
}
