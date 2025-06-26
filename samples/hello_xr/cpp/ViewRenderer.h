#pragma once
#include <map>
#include <openxr/openxr.h>
#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/pipeline.h>

struct Cube {
  XrPosef pose;
  XrVector3f scale;
};

struct ViewRenderer {
  VkDevice device;
  VkQueue queue;
  vuloxr::vk::Pipeline pipeline;
  vuloxr::vk::DepthImage depthBuffer;
  std::map<VkImage, std::shared_ptr<vuloxr::vk::SwapchainFramebuffer>>
      framebufferMap;
  vuloxr::vk::Fence execFence;
  VkCommandPool commandPool = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;

  ViewRenderer(const vuloxr::vk::PhysicalDevice &physicalDevice,
               VkDevice _device, uint32_t queueFamilyIndex, VkExtent2D extent,
               VkFormat colorFormat, VkFormat depthFormat,
               VkSampleCountFlagBits sampleCountFlagBits,
               vuloxr::vk::Pipeline _pipeline);

  ~ViewRenderer();

  void render(VkImage image, VkExtent2D size, VkFormat colorFormat,
              VkFormat depthFormat, const VkClearColorValue &clearColor,
              VkBuffer vertices, VkBuffer indices, uint32_t drawCount,
              const XrPosef &hmdPose, XrFovf fov,
              const std::vector<Cube> &cubes);
};
