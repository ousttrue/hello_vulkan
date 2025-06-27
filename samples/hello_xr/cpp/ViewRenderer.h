#pragma once
#include <map>
#include <openxr/openxr.h>
#include <span>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/swapchain.h>

struct Cube {
  XrPosef pose;
  XrVector3f scale;
};

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  vuloxr::vk::Pipeline pipeline;
  VkCommandPool commandPool = VK_NULL_HANDLE;

  struct RenderTarget {
    std::shared_ptr<vuloxr::vk::SwapchainFramebuffer> framebuffer;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    vuloxr::vk::Fence execFence;
  };
  std::map<VkImage, RenderTarget> framebufferMap;

  ViewRenderer(VkDevice _device, uint32_t queueFamilyIndex,
               VkFormat colorFormat, VkFormat depthFormat,
               std::span<const VkVertexInputBindingDescription>
                   vertexInputBindingDescriptions = {},
               std::span<const VkVertexInputAttributeDescription>
                   vertexInputAttributeDescriptions = {});
  ~ViewRenderer();
  void render(const vuloxr::vk::PhysicalDevice &physicalDevice, VkImage image,
              VkExtent2D extent, VkFormat colorFormat, VkFormat depthFormat,
              VkSampleCountFlagBits sampleCountFlagBits,
              const VkClearColorValue &clearColor, VkBuffer vertices,
              VkBuffer indices, uint32_t drawCount, const XrPosef &hmdPose,
              XrFovf fov, const std::vector<Cube> &cubes);
};
