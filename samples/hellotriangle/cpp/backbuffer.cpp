#include "backbuffer.hpp"
#include "common.hpp"

Backbuffer::Backbuffer(uint32_t i, VkDevice device, uint32_t graphicsQueueIndex,
                       VkImage image, VkFormat format, VkExtent2D size,
                       VkRenderPass renderPass)
    : _index(i), _device(device), _graphicsQueueIndex(graphicsQueueIndex),
      _fenceManager(device),
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

  VK_CHECK(vkCreateImageView(_device, &view, nullptr, &_view));

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

  VK_CHECK(vkCreateFramebuffer(_device, &fbInfo, nullptr, &_framebuffer));
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

void Backbuffer::setSecondaryCommandManagersCount(unsigned count) {
  _secondaryCommandManagers.clear();
  for (unsigned i = 0; i < count; i++) {
    _secondaryCommandManagers.emplace_back(new CommandBufferManager(
        _device, VK_COMMAND_BUFFER_LEVEL_SECONDARY, _graphicsQueueIndex));
  }
}

VkSemaphore
Backbuffer::setSwapchainAcquireSemaphore(VkSemaphore acquireSemaphore) {
  VkSemaphore ret = _swapchainAcquireSemaphore;
  _swapchainAcquireSemaphore = acquireSemaphore;
  return ret;
}

void Backbuffer::setSwapchainReleaseSemaphore(VkSemaphore releaseSemaphore) {
  if (_swapchainReleaseSemaphore != VK_NULL_HANDLE) {
    vkDestroySemaphore(_device, _swapchainReleaseSemaphore, nullptr);
  }
  _swapchainReleaseSemaphore = releaseSemaphore;
}

void Backbuffer::beginFrame() {
  _fenceManager.beginFrame();
  _commandManager.beginFrame();
  for (auto &pManager : _secondaryCommandManagers) {
    pManager->beginFrame();
  }
}
