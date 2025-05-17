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
  static std::shared_ptr<Pipeline>
  create(VkDevice device, VkFormat format, AAssetManager *assetManager,
         const VkPhysicalDeviceMemoryProperties &props);
  VkRenderPass renderPass() const { return _renderPass; }
  void render(VkCommandBuffer cmd, VkFramebuffer framebuffer, VkExtent2D size);

  void initVertexBuffer(const VkPhysicalDeviceMemoryProperties &props);
};
