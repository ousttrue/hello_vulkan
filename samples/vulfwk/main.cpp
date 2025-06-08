#include <vko.h>

#include "vulfwk_pipeline.h"

#ifdef ANDROID
#include <android_native_app_glue.h>

template <typename T> struct UserData {
  struct android_app *pApp = nullptr;
  T *impl = nullptr;
  bool active = false;
};

static void engineHandleCmd(android_app *pApp, int32_t cmd) {
  auto userData = static_cast<UserData<VulkanFramework> *>(pApp->userData);

  switch (cmd) {
  case APP_CMD_RESUME: {
    vko::Logger::Info("Resuming swapchain!\n");
    userData->active = true;
    break;
  }

  case APP_CMD_PAUSE: {
    vko::Logger::Info("Pausing swapchain!\n");
    userData->active = false;
    break;
  }

  case APP_CMD_INIT_WINDOW: {
    vko::Logger::Info("Initializing platform!\n");
    auto vulfwk = userData->impl;

    std::vector<const char *> validationLayers;
    std::vector<const char *> instanceExtensions = {"VK_KHR_surface",
                                                    "VK_KHR_android_surface"};
    std::vector<const char *> deviceExtensions = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifdef NDEBUG
#else
    validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    if (!vulfwk->initializeInstance(validationLayers, instanceExtensions)) {
      vko::Logger::Error("failed: initializeInstance");
      return;
    }

    VkAndroidSurfaceCreateInfoKHR info = {
        .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
        .flags = 0,
        .window = pApp->window,
    };
    VkSurfaceKHR surface;
    if (vkCreateAndroidSurfaceKHR(vulfwk->Instance, &info, nullptr, &surface) !=
        VK_SUCCESS) {
      vko::Logger::Error("failed: vkCreateAndroidSurfaceKHR");
      return;
    }

    if (!vulfwk->initializeDevice(validationLayers, deviceExtensions,
                                  surface)) {
      vko::Logger::Error("failed: initializeDevice");
      return;
    }
    vko::Logger::Info("vulkan initilized");
    auto width = ANativeWindow_getWidth(pApp->window);
    auto height = ANativeWindow_getHeight(pApp->window);
    vulfwk->drawFrame(width, height);

    break;
  }

  case APP_CMD_TERM_WINDOW:
    vko::Logger::Info("Terminating application!\n");
    // userData->impl->onTerminateWindow();
    break;
  }
}

void android_main(android_app *state) {
#ifdef NDEBUG
  vko::Logger::Info("[release]Entering android_main()!\n");
#else
  vko::Logger::Info("[debug]Entering android_main()!\n");
#endif

  VulkanFramework vulfwk("vulfwk", "No Engine");
  UserData<VulkanFramework> userData = {
      .pApp = state,
      .impl = &vulfwk,
  };
  state->userData = &userData;
  state->onAppCmd = engineHandleCmd;

  vulfwk.setAssetManager(state->activity->assetManager);

  for (;;) {
    struct android_poll_source *source;
    int ident;
    int events;

    while ((ident = ALooper_pollOnce(userData.active ? 0 : -1, nullptr, &events,
                                     (void **)&source)) >= 0) {
      if (source)
        source->process(state, source);

      if (state->destroyRequested)
        return;
    }

    // renderFrame
    if (userData.active && state->window) {
      // auto width = ANativeWindow_getWidth(state->window);
      // auto height = ANativeWindow_getHeight(state->window);
      // vulfwk.drawFrame(width, height);
    }
  }
}

#else

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>

class Glfw {

public:
  GLFWwindow *_window = nullptr;
  Glfw() { glfwInit(); }
  ~Glfw() {
    if (_window) {
      glfwDestroyWindow(_window);
    }
    glfwTerminate();
  }
  void *createWindow(int width, int height, const char *title) {
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    // glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

    _window = glfwCreateWindow(width, height, title, nullptr, nullptr);

    return glfwGetWin32Window(_window);
  }
  void addRequiredExtensions(std::vector<const char *> &extensions) {
    uint32_t glfwExtensionCount = 0;
    const char **glfwExtensions;
    glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
      extensions.push_back(glfwExtensions[i]);
    }
  }
  bool nextFrame(int *width, int *height) {
    if (glfwWindowShouldClose(_window)) {
      return false;
    }
    glfwPollEvents();
    glfwGetFramebufferSize(_window, width, height);
    return true;
  }
  // void flush() { glfwSwapBuffers(_window); }
};

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME};
#ifndef NDEBUG
  vko::Logger::Info("[debug build]");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  Glfw glfw;
  auto hwnd = glfw.createWindow(640, 480, "vulfwk");
  glfw.addRequiredExtensions(instanceExtensions);

  vko::Instance Instance;
  Instance.validationLayers = validationLayers;
  Instance.instanceExtensions = instanceExtensions;
  Instance.appInfo.pApplicationName = "vulfwk";
  Instance.appInfo.pEngineName = "No Engine";
  VKO_CHECK(Instance.create());

  // VkQueue _graphicsQueue = VK_NULL_HANDLE;
  // std::shared_ptr<vko::Fence> SubmitCompleteFence;

  VkSurfaceKHR surface;
  VKO_CHECK(glfwCreateWindowSurface(Instance, glfw._window, nullptr, &surface));

  auto picked = Instance.pickPhysicalDevice(surface);
  auto Surface =
      std::make_shared<vko::Surface>(Instance, surface, picked.physicalDevice);

  vko::Device Device;
  Device.validationLayers = Instance.validationLayers;
  VKO_CHECK(Device.create(picked.physicalDevice, picked.graphicsFamilyIndex,
                          picked.presentFamilyIndex));

  auto Pipeline =
      PipelineImpl::create(picked.physicalDevice, surface, Device,
                           Surface->chooseSwapSurfaceFormat().format, nullptr);
  if (!Pipeline) {
    return 1;
  }

  auto semaphorePool = std::make_shared<vko::SemaphorePool>(Device);

  auto SubmitCompleteFence = std::make_shared<vko::Fence>(Device, true);

  //   vkGetDeviceQueue(Device, _picked.graphicsFamilyIndex, 0,
  //   &_graphicsQueue);
  //

  auto _semaphorePool = std::make_shared<vko::SemaphorePool>(Device);
  std::shared_ptr<vko::Swapchain> Swapchain;
  std::vector<std::shared_ptr<vko::SwapchainFramebuffer>> images;
  while (true) {
    int width, height;
    if (!glfw.nextFrame(&width, &height)) {
      break;
    }

    // vulfwk.drawFrame(width, height);
    // glfw.flush();
    // bool VulkanFramework::drawFrame(uint32_t width, uint32_t height) {

    if (!Swapchain) {
      vkDeviceWaitIdle(Device);

      Swapchain = std::make_shared<vko::Swapchain>(Device);
      Swapchain->create(picked.physicalDevice, *Surface,
                        Surface->chooseSwapSurfaceFormat(),
                        Surface->chooseSwapPresentMode(),
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

  return 0;
}
#endif
