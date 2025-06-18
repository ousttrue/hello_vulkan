#include "main_loop.h"
#include <vuloxr/glfw.h>

auto NAME = "vko";

int main(int argc, char **argv) {
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
#ifndef NDEBUG
  vko::Logger::Info("[debug build]");
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  vuloxr::glfw::Glfw glfw;
  auto window = glfw.createWindow(640, 480, NAME);
  uint32_t glfwExtensionCount = 0;
  const char **glfwExtensions;
  glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
  for (uint32_t i = 0; i < glfwExtensionCount; ++i) {
    instanceExtensions.push_back(glfwExtensions[i]);
  }

  vko::Instance instance;
  instance.validationLayers = validationLayers;
  instance.instanceExtensions = instanceExtensions;
  instance.appInfo.pApplicationName = NAME;
  instance.appInfo.pEngineName = "No Engine";
  vko::VKO_CHECK(instance.create());

  VkSurfaceKHR _surface;
  vko::VKO_CHECK(
      glfwCreateWindowSurface(instance, window, nullptr, &_surface));

  auto physicalDevice = instance.pickPhysicalDevice(_surface);
  vko::Surface surface(instance, _surface, physicalDevice.physicalDevice);

  vko::Device device;
  device.validationLayers = instance.validationLayers;
  device.deviceExtensions = deviceExtensions;
  vko::VKO_CHECK(device.create(physicalDevice.physicalDevice,
                               physicalDevice.graphicsFamilyIndex,
                               physicalDevice.presentFamilyIndex));

  vko::Swapchain swapchain(device);
  swapchain.create(
      physicalDevice.physicalDevice, surface, surface.chooseSwapSurfaceFormat(),
      surface.chooseSwapPresentMode(), physicalDevice.graphicsFamilyIndex,
      physicalDevice.presentFamilyIndex);

  main_loop([&glfw]() { return glfw.newFrame(); }, swapchain, physicalDevice,
            device);

  return 0;
}
