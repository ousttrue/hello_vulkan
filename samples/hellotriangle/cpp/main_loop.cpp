#include "main_loop.h"
#include "pipeline.hpp"
#include "vko/vko.h"
#include <vulkan/vulkan_core.h>

static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    vko::Logger::Error("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device, void *p) {

  std::shared_ptr<class Pipeline> pipeline = Pipeline::create(
      physicalDevice.physicalDevice, device,
      surface.chooseSwapSurfaceFormat().format, (AAssetManager *)p);

  vko::Swapchain swapchain(device);
  VKO_CHECK(swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex));

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers(
      swapchain.images.size());
  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  unsigned frameCount = 0;
  auto startTime = getCurrentTime();
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    if (acquired.result == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            surface.chooseSwapSurfaceFormat().format, pipeline->renderPass());
        backbuffers[acquired.imageIndex] = backbuffer;
      }

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [cmd, flight, oldSemaphore] =
          flightManager.blockAndReset(acquireSemaphore);
      if (oldSemaphore != VK_NULL_HANDLE) {
        flightManager.reuseSemaphore(oldSemaphore);
      }

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
      VKO_CHECK(
          vkQueueSubmit(device.graphicsQueue, 1, &info, flight.submitFence));

      VkPresentInfoKHR present = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &flight.submitSemaphore,
          .swapchainCount = 1,
          .pSwapchains = &swapchain.swapchain,
          .pImageIndices = &acquired.imageIndex,
          .pResults = nullptr,
      };
      VKO_CHECK(vkQueuePresentKHR(device.presentQueue, &present));

    } else if (acquired.result == VK_SUBOPTIMAL_KHR ||
               acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
      vko::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      vkQueueWaitIdle(device.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      // TODO:
      // swapchain = {};
      // return true;

    } else {
      // error ?
      vko::Logger::Error("Unrecoverable swapchain error.\n");
      vkQueueWaitIdle(device.presentQueue);
      flightManager.reuseSemaphore(acquireSemaphore);
      return;
    }

    frameCount++;
    if (frameCount == 100) {
      double endTime = getCurrentTime();
      vko::Logger::Info("FPS: %.3f\n", frameCount / (endTime - startTime));
      frameCount = 0;
      startTime = endTime;
    }
  }

  vkDeviceWaitIdle(device);
}
