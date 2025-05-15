#pragma once
#include "platform.hpp"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

namespace MaliSDK {

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
  Platform *pContext;

  std::vector<Backbuffer> backbuffers;
  unsigned width, height;
  VkRenderPass renderPass;
  VkPipeline pipeline;
  VkPipelineCache pipelineCache;
  VkPipelineLayout pipelineLayout;
  Buffer vertexBuffer;

public:
  VulkanApplication() = default;
  ~VulkanApplication() = default;

  static std::shared_ptr<VulkanApplication> create(Platform *platform,
                                                   AAssetManager *assetManager);

  bool initialize(Platform *pContext);
  void updateSwapchain(AAssetManager *assetManager,
                       const std::vector<VkImage> &backbuffers,
                       const SwapchainDimensions &dim);
  void render(unsigned swapchainIndex, float deltaTime);
  void terminate();

private:
  Buffer createBuffer(const void *pInitial, size_t size, VkFlags usage);
  uint32_t findMemoryTypeFromRequirements(uint32_t deviceRequirements,
                                          uint32_t hostRequirements);

  void initRenderPass(VkFormat format);
  void termBackbuffers();

  void initVertexBuffer();
  void initPipeline(AAssetManager *assetManager);
};

} // namespace MaliSDK
