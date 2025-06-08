#include "main_loop.h"
#include "vulfwk_pipeline.h"
#include <assert.h>

void main_loop(const std::function<bool()> &runLoop,
               const vko::Instance &instance, const vko::Surface &surface,
               vko::PhysicalDevice picked) {
  vko::Device Device;
  Device.validationLayers = instance.validationLayers;
  VKO_CHECK(Device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  auto Pipeline =
      PipelineImpl::create(picked.physicalDevice, Device,
                           surface.chooseSwapSurfaceFormat().format, nullptr);
  assert(Pipeline);

  auto semaphorePool = std::make_shared<vko::SemaphorePool>(Device);

  auto SubmitCompleteFence = std::make_shared<vko::Fence>(Device, true);

  auto _semaphorePool = std::make_shared<vko::SemaphorePool>(Device);
  std::shared_ptr<vko::Swapchain> Swapchain;
  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> images;
  while (runLoop()) {
    if (!Swapchain) {
      vkDeviceWaitIdle(Device);

      Swapchain = std::make_shared<vko::Swapchain>(Device);
      Swapchain->create(picked.physicalDevice, surface,
                        surface.chooseSwapSurfaceFormat(),
                        surface.chooseSwapPresentMode(),
                        picked.graphicsFamilyIndex, picked.presentFamilyIndex);

      images.clear();
      images.resize(Swapchain->images.size());
    }

    auto semaphore = semaphorePool->getOrCreateSemaphore();
    auto acquired = Swapchain->acquireNextImage(semaphore);

    auto image = images[acquired.imageIndex];
    if (!image) {
      image = std::make_shared<vko::SwapchainFramebuffer>(
          Device, acquired.image, Swapchain->createInfo.imageExtent,
          Swapchain->createInfo.imageFormat, Pipeline->renderPass());
      images[acquired.imageIndex] = image;
    }

    Pipeline->draw(acquired.commandBuffer, acquired.imageIndex,
                   image->framebuffer, Swapchain->createInfo.imageExtent);

    VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .waitSemaphoreCount = 1,
        .pWaitSemaphores = &semaphore,
        .pWaitDstStageMask = &waitDstStageMask,
        .commandBufferCount = 1,
        .pCommandBuffers = &acquired.commandBuffer,
        .signalSemaphoreCount = 1,
        .pSignalSemaphores = &acquired.submitCompleteSemaphore->semaphore,
    };

    SubmitCompleteFence->reset();
    VKO_CHECK(vkQueueSubmit(Device.graphicsQueue, 1, &submitInfo,
                            *SubmitCompleteFence));

    SubmitCompleteFence->block();
    semaphorePool->returnSemaphore(semaphore);

    VKO_CHECK(Swapchain->present(acquired.imageIndex));
  }

  vkDeviceWaitIdle(Device);
}
