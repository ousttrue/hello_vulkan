#pragma once
#include <vulkan/vulkan.h>

#include <vector>
#include <memory>

class PipelineImpl {
  VkDevice Device;
  VkRenderPass RenderPass;
  VkPipelineLayout PipelineLayout;
  VkPipeline GraphicsPipeline;

  VkCommandPool CommandPool;
  std::vector<VkCommandBuffer> CommandBuffers;

public:
  PipelineImpl(VkDevice device, VkRenderPass renderPass,
               VkPipelineLayout pipelineLayout, VkPipeline graphicsPipeline);

  ~PipelineImpl();

  VkRenderPass renderPass() const { return RenderPass; }

  static std::shared_ptr<PipelineImpl>
  create(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device,
         VkFormat swapchainImageFormat, void *AssetManager) ;

  static VkRenderPass createRenderPass(VkDevice device,
                                       VkFormat swapchainImageFormat) ;

  bool createCommandPool(VkPhysicalDevice physicalDevice,
                         VkSurfaceKHR surface) ;

  bool createCommandBuffers() ;

  VkCommandBuffer draw(uint32_t imageIndex, uint32_t currentFrame,
                       VkFramebuffer framebuffer, VkExtent2D extent) ;

  bool recordCommandBuffer(VkCommandBuffer commandBuffer, uint32_t imageIndex,
                           VkFramebuffer framebuffer, VkExtent2D imageExtent) ;
};
