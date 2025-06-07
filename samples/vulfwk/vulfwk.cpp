#include "vulfwk.h"
#include "vulfwk_pipeline.h"
#include <vko.h>

VulkanFramework::VulkanFramework(const char *appName, const char *engineName) {
  _appName = appName;
  _engineName = engineName;
  LOGI("*** VulkanFramewor::VulkanFramework ***");
}

VulkanFramework::~VulkanFramework() {
  LOGI("*** VulkanFramewor::~VulkanFramework ***");
  vkDeviceWaitIdle(Device);
}

bool VulkanFramework::initializeInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  Instance._validationLayers = layerNames;
  Instance._instanceExtensions = extensionNames;
  Instance._appInfo.pApplicationName = _appName.c_str();
  Instance._appInfo.pEngineName = _engineName.c_str();
  if (Instance.create() != VK_SUCCESS) {
    LOGE("failed to create instance!");
    return false;
  }
  return true;
}

bool VulkanFramework::initializeDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames,
    VkSurfaceKHR surface) {

  _picked = Instance.pickPhysicalDevice(surface);
  Surface = std::make_shared<vko::Surface>(Instance, surface,
                                           _picked._physicalDevice);

  Device._validationLayers = Instance._validationLayers;
  VK_CHECK(Device.create(_picked._physicalDevice, _picked._graphicsFamily,
                         _picked._presentFamily));

  Pipeline = PipelineImpl::create(_picked._physicalDevice, surface, Device,
                                  Surface->chooseSwapSurfaceFormat().format,
                                  AssetManager);
  if (!Pipeline) {
    return false;
  }

  SubmitCompleteFence = std::make_shared<vko::Fence>(Device, true);
  vkGetDeviceQueue(Device, _picked._graphicsFamily, 0, &_graphicsQueue);

  return true;
}

bool VulkanFramework::drawFrame(uint32_t width, uint32_t height) {

  if (!Swapchain) {
    vkDeviceWaitIdle(Device);

    Swapchain = std::make_shared<vko::Swapchain>(Device);
    Swapchain->create(_picked._physicalDevice, Surface->_surface,
                      Surface->chooseSwapSurfaceFormat(),
                      Surface->chooseSwapPresentMode(), _picked._graphicsFamily,
                      _picked._presentFamily);

    _images.clear();
    _images.resize(Swapchain->_images.size());
  }

  VkSemaphore semaphore;
  auto acquired = Swapchain->acquireNextImage(semaphore);

  auto image = _images[acquired.imageIndex];
  if (!image) {
    image = std::make_shared<vko::SwapchainFramebuffer>(
        Device, acquired.image, Swapchain->_createInfo.imageExtent,
        Swapchain->_createInfo.imageFormat, Pipeline->renderPass());
    _images[acquired.imageIndex] = image;
  }

  Pipeline->draw(acquired.commandBuffer, acquired.imageIndex,
                 image->_framebuffer, Swapchain->_createInfo.imageExtent);

  VkPipelineStageFlags waitDstStageMask = VK_PIPELINE_STAGE_TRANSFER_BIT;
  VkSubmitInfo submitInfo = {
      .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
      .waitSemaphoreCount = 1,
      .pWaitSemaphores = &semaphore,
      .pWaitDstStageMask = &waitDstStageMask,
      .commandBufferCount = 1,
      .pCommandBuffers = &acquired.commandBuffer,
      .signalSemaphoreCount = 1,
      .pSignalSemaphores = &acquired.submitCompleteSemaphore->_semaphore,
  };

  SubmitCompleteFence->reset();
  VK_CHECK(vkQueueSubmit(_graphicsQueue, 1, &submitInfo, *SubmitCompleteFence));

  SubmitCompleteFence->block();
  VK_CHECK(Swapchain->present(acquired.imageIndex));

  return true;
}
