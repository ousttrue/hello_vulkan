#include "../main_loop.h"

#include <assert.h>
#include <chrono>
#include <cmath>

static void clearImage(VkCommandBuffer commandBuffer,
                       VkClearColorValue clearColorValue, VkImage image,
                       uint32_t graphicsQueueFamilyIndex) {

  VkCommandBufferBeginInfo CommandBufferBeginInfo{
      .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
      .pNext = nullptr,
      .flags = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT,
      .pInheritanceInfo = nullptr,
  };
  VKO_CHECK(vkBeginCommandBuffer(commandBuffer, &CommandBufferBeginInfo));

  VkImageSubresourceRange subResourceRange = {
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };

  VkImageMemoryBarrier presentToClearBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_UNDEFINED,
      .newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .srcQueueFamilyIndex = graphicsQueueFamilyIndex,
      .dstQueueFamilyIndex = graphicsQueueFamilyIndex,
      .image = image,
      .subresourceRange = subResourceRange,
  };
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &presentToClearBarrier);

  VkImageSubresourceRange imageSubresourceRange{
      .aspectMask = VK_IMAGE_ASPECT_COLOR_BIT,
      .baseMipLevel = 0,
      .levelCount = 1,
      .baseArrayLayer = 0,
      .layerCount = 1,
  };
  vkCmdClearColorImage(commandBuffer, image,
                       VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColorValue,
                       1, &imageSubresourceRange);

  VkImageMemoryBarrier clearToPresentBarrier = {
      .sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER,
      .srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT,
      .dstAccessMask = VK_ACCESS_MEMORY_READ_BIT,
      .oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
      .newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
      .srcQueueFamilyIndex = graphicsQueueFamilyIndex,
      .dstQueueFamilyIndex = graphicsQueueFamilyIndex,
      .image = image,
      .subresourceRange = subResourceRange,
  };
  vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
                       VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0,
                       nullptr, 1, &clearToPresentBarrier);

  VKO_CHECK(vkEndCommandBuffer(commandBuffer));
}

VkClearColorValue getColorForTime(std::chrono::nanoseconds nano) {
  // auto sec = std::chrono::duration_cast<std::chrono::seconds>(nano);
  auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(nano);
  const auto SPD = 3.0f;
  float v = (std::sin(sec.count() * SPD) + 1.0f) * 0.5;
  // std::cout << nano << ": " << std::fmod(sec.count(), 1.0) << ": "
  //           << std::sin(sec.count()) << ": " << v << std::endl;
  return {v, 0.0, 0.0, 0.0};
}

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {

  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex);

  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    VKO_CHECK(acquired.result);

    auto [cmd, flight, oldSemaphore] =
        flightManager.blockAndReset(acquireSemaphore);
    if (oldSemaphore != VK_NULL_HANDLE) {
      flightManager.reuseSemaphore(oldSemaphore);
    }

    {
      auto color =
          getColorForTime(std::chrono::nanoseconds(acquired.presentTimeNano));
      clearImage(cmd, color, acquired.image,
                 physicalDevice.graphicsFamilyIndex);

      VKO_CHECK(device.submit(cmd, acquireSemaphore, flight.submitSemaphore,
                              flight.submitFence));
    }

    VKO_CHECK(swapchain.present(acquired.imageIndex, flight.submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}
