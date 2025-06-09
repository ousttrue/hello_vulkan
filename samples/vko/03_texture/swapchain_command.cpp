#include "swapchain_command.h"
#include "memory_allocator.h"
#include "per_image_object.h"
#include "types.h"
#include <stdexcept>
#include <vulkan/vulkan_core.h>

SwapchainCommand::SwapchainCommand(VkPhysicalDevice physicalDevice,
                                   VkDevice device,
                                   uint32_t graphicsQueueFamilyIndex)
    : _physicalDevice(physicalDevice), _device(device),
      _graphicsQueueFamilyIndex(graphicsQueueFamilyIndex) {
  VkCommandPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
      .flags = 0,
      .queueFamilyIndex = graphicsQueueFamilyIndex,
  };
  if (vkCreateCommandPool(device, &poolInfo, nullptr, &_commandPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create command pool!");
  }
}

SwapchainCommand::~SwapchainCommand() {
  clearSwapchainDependent();

  vkDestroyCommandPool(_device, _commandPool, nullptr);
}

void SwapchainCommand::createSwapchainDependent(
    VkExtent2D swapchainExtent, uint32_t swapchainImageCount,
    VkDescriptorSetLayout descriptorSetLayout) {
  clearSwapchainDependent();

  _backbuffers.resize(swapchainImageCount);
  _commandBuffers.resize(swapchainImageCount);
  VkCommandBufferAllocateInfo allocInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
      .commandPool = _commandPool,
      .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
      .commandBufferCount = (uint32_t)_commandBuffers.size(),
  };
  if (vkAllocateCommandBuffers(_device, &allocInfo, _commandBuffers.data()) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to allocate command buffers!");
  }

  auto memory = std::make_shared<MemoryAllocator>(_physicalDevice, _device,
                                                  _graphicsQueueFamilyIndex);
  // this->createDescriptorPool(swapchainImageCount);
  VkDescriptorPoolSize poolSizes[] = {
      VkDescriptorPoolSize{
          .type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER,
          .descriptorCount = static_cast<uint32_t>(swapchainImageCount),
      },
      VkDescriptorPoolSize{
          .type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER,
          .descriptorCount = static_cast<uint32_t>(swapchainImageCount),
      },
  };
  VkDescriptorPoolCreateInfo poolInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
      // specifies the maximum number of descriptor sets that may be allocated
      .maxSets = static_cast<uint32_t>(swapchainImageCount),
      .poolSizeCount = static_cast<uint32_t>(std::size(poolSizes)),
      .pPoolSizes = poolSizes,
  };
  if (vkCreateDescriptorPool(_device, &poolInfo, nullptr, &_descriptorPool) !=
      VK_SUCCESS) {
    throw std::runtime_error("failed to create descriptor pool!");
  }

  std::vector<VkDescriptorSetLayout> layouts(swapchainImageCount,
                                             descriptorSetLayout);
  VkDescriptorSetAllocateInfo descriptorAllocInfo{
      .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
      .descriptorPool = _descriptorPool,
      .descriptorSetCount = swapchainImageCount,
      .pSetLayouts = layouts.data(),
  };
  _descriptorSets.resize(swapchainImageCount);
  if (vkAllocateDescriptorSets(_device, &descriptorAllocInfo,
                               _descriptorSets.data()) != VK_SUCCESS) {
    throw std::runtime_error("failed to allocate descriptor sets!");
  }
}

void SwapchainCommand::clearSwapchainDependent() {
  vkDeviceWaitIdle(_device);

  _backbuffers.clear();

  if (_commandBuffers.size() > 0) {
    vkFreeCommandBuffers(_device, _commandPool,
                         static_cast<uint32_t>(_commandBuffers.size()),
                         _commandBuffers.data());
    _commandBuffers.clear();
  }

  if (_descriptorPool != VK_NULL_HANDLE) {
    // The descriptor pool should be destroyed here since it relies on the
    // number of images
    vkDestroyDescriptorPool(_device, _descriptorPool, nullptr);
    _descriptorPool = VK_NULL_HANDLE;
  }
}

std::tuple<VkCommandBuffer, VkDescriptorSet, std::shared_ptr<PerImageObject>>
SwapchainCommand::frameResource(uint32_t currentImage) {
  return {
      _commandBuffers[currentImage],
      _descriptorSets[currentImage],
      _backbuffers[currentImage],
  };
}

std::shared_ptr<PerImageObject> SwapchainCommand::createFrameResource(
    uint32_t currentImage, VkImage swapchainImage, VkExtent2D swapchainExtent,
    VkFormat format, VkRenderPass renderPass) {
  auto memory = std::make_shared<MemoryAllocator>(_physicalDevice, _device,
                                                  _graphicsQueueFamilyIndex);

  auto uniformBuffer = memory->createBuffer(
      nullptr, sizeof(UniformBufferObject), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
      VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT |
          VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);

  auto backbuffer = std::make_shared<PerImageObject>(
      _device, swapchainImage, renderPass, swapchainExtent, format,
      _descriptorSets[currentImage], uniformBuffer);

  _backbuffers[currentImage] = backbuffer;

  return backbuffer;
}
