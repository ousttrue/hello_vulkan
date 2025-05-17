#include "device_manager.hpp"
#include "common.hpp"
#include "extensions.hpp"

///
/// PhysicalDevice
///
bool PhysicalDevice::SelectQueueFamily(VkPhysicalDevice gpu,
                                       VkSurfaceKHR surface) {
  Gpu = gpu;

  vkGetPhysicalDeviceProperties(Gpu, &Properties);
  vkGetPhysicalDeviceMemoryProperties(Gpu, &MemoryProperties);

  uint32_t queueCount;
  vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &queueCount, nullptr);
  if (queueCount < 1) {
    LOGE("Failed to query number of queues.");
    return false;
  }

  QueueProperties.resize(queueCount);
  vkGetPhysicalDeviceQueueFamilyProperties(Gpu, &queueCount,
                                           QueueProperties.data());

  bool foundQueue = false;
  for (uint32_t i = 0; i < queueCount; i++) {
    VkBool32 supportsPresent;
    vkGetPhysicalDeviceSurfaceSupportKHR(gpu, i, surface, &supportsPresent);

    // We want a queue which supports all of graphics, compute and
    // presentation.

    // There must exist at least one queue that has graphics and compute
    // support.
    const VkQueueFlags required = VK_QUEUE_GRAPHICS_BIT | VK_QUEUE_COMPUTE_BIT;

    if (((QueueProperties[i].queueFlags & required) == required) &&
        supportsPresent) {
      SelectedQueueFamilyIndex = i;
      foundQueue = true;
      break;
    }
  }

  if (!foundQueue) {
    LOGE("Did not find suitable queue which supports graphics, compute and "
         "presentation.\n");
    return false;
  }

  return true;
}

///
/// DeviceManager
///
DeviceManager::~DeviceManager() {
  if (Device) {
    vkDestroyDevice(Device, nullptr);
  }
  if (Surface) {
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
  }
  if (DebugUtilsMessengerEXT) {
    DestroyDebugUtilsMessengerEXT(Instance, DebugUtilsMessengerEXT, nullptr);
  }
  if (Instance) {
    vkDestroyInstance(Instance, nullptr);
  }
}

std::shared_ptr<DeviceManager>
DeviceManager::create(const char *appName, const char *engineName,
                      const std::vector<const char *> &layers) {

  InstanceExtensionManager instanceExtensions(
      {layers}, {"VK_KHR_surface", "VK_KHR_android_surface"});
  if (layers.size()) {
    auto supported =
        instanceExtensions.pushExtension(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
    if (!supported) {
      instanceExtensions.pushExtension("VK_EXT_debug_report");
    }
  }

  VkApplicationInfo app = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = appName,
      .applicationVersion = 0,
      .pEngineName = engineName,
      .engineVersion = 0,
      .apiVersion = VK_MAKE_VERSION(1, 0, 24),
  };

  VkInstanceCreateInfo instanceInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pApplicationInfo = &app,
      .enabledLayerCount =
          static_cast<uint32_t>(instanceExtensions.RequiredLayers.size()),
      .ppEnabledLayerNames = instanceExtensions.RequiredLayers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(instanceExtensions.RequiredExtensions.size()),
      .ppEnabledExtensionNames = instanceExtensions.RequiredExtensions.data(),
  };

  VkInstance instance;
  VkResult res = vkCreateInstance(&instanceInfo, nullptr, &instance);

  // Try to fall back to compatible Vulkan versions if the driver is using
  // older, but compatible API versions.
  if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
    app.apiVersion = VK_MAKE_VERSION(1, 0, 1);
    res = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (res == VK_SUCCESS) {
      LOGI("Created Vulkan instance with API version 1.0.1.\n");
    }
  }
  if (res == VK_ERROR_INCOMPATIBLE_DRIVER) {
    app.apiVersion = VK_MAKE_VERSION(1, 0, 2);
    res = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (res == VK_SUCCESS)
      LOGI("Created Vulkan instance with API version 1.0.2.\n");
  }
  if (res != VK_SUCCESS) {
    LOGE("Failed to create Vulkan instance (error: %d).\n", int(res));
    return {};
  }

  auto ptr = std::shared_ptr<DeviceManager>(new DeviceManager(instance));

  if (layers.size()) {
    ptr->DebugUtilsMessengerEXT = CreateDebugUtilsMessengerEXT(instance);
  }

  return ptr;
}

bool DeviceManager::createSurfaceFromAndroid(ANativeWindow *window) {
  VkAndroidSurfaceCreateInfoKHR info = {
      .sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR,
      .pNext = 0,
      .flags = 0,
      .window = window,
  };
  if (vkCreateAndroidSurfaceKHR(Instance, &info, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("vkCreateAndroidSurfaceKHR");
    return false;
  }

  return true;
}

VkPhysicalDevice DeviceManager::selectGpu() {
  uint32_t gpuCount = 0;
  if (vkEnumeratePhysicalDevices(Instance, &gpuCount, nullptr) != VK_SUCCESS) {
    LOGE("vkEnumeratePhysicalDevices");
    return VK_NULL_HANDLE;
  }
  if (gpuCount < 1) {
    LOGE("vkEnumeratePhysicalDevices: no device");
    return VK_NULL_HANDLE;
  }
  Gpus.resize(gpuCount);
  if (vkEnumeratePhysicalDevices(Instance, &gpuCount, Gpus.data()) !=
      VK_SUCCESS) {
    LOGE("vkEnumeratePhysicalDevices");
    return VK_NULL_HANDLE;
  }
  GpuProps.resize(gpuCount);
  for (uint32_t i = 0; i < gpuCount; ++i) {
    vkGetPhysicalDeviceProperties(Gpus[i], &GpuProps[i]);
    LOGI("[gpu: %02d] %s", i, GpuProps[i].deviceName);
  }
  if (gpuCount > 1) {
    LOGI("select 1st device");
  }
  if (!Selected.SelectQueueFamily(Gpus[0], Surface)) {
    return VK_NULL_HANDLE;
  }

  return Selected.Gpu;
}

bool DeviceManager::createLogicalDevice(
    const std::vector<const char *> &layers) {

  DeviceExtensionManager deviceExtensions(Selected.Gpu, {layers},
                                          {"VK_KHR_swapchain"});
  if (layers.size()) {
  }

  static const float one = 1.0f;
  VkDeviceQueueCreateInfo queueInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
      .pNext = 0,
      .flags = 0,
      .queueFamilyIndex = Selected.SelectedQueueFamilyIndex,
      .queueCount = 1,
      .pQueuePriorities = &one,
  };

  VkPhysicalDeviceFeatures features = {false};
  VkDeviceCreateInfo deviceInfo = {
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 1,
      .pQueueCreateInfos = &queueInfo,
      .enabledLayerCount =
          static_cast<uint32_t>(deviceExtensions.RequiredLayers.size()),
      .ppEnabledLayerNames = deviceExtensions.RequiredLayers.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(deviceExtensions.RequiredExtensions.size()),
      .ppEnabledExtensionNames = deviceExtensions.RequiredExtensions.data(),
      .pEnabledFeatures = &features,
  };

  if (vkCreateDevice(Selected.Gpu, &deviceInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    return false;
  }

  vkGetDeviceQueue(Device, Selected.SelectedQueueFamilyIndex, 0, &Queue);

  return true;
}
