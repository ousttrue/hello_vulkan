#include "main_loop.h"
#include "vulfwk_pipeline.h"
#include <assert.h>

void main_loop(const std::function<bool()> &runLoop,
               const vko::Surface &surface, vko::PhysicalDevice physicalDevice,
               const vko::Device &device) {

  auto Pipeline =
      PipelineImpl::create(physicalDevice.physicalDevice, device,
                           surface.chooseSwapSurfaceFormat().format, nullptr);
  assert(Pipeline);

  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex);

  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> images(
      swapchain.images.size());

  // auto semaphorePool = std::make_shared<vko::SemaphorePool>(Device);

  // auto SubmitCompleteFence = std::make_shared<vko::Fence>(Device, true);
  vko::FlightManager flightManager(device, physicalDevice.graphicsFamilyIndex,
                                   swapchain.images.size());

  // auto _semaphorePool = std::make_shared<vko::SemaphorePool>(Device);
  while (runLoop()) {
    auto acquireSemaphore = flightManager.getOrCreateSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);

    auto image = images[acquired.imageIndex];
    if (!image) {
      image = std::make_shared<vko::SwapchainFramebuffer>(
          device, acquired.image, swapchain.createInfo.imageExtent,
          swapchain.createInfo.imageFormat, Pipeline->renderPass());
      images[acquired.imageIndex] = image;
    }

    auto [cmd, flight, oldSemaphore] =
        flightManager.blockAndReset(acquireSemaphore);
    if (oldSemaphore != VK_NULL_HANDLE) {
      flightManager.reuseSemaphore(oldSemaphore);
    }

    Pipeline->draw(cmd, acquired.imageIndex, image->framebuffer,
                   swapchain.createInfo.imageExtent);

    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &acquireSemaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &flight.submitSemaphore,
    };
    VKO_CHECK(vkQueueSubmit(device.graphicsQueue, 1, &submitInfo,
                            flight.submitFence));

    VKO_CHECK(swapchain.present(acquired.imageIndex, flight.submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}
