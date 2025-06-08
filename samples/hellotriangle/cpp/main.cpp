#include "userdata.h"

#include "pipeline.hpp"
#include <assert.h>
#include <vko.h>

auto APP_NAME = "hellotriangle";

class SemaphoreManager {
  VkDevice _device = VK_NULL_HANDLE;
  std::vector<VkSemaphore> _recycledSemaphores;

public:
  SemaphoreManager(VkDevice device) : _device(device) {}
  ~SemaphoreManager() {
    for (auto &semaphore : _recycledSemaphores) {
      vkDestroySemaphore(_device, semaphore, nullptr);
    }
  }
  VkSemaphore getClearedSemaphore() {
    if (_recycledSemaphores.empty()) {
      VkSemaphoreCreateInfo info = {VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
      VkSemaphore semaphore;
      if (vkCreateSemaphore(_device, &info, nullptr, &semaphore) !=
          VK_SUCCESS) {
        vko::Logger::Error("vkCreateSemaphore");
        abort();
      }
      return semaphore;
    } else {
      auto semaphore = _recycledSemaphores.back();
      _recycledSemaphores.pop_back();
      return semaphore;
    }
  }
  void addClearedSemaphore(VkSemaphore semaphore) {
    _recycledSemaphores.push_back(semaphore);
  }
};

class FlightManager {
  VkDevice _device = VK_NULL_HANDLE;
  VkFence _submitFence = VK_NULL_HANDLE;
  VkSemaphore _submitSemaphore = VK_NULL_HANDLE;

  VkCommandBufferLevel _commandBufferLevel;
  unsigned _commandCount = 0;

  VkCommandPool _pool = VK_NULL_HANDLE;
  std::vector<VkCommandBuffer> _buffers;

  VkSemaphore _swapchainAcquireSemaphore = VK_NULL_HANDLE;

public:
  FlightManager(VkDevice device, VkCommandBufferLevel bufferLevel,
                uint32_t graphicsQueueIndex)
      : _device(device), _commandBufferLevel(bufferLevel) {
    VkCommandPoolCreateInfo info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT,
        .queueFamilyIndex = graphicsQueueIndex,
    };
    if (vkCreateCommandPool(_device, &info, nullptr, &_pool) != VK_SUCCESS) {
      vko::Logger::Error("vkCreateCommandPool");
      abort();
    }

    VkFenceCreateInfo fenceInfo = {
        .sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO,
    };
    if (vkCreateFence(_device, &fenceInfo, nullptr, &_submitFence) !=
        VK_SUCCESS) {
      vko::Logger::Error("vkCreateFence");
      abort();
    };

    VkSemaphoreCreateInfo semaphoreInfo = {
        VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO};
    if (vkCreateSemaphore(_device, &semaphoreInfo, nullptr,
                          &_submitSemaphore) != VK_SUCCESS) {
      vko::Logger::Error("vkCreateSemaphore");
      abort();
    }
  }
  ~FlightManager() {
    vkWaitForFences(_device, 1, &_submitFence, true, UINT64_MAX);
    vkDestroyFence(_device, _submitFence, nullptr);
    vkDestroySemaphore(_device, _submitSemaphore, nullptr);

    if (!_buffers.empty()) {
      vkFreeCommandBuffers(_device, _pool, _buffers.size(), _buffers.data());
    }
    vkDestroyCommandPool(_device, _pool, nullptr);
  }

  std::tuple<VkSemaphore, VkCommandBuffer, VkSemaphore, VkFence>
  newFrame(VkSemaphore acquireSemaphore) {
    vkWaitForFences(_device, 1, &_submitFence, true, UINT64_MAX);
    vkResetFences(_device, 1, &_submitFence);

    _commandCount = 0;
    vkResetCommandPool(_device, _pool, 0);

    if (_commandCount >= _buffers.size()) {
      vko::Logger::Info("** vkAllocateCommandBuffers(%d) **", _commandCount);
      VkCommandBufferAllocateInfo info = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
          .commandPool = _pool,
          .level = _commandBufferLevel,
          .commandBufferCount = 1,
      };
      VkCommandBuffer ret = VK_NULL_HANDLE;
      if (vkAllocateCommandBuffers(_device, &info, &ret) != VK_SUCCESS) {
        vko::Logger::Error("vkAllocateCommandBuffers");
        abort();
      }
      _buffers.push_back(ret);
    }

    VkSemaphore ret = _swapchainAcquireSemaphore;
    _swapchainAcquireSemaphore = acquireSemaphore;

    return {ret, _buffers[_commandCount++], _submitSemaphore, _submitFence};
  }
};

static double getCurrentTime() {
  timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) < 0) {
    vko::Logger::Error("clock_gettime() failed.\n");
    return 0.0;
  }
  return ts.tv_sec + ts.tv_nsec * 1e-9;
}

static bool main_loop(android_app *state, UserData *userdata) {
  vko::Logger::Info("## main_loop");
  vko::Instance instance;
  instance.appInfo.pApplicationName = "Mali SDK";
  instance.appInfo.pEngineName = "Mali SDK";
  instance.instanceExtensions = {"VK_KHR_surface", "VK_KHR_android_surface"};
#ifdef NDEBUG
#else
  instance.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instance.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  instance.instanceExtensions.push_back("VK_EXT_debug_report");
#endif
  VKO_CHECK(instance.create());

  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .flags = 0,
      .window = state->window,
  };
  VkSurfaceKHR _surface;
  VKO_CHECK(vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &_surface));

  vko::PhysicalDevice picked = instance.pickPhysicalDevice(_surface);
  assert(picked.physicalDevice);

  vko::Surface surface(instance, _surface, picked.physicalDevice);

  vko::Device device;
  device.validationLayers = instance.validationLayers;
  VKO_CHECK(device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  std::shared_ptr<class Pipeline> pipeline = Pipeline::create(
      picked.physicalDevice, device, surface.chooseSwapSurfaceFormat().format,
      state->activity->assetManager);

  vko::Swapchain swapchain(device);
  VKO_CHECK(swapchain.create(
      picked.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), picked.graphicsFamilyIndex,
      picked.presentFamilyIndex));
  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers;
  std::vector<std::shared_ptr<FlightManager>> flights;
  {
    auto imageCount = swapchain.images.size();
    backbuffers.resize(imageCount);
    flights.resize(imageCount);
    for (int i = 0; i < imageCount; ++i) {
      backbuffers[i] = nullptr;
      flights[i] = std::make_shared<FlightManager>(
          device, VK_COMMAND_BUFFER_LEVEL_PRIMARY, picked.graphicsFamilyIndex);
    }
  }

  unsigned frameCount = 0;
  auto startTime = getCurrentTime();

  auto semaphoreManager = std::make_shared<SemaphoreManager>(device);

  for (;;) {
    while (true) {
      if (!userdata->_active) {
        vkDeviceWaitIdle(device);
        return false;
      }

      struct android_poll_source *source;
      int events;
      if (ALooper_pollOnce(
              // timeout 0 for vulkan animation
              0, nullptr, &events, (void **)&source) < 0) {
        break;
      }
      if (source) {
        source->process(state, source);
      }
      if (state->destroyRequested) {
        return true;
      }
    }

    auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
    auto acquired = swapchain.acquireNextImage(acquireSemaphore);
    if (acquired.result == VK_SUCCESS) {
      auto backbuffer = backbuffers[acquired.imageIndex];
      if (!backbuffer) {
        backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
            device, acquired.image, swapchain.createInfo.imageExtent,
            surface.chooseSwapSurfaceFormat().format, pipeline->renderPass());
        backbuffers[acquired.imageIndex] = backbuffer;
      }
      auto flight = flights[acquired.imageIndex];

      // All queue submissions get a fence that CPU will wait
      // on for synchronization purposes.
      auto [oldSemaphore, cmd, semaphore, fence] =
          flight->newFrame(acquireSemaphore);

      // We will only submit this once before it's recycled.
      VkCommandBufferBeginInfo beginInfo = {
          .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
          .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
      };
      vkBeginCommandBuffer(cmd, &beginInfo);

      // Signal the underlying context that we're using this backbuffer now.
      // This will also wait for all fences associated with this swapchain image
      // to complete first.
      // When submitting command buffer that writes to swapchain, we need to
      // wait for this semaphore first. Also, delete the older semaphore. auto
      // oldSemaphore = backbuffer->beginFrame(cmd, acquireSemaphore);
      if (oldSemaphore != VK_NULL_HANDLE) {
        semaphoreManager->addClearedSemaphore(oldSemaphore);
      }
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
          .pSignalSemaphores = &semaphore,
      };
      if (vkQueueSubmit(device.graphicsQueue, 1, &info, fence) != VK_SUCCESS) {
        vko::Logger::Error("vkQueueSubmit");
        abort();
      }

      VkPresentInfoKHR present = {
          .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
          .waitSemaphoreCount = 1,
          .pWaitSemaphores = &semaphore,
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
      semaphoreManager->addClearedSemaphore(acquireSemaphore);
      // TODO:
      // swapchain = {};
      // return true;

    } else {
      // error ?
      vko::Logger::Error("Unrecoverable swapchain error.\n");
      vkQueueWaitIdle(device.presentQueue);
      semaphoreManager->addClearedSemaphore(acquireSemaphore);
      return true;
    }

    frameCount++;
    if (frameCount == 100) {
      double endTime = getCurrentTime();
      vko::Logger::Info("FPS: %.3f\n", frameCount / (endTime - startTime));
      frameCount = 0;
      startTime = endTime;
    }
  }
}

void android_main(android_app *state) {

#ifdef NDEBUG
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [release][android_main] ####");
#else
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [debug][android_main] ####");
#endif

  UserData userdata{
      .pApp = state,
      ._appName = APP_NAME,
  };
  state->userData = &userdata;
  state->onAppCmd = UserData::on_app_cmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    auto is_exit = wait_window(state, &userdata);
    if (is_exit) {
      break;
    }

    is_exit = main_loop(state, &userdata);
    if (is_exit) {
      break;
    }
  }
}
