#include "../main_loop.h"
#include "pipeline.hpp"
#include "vko/vko.h"
#include <chrono>

void main_loop(const std::function<bool()> &runLoop,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device) {

  std::shared_ptr<class Pipeline> pipeline = Pipeline::create(
      physicalDevice.physicalDevice, device, swapchain.createInfo.imageFormat);

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  unsigned frameCount = 0;
  auto startTime = std::chrono::high_resolution_clock::now();
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    if (acquired.result == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            swapchain.createInfo.imageFormat, pipeline->renderPass());
        backbuffers[acquired.imageIndex] = backbuffer;
      }

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [cmd, flight] = flightManager.sync(acquireSemaphore);

      // We will only submit this once before it's recycled.
      VkCommandBufferBeginInfo beginInfo = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      vkBeginCommandBuffer(cmd, &beginInfo);

      // Signal the underlying context that we're using this backbuffer now.
      // This will also wait for all fences associated with this swapchain
      // image to complete first. When submitting command buffer that writes
      // to swapchain, we need to wait for this semaphore first. Also, delete
      // the older semaphore. auto oldSemaphore = backbuffer->beginFrame(cmd,
      // acquireSemaphore);
      pipeline->render(cmd, backbuffer->framebuffer,
                       swapchain.createInfo.imageExtent);

      const VkPipelineStageFlags waitStage =
          VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
      VkSubmitInfo info = {
          .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
          .waitSemaphoreCount =
              static_cast<uint32_t>(acquireSemaphore != VK_NULL_HANDLE ? 1 : 0),
          .pWaitSemaphores = &acquireSemaphore,
          .pWaitDstStageMask = &waitStage,
          .commandBufferCount = 1,
          .pCommandBuffers = &cmd,
          .signalSemaphoreCount = 1,
          .pSignalSemaphores = &flight.submitSemaphore,
      };
      vko::VKO_CHECK(vkQueueSubmit(device.queue, 1, &info, flight.submitFence));

      VkPresentInfoKHR present = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &flight.submitSemaphore,
          .swapchainCount = 1,
          .pSwapchains = &swapchain.swapchain,
          .pImageIndices = &acquired.imageIndex,
          .pResults = nullptr,
      };
      vko::VKO_CHECK(vkQueuePresentKHR(device.queue, &present));

    } else if (acquired.result == VK_SUBOPTIMAL_KHR ||
               acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
      vko::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      // TODO:
      // swapchain = {};
      // return true;

    } else {
      // error ?
      vko::Logger::Error("Unrecoverable swapchain error.\n");
      vkQueueWaitIdle(swapchain.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      return;
    }

    frameCount++;
    if (frameCount == 1000) {
      auto endTime = std::chrono::high_resolution_clock::now();
      // vko::Logger::Info(
      //     "FPS: %.3f\n",
      //     (1000.0f * frameCount) /
      //         std::chrono::duration_cast<std::chrono::milliseconds>(endTime -
      //                                                               startTime)
      //             .count());
      frameCount = 0;
      startTime = endTime;
    }
  }

  vkDeviceWaitIdle(device);
}
