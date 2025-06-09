#include "vko.h"

namespace vko {

struct CommandBufferScope : public not_copyable {
  VkCommandBuffer commandBuffer;
  CommandBufferScope(VkCommandBuffer _commandBuffer, VkRenderPass renderPass,
                     VkFramebuffer framebuffer, VkExtent2D extent,
                     VkClearValue clearColor)
      : commandBuffer(_commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
    };
    VKO_CHECK(vkBeginCommandBuffer(this->commandBuffer, &beginInfo));

    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {.offset = {0, 0}, .extent = extent},
        .clearValueCount = 1,
        .pClearValues = &clearColor,
    };
    vkCmdBeginRenderPass(this->commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = (float)extent.width,
        .height = (float)extent.height,
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(this->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdSetScissor(this->commandBuffer, 0, 1, &scissor);
  }
  ~CommandBufferScope() {
    vkCmdEndRenderPass(this->commandBuffer);
    VKO_CHECK(vkEndCommandBuffer(this->commandBuffer));
  }
  void draw(VkPipeline pipeline, uint32_t vertexCount) {
    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);
    vkCmdDraw(this->commandBuffer, vertexCount, 1, 0, 0);
  }
};

} // namespace vko
