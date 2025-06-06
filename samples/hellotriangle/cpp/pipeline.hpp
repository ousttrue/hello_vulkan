#pragma once

#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>

#include <memory>

class Pipeline {
  VkDevice _device;
  VkRenderPass _renderPass;
  VkPipelineLayout _pipelineLayout;
  VkPipeline _pipeline;
  VkPipelineCache _pipelineCache;

  std::shared_ptr<class Buffer> _vertexBuffer;

  Pipeline(VkDevice device, VkRenderPass renderPass,
           VkPipelineLayout pipelineLayout, VkPipeline pipeline,
           VkPipelineCache pipelineCache);

public:
  ~Pipeline();
  static std::shared_ptr<Pipeline> create(VkPhysicalDevice physicalDevice,
                                          VkDevice device, VkFormat format,
                                          AAssetManager *assetManager);
  VkRenderPass renderPass() const { return _renderPass; }
  void render(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D size);

  void initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props);
};
