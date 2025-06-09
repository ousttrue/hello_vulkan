#include "per_image_object.h"
#include "memory_allocator.h"
#include "types.h"
#include <stdexcept>

PerImageObject::PerImageObject(
    VkDevice device, VkImage swapchainImage, VkRenderPass renderPass,
    VkExtent2D extent, VkFormat format,
    const std::shared_ptr<BufferObject> &uniformBUffer)
    : _device(device),
      _framebuffer(device, swapchainImage, extent, format, renderPass),
      _renderPass(renderPass), _extent(extent), _uniformBuffer(uniformBUffer) {}

PerImageObject::~PerImageObject() {}

std::shared_ptr<PerImageObject>
PerImageObject::create(VkPhysicalDevice physicalDevice, VkDevice device,
                       uint32_t graphicsQueueFamilyIndex, uint32_t currentImage,
                       VkImage swapchainImage, VkExtent2D swapchainExtent,
                       VkFormat format, VkRenderPass renderPass) {
  auto memory = std::make_shared<MemoryAllocator>(physicalDevice, device,
                                                  graphicsQueueFamilyIndex);

  auto uniformBuffer = memory->createBuffer(
      nullptr, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  return std::make_shared<PerImageObject>(device, swapchainImage, renderPass,
                                          swapchainExtent, format,
                                          uniformBuffer);
}

void PerImageObject::bindTexture(VkImageView imageView, VkSampler sampler,
                                 VkDescriptorSet descriptorSet) {
  VkDescriptorBufferInfo bufferInfo{
      .buffer = _uniformBuffer->buffer(),
      .offset = 0,
      .range = sizeof(UniformBufferObject),
  };
  VkDescriptorImageInfo imageInfo{
      .sampler = sampler,
      .imageView = imageView,
      .imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
  };
  VkWriteDescriptorSet descriptorWrites[2] = {
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 0,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .pBufferInfo = &bufferInfo,
      },
      {
          .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET,
          .dstSet = descriptorSet,
          .dstBinding = 1,
          .dstArrayElement = 0,
          .descriptorCount = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .pImageInfo = &imageInfo,
      },
  };
  vkUpdateDescriptorSets(_device,
                         static_cast<uint32_t>(std::size(descriptorWrites)),
                         descriptorWrites, 0, nullptr);
}

void PerImageObject::updateUbo(float time, VkExtent2D extent) {
  UniformBufferObject ubo{};
  ubo.setTime(time, extent.width, extent.height);
  _uniformBuffer->copy(ubo);
}
