#pragma once
#include "../vk.h"

namespace vuloxr {

namespace vk {

struct RenderPassRecording : NonCopyable {
  VkCommandBuffer commandBuffer;

  struct ClearColor {
    float r, g, b, a;
  };

  RenderPassRecording(VkCommandBuffer _commandBuffer,
                      VkPipelineLayout pipelineLayout, VkRenderPass renderPass,
                      VkFramebuffer framebuffer, VkExtent2D extent,
                      const ClearColor &clearColor,
                      VkDescriptorSet descriptorSet = VK_NULL_HANDLE,
                      VkCommandBufferUsageFlags flags =
                          VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
      : commandBuffer(_commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
    CheckVkResult(vkBeginCommandBuffer(this->commandBuffer, &beginInfo));

    VkRenderPassBeginInfo renderPassInfo{
        .sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO,
        .renderPass = renderPass,
        .framebuffer = framebuffer,
        .renderArea = {.offset = {0, 0}, .extent = extent},
        .clearValueCount = 1,
        .pClearValues = reinterpret_cast<const VkClearValue *>(&clearColor),
    };
    vkCmdBeginRenderPass(this->commandBuffer, &renderPassInfo,
                         VK_SUBPASS_CONTENTS_INLINE);

    VkViewport viewport{
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };
    vkCmdSetViewport(this->commandBuffer, 0, 1, &viewport);

    VkRect2D scissor{
        .offset = {0, 0},
        .extent = extent,
    };
    vkCmdSetScissor(this->commandBuffer, 0, 1, &scissor);

    if (pipelineLayout != VK_NULL_HANDLE && descriptorSet != VK_NULL_HANDLE) {
      // take the descriptor set for the corresponding swap image, and bind it
      // to the descriptors in the shader
      vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                              pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
    }
  }

  ~RenderPassRecording() {
    vkCmdEndRenderPass(this->commandBuffer);
    CheckVkResult(vkEndCommandBuffer(this->commandBuffer));
  }

  void draw(VkPipeline pipeline, VkBuffer buffer, uint32_t vertexCount) {
    vkCmdBindPipeline(this->commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS,
                      pipeline);

    if (buffer != VK_NULL_HANDLE) {
      VkDeviceSize offset = 0;
      vkCmdBindVertexBuffers(this->commandBuffer, 0, 1, &buffer, &offset);
    }

    vkCmdDraw(this->commandBuffer, vertexCount, 1, 0, 0);
  }

  // void drawIndexed(const IndexedMesh &mesh) {
  //   if (mesh.vertexBuffer->buffer != VK_NULL_HANDLE) {
  //     VkBuffer vertexBuffers[] = {mesh.vertexBuffer->buffer};
  //     VkDeviceSize offsets[] = {0};
  //     vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
  //   }
  //   if (mesh.indexBuffer->buffer != VK_NULL_HANDLE) {
  //     vkCmdBindIndexBuffer(commandBuffer, mesh.indexBuffer->buffer, 0,
  //                          VK_INDEX_TYPE_UINT16);
  //   }
  //   vkCmdDrawIndexed(commandBuffer, mesh.indexDrawCount, 1, 0, 0, 0);
  // }

  void transitionLayout(VkImage image, VkImageLayout oldLayout,
                        VkImageLayout newLayout) {
    VkImageMemoryBarrier depthBarrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT,
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .image = image,
        .subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 1, 0, 1},
    };
    vkCmdPipelineBarrier(this->commandBuffer,
                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
                         VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &depthBarrier);
  }
};

struct CommandScope : NonCopyable {
  VkCommandBuffer commandBuffer;

  CommandScope(VkCommandBuffer _commandBuffer,
               VkCommandBufferUsageFlags flags =
                   VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
      : commandBuffer(_commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = flags,
    };
    CheckVkResult(vkBeginCommandBuffer(this->commandBuffer, &beginInfo));
  }

  ~CommandScope() { CheckVkResult(vkEndCommandBuffer(this->commandBuffer)); }

  const CommandScope &transitionImageLayout(VkImage image, VkFormat format,
                                            VkImageLayout oldLayout,
                                            VkImageLayout newLayout) const {

    VkImageMemoryBarrier barrier{
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        // can also use VK_IMAGE_LAYOUT_UNDEFINED if we don't care about the
        // existing contents of image!
        .oldLayout = oldLayout,
        .newLayout = newLayout,
        .srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED,
        .image = image,
        .subresourceRange =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .baseMipLevel = 0,
                .levelCount = 1,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
    };

    VkPipelineStageFlags sourceStage;
    VkPipelineStageFlags destinationStage;
    if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED &&
        newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL) {
      barrier.srcAccessMask = 0;
      barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

      sourceStage = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
      destinationStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    } else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL &&
               newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL) {
      barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
      barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

      sourceStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
      destinationStage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
    } else {
      throw std::invalid_argument("unsupported layout transition!");
    }

    vkCmdPipelineBarrier(commandBuffer, sourceStage, destinationStage, 0, 0,
                         nullptr, 0, nullptr, 1, &barrier);

    return *this;
  }

  const CommandScope &copyBufferToImage(VkBuffer buffer, VkImage image,
                                        uint32_t width, uint32_t height) const {
    VkBufferImageCopy region{
        .bufferOffset = 0,
        // functions as "padding" for the image destination
        .bufferRowLength = 0,
        .bufferImageHeight = 0,
        .imageSubresource =
            {
                .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                .mipLevel = 0,
                .baseArrayLayer = 0,
                .layerCount = 1,
            },
        .imageOffset = {0, 0, 0},
        .imageExtent = {width, height, 1},
    };
    vkCmdCopyBufferToImage(commandBuffer, buffer, image,
                           VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);

    return *this;
  }
};

} // namespace vk
} // namespace vuloxr
