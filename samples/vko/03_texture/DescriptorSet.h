#pragma once
#include <iterator>
#include <memory>
#include <stdexcept>
#include <vulkan/vulkan.h>

struct DescriptorSetLayout {
  VkDevice _device;
  VkDescriptorSetLayout _descriptorSetLayout = VK_NULL_HANDLE;

  VkDescriptorSetLayoutBinding bindings[2] = {
      {
          .binding = 0,
          .descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = 1,
          .stageFlags = VK_SHADER_STAGE_VERTEX_BIT,
          .pImmutableSamplers = nullptr,
      },
      {
          .binding = 1,
          .descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = 1,
          // indicate that we want to use the combined image sampler
          // descriptor in the fragment shader
          .stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT,
          .pImmutableSamplers = nullptr,
      },
  };

  VkDescriptorSetLayoutCreateInfo layoutInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
      .bindingCount = static_cast<uint32_t>(std::size(bindings)),
      .pBindings = bindings,
  };

  // VkDescriptorSetLayout descriptorSetLayout;
  DescriptorSetLayout(VkDevice device) : _device(device) {
    if (vkCreateDescriptorSetLayout(device, &layoutInfo, nullptr,
                                    &_descriptorSetLayout) != VK_SUCCESS) {
      throw std::runtime_error("failed to create descriptor set layout!");
    }
  }

  static std::shared_ptr<DescriptorSetLayout> create(VkDevice device) {
    auto ptr =
        std::shared_ptr<DescriptorSetLayout>(new DescriptorSetLayout(device));
    return ptr;
  }

  ~DescriptorSetLayout() {
    vkDestroyDescriptorSetLayout(_device, _descriptorSetLayout, nullptr);
  }
};
