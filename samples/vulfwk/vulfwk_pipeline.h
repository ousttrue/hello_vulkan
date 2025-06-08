#pragma once
#include <vulkan/vulkan.h>

#include <memory>
#include <vector>

class PipelineImpl {
  VkDevice Device;
  VkRenderPass RenderPass;
  VkPipelineLayout PipelineLayout;
  VkPipeline GraphicsPipeline;

public:
  PipelineImpl(VkDevice device, VkRenderPass renderPass,
               VkPipelineLayout pipelineLayout, VkPipeline graphicsPipeline);

  ~PipelineImpl();

  VkRenderPass renderPass() const { return RenderPass; }

  static std::shared_ptr<PipelineImpl> create(VkPhysicalDevice physicalDevice,
                                              VkDevice device,
                                              VkFormat swapchainImageFormat,
                                              void *AssetManager);

  static VkRenderPass createRenderPass(VkDevice device,
                                       VkFormat swapchainImageFormat);

  void draw(VkCommandBuffer commandBuffer, uint32_t imageIndex,
            VkFramebuffer framebuffer, VkExtent2D imageExtent);

  bool recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                           VkFramebuffer framebuffer, VkExtent2D imageExtent);
};
