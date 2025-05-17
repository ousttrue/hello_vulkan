#include "platform.hpp"
#include "common.hpp"
#include "device_manager.hpp"
#include "semaphore_manager.hpp"
#include "swapchain_manager.hpp"
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

MaliSDK::Result Platform::initVulkan(ANativeWindow *window) {
  std::vector<const char *> layers;
#ifdef NDEBUG
#else
  layers.push_back("VK_LAYER_KHRONOS_validation");
#endif

  _device = DeviceManager::create("Mali SDK", "Mali SDK", layers);
  if (!_device) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createSurfaceFromAndroid(window)) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  auto gpu = _device->selectGpu();
  if (!gpu) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  if (!_device->createLogicalDevice(layers)) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }

  semaphoreManager.reset(new SemaphoreManager(_device->Device));
  return MaliSDK::RESULT_SUCCESS;
}

std::shared_ptr<Platform> Platform::create(ANativeWindow *window) {
  auto ptr = std::shared_ptr<Platform>(new Platform);
  if (ptr->initVulkan(window) != MaliSDK::RESULT_SUCCESS) {
    LOGE("Failed to create Vulkan window.\n");
    abort();
  }
  return ptr;
}

VkDevice Platform::getDevice() const { return _device->Device; }

Platform::~Platform() {
  // Tear down backbuffers.
  // If our swapchain changes, we will call this, and create a new swapchain.
  // vkQueueWaitIdle(getGraphicsQueue());
  semaphoreManager = {};

  _swapchain = {};
}

const VkPhysicalDeviceMemoryProperties &Platform::getMemoryProperties() const {
  return _device->Selected.MemoryProperties;
}

MaliSDK::Result Platform::initSwapchain(VkRenderPass renderPass) {
  _swapchain = SwapchainManager::create(
      _device->Selected.Gpu, _device->Surface, _device->Device,
      _device->Selected.SelectedQueueFamilyIndex, _device->Queue,
      _device->Queue, renderPass, _swapchain ? _swapchain->Handle() : nullptr);
  if (!_swapchain) {
    return MaliSDK::RESULT_ERROR_GENERIC;
  }
  return MaliSDK::RESULT_SUCCESS;
}

MaliSDK::Result Platform::acquireNextImage(unsigned *image,
                                           VkRenderPass renderPass) {
  if (!_swapchain) {
    // Recreate swapchain.
    if (initSwapchain(renderPass) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  }

  auto device = _device->Device;
  auto queue = _device->Queue;
  auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
  VkResult res = vkAcquireNextImageKHR(device, _swapchain->Handle(), UINT64_MAX,
                                       acquireSemaphore, VK_NULL_HANDLE, image);

  if (res == VK_SUBOPTIMAL_KHR || res == VK_ERROR_OUT_OF_DATE_KHR) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);

    // Recreate swapchain.
    if (initSwapchain(renderPass) == MaliSDK::RESULT_SUCCESS)
      return MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN;
    else
      return MaliSDK::RESULT_ERROR_GENERIC;
  } else if (res != VK_SUCCESS) {
    vkQueueWaitIdle(queue);
    semaphoreManager->addClearedSemaphore(acquireSemaphore);
    return MaliSDK::RESULT_ERROR_GENERIC;
  } else {
    // Signal the underlying context that we're using this backbuffer now.
    // This will also wait for all fences associated with this swapchain image
    // to complete first.
    // When submitting command buffer that writes to swapchain, we need to wait
    // for this semaphore first.
    // Also, delete the older semaphore.
    auto oldSemaphore = _swapchain->beginFrame(*image, acquireSemaphore);

    // Recycle the old semaphore back into the semaphore manager.
    if (oldSemaphore != VK_NULL_HANDLE)
      semaphoreManager->addClearedSemaphore(oldSemaphore);

    return MaliSDK::RESULT_SUCCESS;
  }
}

std::shared_ptr<Backbuffer> Platform::getBackbuffer(VkRenderPass renderPass) {
  unsigned index;
  for (auto res = this->acquireNextImage(&index, renderPass); true;
       res = this->acquireNextImage(&index, renderPass)) {
    if (res == MaliSDK::RESULT_SUCCESS) {
      break;
    }
    if (res == MaliSDK::RESULT_ERROR_OUTDATED_SWAPCHAIN) {
      // Handle Outdated error in acquire.
      LOGE("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
      continue;
    }
    // error
    LOGE("Unrecoverable swapchain error.\n");
    return {};
  }

  // swapchain current backbuffer
  return _swapchain->getBackbuffer(index);
}
