#pragma once
#include <memory>
#include <vko/vko.h>
#include <vulkan/vulkan.h>

class PerImageObject {
  VkDevice _device;
  VkRenderPass _renderPass;
  vko::SwapchainFramebuffer _framebuffer;
  VkExtent2D _extent;
  std::shared_ptr<class BufferObject> _uniformBuffer;

public:
  PerImageObject(VkDevice device, VkImage swapchainImage,
                 VkRenderPass renderPass, VkExtent2D swapchainExtent,
                 VkFormat format,
                 const std::shared_ptr<BufferObject> &uniformBuffer);
  ~PerImageObject();

  static std::shared_ptr<PerImageObject>
  create(VkPhysicalDevice physicalDevice, VkDevice device,
         uint32_t graphicsQueueFamilyIndex, uint32_t currentImage,
         VkImage swapchainImage, VkExtent2D swapchainExtent, VkFormat format,
         VkRenderPass renderPass);

  void bindTexture(VkImageView imageView, VkSampler sampler,
                   VkDescriptorSet descriptorSet);
  void updateUbo(float time, VkExtent2D extent);
  VkFramebuffer framebuffer() const { return _framebuffer.framebuffer; }
};
