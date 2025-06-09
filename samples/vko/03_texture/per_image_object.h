#pragma once
#include <memory>
#include <vko/vko.h>
#include <vulkan/vulkan.h>

class PerImageObject {
  VkDevice _device;
  VkRenderPass _renderPass;
  vko::SwapchainFramebuffer _framebuffer;
  VkExtent2D _extent;
  VkDescriptorSet _descriptorSet;
  std::shared_ptr<class BufferObject> _uniformBuffer;

public:
  PerImageObject(VkDevice device, VkImage swapchainImage,
                 VkRenderPass renderPass, VkExtent2D swapchainExtent,
                 VkFormat format, VkDescriptorSet descriptorSet,
                 const std::shared_ptr<BufferObject> &uniformBuffer);
  ~PerImageObject();
  void bindTexture(VkImageView imageView, VkSampler sampler);
  void updateUbo(float time, VkExtent2D extent);
  VkFramebuffer framebuffer() const { return _framebuffer.framebuffer; }
};
