#pragma once
#include "../vk.h"

namespace vuloxr {

namespace vk {

struct CommandSemahoreFence {
  VkQueue queue = VK_NULL_HANDLE;
  VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
  VkFence submitFence = VK_NULL_HANDLE;
  VkSemaphore submitSemaphore = VK_NULL_HANDLE;

  VkResult submit(VkSemaphore acquireSemaphore,
                  VkPipelineStageFlags waitDstStageMask =
                      VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT) const {
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        // wait swapchain image ready
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquireSemaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        // command
        .commandBufferCount = 1,
        .pCommandBuffers = &this->commandBuffer,
        // signal
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &this->submitSemaphore,
    };
    return vkQueueSubmit(this->queue, 1, &submitInfo, this->submitFence);
  }
};

struct CommandBufferPool : NonCopyable {
  VkDevice device;
  uint32_t queueFamilyIndex;
  VkCommandPool pool;
  std::vector<VkCommandBuffer> commandBuffers;
  std::vector<CommandSemahoreFence> commands;

  CommandBufferPool(VkDevice _device, uint32_t _queueFamilyIndex,
                    VkCommandPoolCreateFlags flags =
                        VK_COMMAND_POOL_CREATE_TRANSIENT_BIT |
                        VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT)
      : device(_device), queueFamilyIndex(_queueFamilyIndex) {
    VkCommandPoolCreateInfo commandPoolCreateInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = flags,
        .queueFamilyIndex = _queueFamilyIndex,
    };
    CheckVkResult(vkCreateCommandPool(this->device, &commandPoolCreateInfo,
                                      nullptr, &this->pool));
  }

  ~CommandBufferPool() {
    release();
    vkDestroyCommandPool(this->device, this->pool, nullptr);
  }

  void release() {
    for (auto &cmd : this->commands) {
      vkDestroyFence(this->device, cmd.submitFence, nullptr);
      vkDestroySemaphore(this->device, cmd.submitSemaphore, nullptr);
    }
    this->commands.clear();

    if (this->commandBuffers.size()) {
      vkFreeCommandBuffers(this->device, this->pool,
                           this->commandBuffers.size(),
                           this->commandBuffers.data());
    }
    this->commandBuffers.clear();
  }

  void reset(uint32_t poolSize) {
    release();

    this->commandBuffers.resize(poolSize);
    VkCommandBufferAllocateInfo commandBufferAllocateInfo = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = this->pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = poolSize,
    };
    CheckVkResult(vkAllocateCommandBuffers(
        this->device, &commandBufferAllocateInfo, this->commandBuffers.data()));

    this->commands.resize(poolSize);
    for (int i = 0; i < poolSize; ++i) {
      vkGetDeviceQueue(device, this->queueFamilyIndex, 0,
                       &this->commands[i].queue);
      this->commands[i].commandBuffer = this->commandBuffers[i];

      VkFenceCreateInfo fenceInfo = {
          .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
          .flags = VK_FENCE_CREATE_SIGNALED_BIT,
      };
      CheckVkResult(vkCreateFence(this->device, &fenceInfo, nullptr,
                                  &this->commands[i].submitFence));

      VkSemaphoreCreateInfo semaphoreInfo = {
          .sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO,
      };
      CheckVkResult(vkCreateSemaphore(this->device, &semaphoreInfo, nullptr,
                                      &this->commands[i].submitSemaphore));
    }
  }

  const CommandSemahoreFence &operator[](uint32_t index) const {
    return this->commands[index];
  }
};

struct CommandScope : NonCopyable {
  uint32_t queueFamilyIndex;
  VkCommandBuffer commandBuffer;

  CommandScope(uint32_t _queueFamilyIndex, VkCommandBuffer _commandBuffer,
               VkCommandBufferUsageFlags flags =
                   VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT)
      //       .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      : queueFamilyIndex(_queueFamilyIndex), commandBuffer(_commandBuffer) {
    VkCommandBufferBeginInfo beginInfo{
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .pNext = nullptr,
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

  void transitionDepthLayout(VkImage image, VkImageLayout oldLayout,
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

  void clearImage(VkClearColorValue clearColorValue, VkImage image,
                  const VkImageSubresourceRange &subResourceRange = {
                      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
                      .baseMipLevel = 0,
                      .levelCount = 1,
                      .baseArrayLayer = 0,
                      .layerCount = 1,
                  }) {
    VkImageMemoryBarrier presentToClearBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
        .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .srcQueueFamilyIndex = this->queueFamilyIndex,
        .dstQueueFamilyIndex = this->queueFamilyIndex,
        .image = image,
        .subresourceRange = subResourceRange,
    };
    vkCmdPipelineBarrier(this->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &presentToClearBarrier);

    vkCmdClearColorImage(this->commandBuffer, image,
                         VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue,
                         1, &subResourceRange);

    VkImageMemoryBarrier clearToPresentBarrier = {
        .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
        .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
        .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
        .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
        .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
        .srcQueueFamilyIndex = this->queueFamilyIndex,
        .dstQueueFamilyIndex = this->queueFamilyIndex,
        .image = image,
        .subresourceRange = subResourceRange,
    };
    vkCmdPipelineBarrier(this->commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &clearToPresentBarrier);
  }
};

} // namespace vk
} // namespace vuloxr
