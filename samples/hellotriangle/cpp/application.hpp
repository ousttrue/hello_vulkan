#pragma once
#include "platform.hpp"

namespace MaliSDK {

struct Backbuffer {
  // We get this image from the platform. Its memory is bound to the display or
  // window.
  VkImage image;
  // We need an image view to be able to access the image as a framebuffer.
  VkImageView view;
  // The actual framebuffer.
  VkFramebuffer framebuffer;
};

struct Buffer {
  VkBuffer buffer;
  // Buffer objects are backed by device memory.
  VkDeviceMemory memory;
};

class VulkanApplication {
  class AssetManager *pAsset;
  Platform *pContext;

  std::vector<Backbuffer> backbuffers;
  unsigned width, height;
  VkRenderPass renderPass;
  VkPipeline pipeline;
  VkPipelineCache pipelineCache;
  VkPipelineLayout pipelineLayout;
  Buffer vertexBuffer;

public:
  VulkanApplication(AssetManager *asset) : pAsset(asset) {}
  ~VulkanApplication() = default;
  bool isReady() const { return pContext != nullptr; }
  bool initialize(Platform *pContext);
  void updateSwapchain(const std::vector<VkImage> &backbuffers,
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
  void initPipeline();
};

} // namespace MaliSDK
