#pragma once
#include "platform.hpp"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

struct Backbuffer {
  VkImage image;
  VkImageView view;
  VkFramebuffer framebuffer;
};

struct Buffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
};

class VulkanApplication {
  VkDevice _device;
  MaliSDK::Platform *pContext;

  std::vector<Backbuffer> backbuffers;
  unsigned width, height;
  VkRenderPass renderPass;
  VkPipeline pipeline;
  VkPipelineCache pipelineCache;
  VkPipelineLayout pipelineLayout;
  Buffer vertexBuffer;

  VulkanApplication(VkDevice device, MaliSDK::Platform *platform);

public:
  ~VulkanApplication() = default;

  static std::shared_ptr<VulkanApplication> create(MaliSDK::Platform *platform,
                                                   AAssetManager *assetManager);

  void updateSwapchain(AAssetManager *assetManager,
                       const std::vector<VkImage> &backbuffers,
                       const MaliSDK::SwapchainDimensions &dim);
  void render(unsigned swapchainIndex, float deltaTime);
  void terminate();

private:
  Buffer createBuffer(const VkPhysicalDeviceMemoryProperties &props,
                      const void *pInitial, size_t size, VkFlags usage);

  void initRenderPass(VkFormat format);
  void termBackbuffers();

  void initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props);
  void initPipeline(AAssetManager *assetManager);
};
