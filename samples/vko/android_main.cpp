#include <assert.h>
#include <vko/android_userdata.h>
#include <vko/vko.h>

#include "main_loop.h"

auto APP_NAME = "vulfwk";

static bool _main_loop(android_app *app, vko::UserData *userdata) {
  vko::Logger::Info("## main_loop");
  vko::Instance instance;
  instance.appInfo.pApplicationName = "vulfwk";
  instance.appInfo.pEngineName = "vulfwk";
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
      .window = app->window,
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

  main_loop(
      [userdata, app]() {
        while (true) {
          if (!userdata->_active) {
            return false;
          }

          struct android_poll_source *source;
          int events;
          if (ALooper_pollOnce(
                  // timeout 0 for vulkan animation
                  0, nullptr, &events, (void **)&source) < 0) {
            return true;
          }
          if (source) {
            source->process(app, source);
          }
          if (app->destroyRequested) {
            return false;
          }
        }
      },
      surface, picked, device);

  return true;

  //
  //   vko::Device device;
  //   device.validationLayers = instance.validationLayers;
  //   VKO_CHECK(device.create(picked.physicalDevice,
  //   picked.graphicsFamilyIndex,
  //                           picked.presentFamilyIndex));
  //
  //   std::shared_ptr<class Pipeline> pipeline = Pipeline::create(
  //       picked.physicalDevice, device,
  //       surface.chooseSwapSurfaceFormat().format,
  //       app->activity->assetManager);
  //
  //   vko::Swapchain swapchain(device);
  //   VKO_CHECK(swapchain.create(
  //       picked.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
  //       surface.chooseSwapPresentMode(), picked.graphicsFamilyIndex,
  //       picked.presentFamilyIndex));
  //   std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> backbuffers;
  //   std::vector<std::shared_ptr<FlightManager>> flights;
  //   {
  //     auto imageCount = swapchain.images.size();
  //     backbuffers.resize(imageCount);
  //     flights.resize(imageCount);
  //     for (int i = 0; i < imageCount; ++i) {
  //       backbuffers[i] = nullptr;
  //       flights[i] = std::make_shared<FlightManager>(
  //           device, VK_COMMAND_BUFFER_LEVEL_PRIMARY,
  //           picked.graphicsFamilyIndex);
  //     }
  //   }
  //
  //   unsigned frameCount = 0;
  //   auto startTime = getCurrentTime();
  //
  //   auto semaphoreManager = std::make_shared<SemaphoreManager>(device);
  //
  //   for (;;) {
  //     while (true) {
  //       if (!userdata->_active) {
  //         vkDeviceWaitIdle(device);
  //         return false;
  //       }
  //
  //       struct android_poll_source *source;
  //       int events;
  //       if (ALooper_pollOnce(
  //               // timeout 0 for vulkan animation
  //               0, nullptr, &events, (void **)&source) < 0) {
  //         break;
  //       }
  //       if (source) {
  //         source->process(app, source);
  //       }
  //       if (app->destroyRequested) {
  //         return true;
  //       }
  //     }
  //
  //     auto acquireSemaphore = semaphoreManager->getClearedSemaphore();
  //     auto acquired = swapchain.acquireNextImage(acquireSemaphore);
  //     if (acquired.result == VK_SUCCESS) {
  //       auto backbuffer = backbuffers[acquired.imageIndex];
  //       if (!backbuffer) {
  //         backbuffer = std::make_shared<vko::SwapchainFramebuffer>(
  //             device, acquired.image, swapchain.createInfo.imageExtent,
  //             surface.chooseSwapSurfaceFormat().format,
  //             pipeline->renderPass());
  //         backbuffers[acquired.imageIndex] = backbuffer;
  //       }
  //       auto flight = flights[acquired.imageIndex];
  //
  //       // All queue submissions get a fence that CPU will wait
  //       // on for synchronization purposes.
  //       auto [oldSemaphore, cmd, semaphore, fence] =
  //           flight->newFrame(acquireSemaphore);
  //
  //       // We will only submit this once before it's recycled.
  //       VkCommandBufferBeginInfo beginInfo = {
  //           .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
  //           .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT,
  //       };
  //       vkBeginCommandBuffer(cmd, &beginInfo);
  //
  //       // Signal the underlying context that we're using this backbuffer
  //       now.
  //       // This will also wait for all fences associated with this swapchain
  //       image
  //       // to complete first.
  //       // When submitting command buffer that writes to swapchain, we need
  //       to
  //       // wait for this semaphore first. Also, delete the older semaphore.
  //       auto
  //       // oldSemaphore = backbuffer->beginFrame(cmd, acquireSemaphore);
  //       if (oldSemaphore != VK_NULL_HANDLE) {
  //         semaphoreManager->addClearedSemaphore(oldSemaphore);
  //       }
  //       pipeline->render(cmd, backbuffer->framebuffer,
  //                        swapchain.createInfo.imageExtent);
  //
  //       const VkPipelineStageFlags waitStage =
  //           VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
  //       VkSubmitInfo info = {
  //           .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
  //           .waitSemaphoreCount =
  //               static_cast<uint32_t>(acquireSemaphore != VK_NULL_HANDLE ? 1
  //               : 0),
  //           .pWaitSemaphores = &acquireSemaphore,
  //           .pWaitDstStageMask = &waitStage,
  //           .commandBufferCount = 1,
  //           .pCommandBuffers = &cmd,
  //           .signalSemaphoreCount = 1,
  //           .pSignalSemaphores = &semaphore,
  //       };
  //       if (vkQueueSubmit(device.graphicsQueue, 1, &info, fence) !=
  //       VK_SUCCESS) {
  //         vko::Logger::Error("vkQueueSubmit");
  //         abort();
  //       }
  //
  //       VkPresentInfoKHR present = {
  //           .sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR,
  //           .waitSemaphoreCount = 1,
  //           .pWaitSemaphores = &semaphore,
  //           .swapchainCount = 1,
  //           .pSwapchains = &swapchain.swapchain,
  //           .pImageIndices = &acquired.imageIndex,
  //           .pResults = nullptr,
  //       };
  //       VKO_CHECK(vkQueuePresentKHR(device.presentQueue, &present));
  //
  //     } else if (acquired.result == VK_SUBOPTIMAL_KHR ||
  //                acquired.result == VK_ERROR_OUT_OF_DATE_KHR) {
  //       vko::Logger::Error("[RESULT_ERROR_OUTDATED_SWAPCHAIN]");
  //       vkQueueWaitIdle(device.presentQueue);
  //       semaphoreManager->addClearedSemaphore(acquireSemaphore);
  //       // TODO:
  //       // swapchain = {};
  //       // return true;
  //
  //     } else {
  //       // error ?
  //       vko::Logger::Error("Unrecoverable swapchain error.\n");
  //       vkQueueWaitIdle(device.presentQueue);
  //       semaphoreManager->addClearedSemaphore(acquireSemaphore);
  //       return true;
  //     }
  //
  //     frameCount++;
  //     if (frameCount == 100) {
  //       double endTime = getCurrentTime();
  //       vko::Logger::Info("FPS: %.3f\n", frameCount / (endTime - startTime));
  //       frameCount = 0;
  //       startTime = endTime;
  //     }
  //   }
}

void android_main(android_app *app) {
#ifdef NDEBUG
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [release][android_main] ####");
#else
  __android_log_print(ANDROID_LOG_INFO, APP_NAME,
                      "#### [debug][android_main] ####");
#endif

  vko::UserData userdata{
      .pApp = app,
      ._appName = APP_NAME,
  };
  app->userData = &userdata;
  app->onAppCmd = vko::UserData::on_app_cmd;
  app->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    auto is_exit = wait_window(app, &userdata);
    if (is_exit) {
      break;
    }

    is_exit = _main_loop(app, &userdata);
    if (is_exit) {
      break;
    }
  }
}
