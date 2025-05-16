#pragma once

#include <android_native_app_glue.h>
#include <vulkan/vulkan.h>

#include <memory>

struct Buffer {
  VkBuffer buffer;
  VkDeviceMemory memory;
};

class Pipeline {
  VkDevice _device;
  VkRenderPass _renderPass;
  VkPipelineLayout _pipelineLayout;
  VkPipeline _pipeline;
  VkPipelineCache _pipelineCache;

  Buffer vertexBuffer;

  Pipeline(VkDevice device, VkRenderPass renderPass,
           VkPipelineLayout pipelineLayout, VkPipeline pipeline,
           VkPipelineCache pipelineCache);

public:
  ~Pipeline();
  static std::shared_ptr<Pipeline> create(VkDevice device, VkFormat format,
                                          AAssetManager *assetManager);
  VkRenderPass renderPass() const { return _renderPass; }
  void render(VkCommandBuffer cmd, VkFramebuffer framebuffer, uint32_t width,
              uint32_t height);

  void initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props);
};
