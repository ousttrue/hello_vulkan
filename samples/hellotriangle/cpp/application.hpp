#pragma once
#include "platform.hpp"

#include <android/asset_manager.h>
#include <android_native_app_glue.h>

#include <memory>
#include <vulkan/vulkan_core.h>

class VulkanApplication {
  VkDevice _device;
  MaliSDK::Platform *pContext;

public:
  std::vector<std::shared_ptr<Backbuffer>> backbuffers;

private:
  unsigned width, height;

  VulkanApplication(VkDevice device, MaliSDK::Platform *platform);

public:
  ~VulkanApplication() = default;

  static std::shared_ptr<VulkanApplication> create(MaliSDK::Platform *platform,
                                                   VkFormat format,
                                                   AAssetManager *assetManager);

  void updateSwapchain(const std::vector<VkImage> &backbuffers,
                       const MaliSDK::SwapchainDimensions &dim,
                       VkRenderPass renderPass);
  VkCommandBuffer beginRender(const std::shared_ptr<Backbuffer> &backbuffer,
                              uint32_t width, uint32_t height);
  void terminate();

private:
  void termBackbuffers();
  void initPipeline(VkFormat format, AAssetManager *assetManager);
};
