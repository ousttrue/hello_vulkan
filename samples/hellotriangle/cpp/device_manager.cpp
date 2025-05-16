#include "device_manager.hpp"
#include "common.hpp"

#ifdef FORCE_NO_VALIDATION
#define ENABLE_VALIDATION_LAYERS 0
#else
#define ENABLE_VALIDATION_LAYERS 1
#endif

#define NELEMS(x) (sizeof(x) / sizeof((x)[0]))
static const char *pValidationLayers[] = {
    "VK_LAYER_KHRONOS_validation",
};

// typedef VkBool32 (VKAPI_PTR *PFN_vkDebugUtilsMessengerCallbackEXT)(
//     VkDebugUtilsMessageSeverityFlagBitsEXT           messageSeverity,
//     VkDebugUtilsMessageTypeFlagsEXT                  messageTypes,
//     const VkDebugUtilsMessengerCallbackDataEXT*      pCallbackData,
//     void*                                            pUserData);
static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageTypes,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  if (pCallbackData->flags & VK_DEBUG_REPORT_ERROR_BIT_EXT) {
    LOGE("Validation Layer: Error: %s: %s\n", pCallbackData->pMessageIdName,
         pCallbackData->pMessage);
  } else if (pCallbackData->flags & VK_DEBUG_REPORT_WARNING_BIT_EXT) {
    LOGE("Validation Layer: Warning: %s: %s\n", pCallbackData->pMessageIdName,
         pCallbackData->pMessage);
  } else if (pCallbackData->flags &
             VK_DEBUG_REPORT_PERFORMANCE_WARNING_BIT_EXT) {
    LOGI("Validation Layer: Performance warning: %s: %s\n",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  } else {
    LOGI("Validation Layer: Information: %s: %s\n",
         pCallbackData->pMessageIdName, pCallbackData->pMessage);
  }
  return VK_FALSE;
}

static VkResult CreateDebugUtilsMessengerEXT(
    VkInstance instance, const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
    const VkAllocationCallbacks *pAllocator,
    VkDebugUtilsMessengerEXT *pDebugMessenger) {
  auto func = (PFN_vkCreateDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkCreateDebugUtilsMessengerEXT");
  if (func != nullptr) {
    return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
  } else {
    return VK_ERROR_EXTENSION_NOT_PRESENT;
  }
}
static void
addSupportedLayers(std::vector<const char *> &activeLayers,
                   const std::vector<VkLayerProperties> &instanceLayers,
                   const char **ppRequested, unsigned numRequested) {
  for (unsigned i = 0; i < numRequested; i++) {
    auto *layer = ppRequested[i];
    for (auto &ext : instanceLayers) {
      if (strcmp(ext.layerName, layer) == 0) {
        activeLayers.push_back(layer);
        break;
      }
    }
  }
}

//   std::vector<std::string> externalLayers;
//   PFN_vkDebugReportCallbackEXT externalDebugCallback = nullptr;
//   void *pExternalDebugCallbackUserData = nullptr;
//   VkDebugUtilsMessengerEXT DebugUtilsMessengerEXT = VK_NULL_HANDLE;
//
//
//
//   inline void
//   addExternalLayers(std::vector<const char *> &activeLayers,
//                     const std::vector<VkLayerProperties> &supportedLayers) {
//     for (auto &layer : externalLayers) {
//       for (auto &supportedLayer : supportedLayers) {
//         if (layer == supportedLayer.layerName) {
//           activeLayers.push_back(supportedLayer.layerName);
//           LOGI("Found external layer: %s\n", supportedLayer.layerName);
//           break;
//         }
//       }
//     }
//   }
//
//
//
// #if ENABLE_VALIDATION_LAYERS
//   uint32_t instanceLayerCount;
//   VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
//   vector<VkLayerProperties> instanceLayers(instanceLayerCount);
//   VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount,
//                                               instanceLayers.data()));
//
//   // A layer could have VK_EXT_debug_report extension.
//   for (auto &layer : instanceLayers) {
//     uint32_t count;
//     VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count,
//                                                     nullptr));
//     vector<VkExtensionProperties> extensions(count);
//     VK_CHECK(vkEnumerateInstanceExtensionProperties(layer.layerName, &count,
//                                                     extensions.data()));
//     for (auto &ext : extensions)
//       instanceExtensions.push_back(ext);
//   }
//
//   // On desktop, the LunarG loader exposes a meta-layer that combines all
//   // relevant validation layers.
//   vector<const char *> activeLayers;
//
//   // On Android, add all relevant layers one by one.
//   if (activeLayers.empty()) {
//     addSupportedLayers(activeLayers, instanceLayers, pValidationLayers,
//                        NELEMS(pValidationLayers));
//   }
//
//   if (activeLayers.empty())
//     LOGI("Did not find validation layers.\n");
//   else
//     LOGI("Found validation layers!\n");
//
//   addExternalLayers(activeLayers, instanceLayers);
// #endif
//
// #if ENABLE_VALIDATION_LAYERS
//   if (!activeLayers.empty()) {
//     instanceInfo.enabledLayerCount = activeLayers.size();
//     instanceInfo.ppEnabledLayerNames = activeLayers.data();
//     LOGI("Using Vulkan instance validation layers.\n");
//   }
// #endif
//
//   VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfoEXT{
//       .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
//       .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
//                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
//                          VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
//       .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
//                      VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
//                      VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
//       .pfnUserCallback = debugCallback,
//   };
//
//   if (activeLayers.size()) {
//     if (CreateDebugUtilsMessengerEXT(instance,
//                                      &DebugUtilsMessengerCreateInfoEXT,
//                                      nullptr, &DebugUtilsMessengerEXT) !=
//                                      VK_SUCCESS) {
//       LOGE("failed to set up debug messenger!");
//       return MaliSDK::RESULT_ERROR_GENERIC;
//     }
//   }
//
// #if ENABLE_VALIDATION_LAYERS
//   uint32_t deviceLayerCount;
//   VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount,
//   nullptr)); vector<VkLayerProperties> deviceLayers(deviceLayerCount);
//   VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount,
//                                             deviceLayers.data()));
//
//   activeLayers.clear();
//   // On desktop, the LunarG loader exposes a meta-layer that combines all
//   // relevant validation layers.
//
//   // On Android, add all relevant layers one by one.
//   if (activeLayers.empty()) {
//     addSupportedLayers(activeLayers, deviceLayers, pValidationLayers,
//                        NELEMS(pValidationLayers));
//   }
//   addExternalLayers(activeLayers, deviceLayers);
// #endif
//
// #if ENABLE_VALIDATION_LAYERS
//   if (!activeLayers.empty()) {
//     deviceInfo.enabledLayerCount = activeLayers.size();
//     deviceInfo.ppEnabledLayerNames = activeLayers.data();
//     LOGI("Using Vulkan device validation layers.\n");
//   }
// #endif
//
//
//
// inline void addExternalLayer(const char *pName) {
//   externalLayers.push_back(pName);
// }
// inline void setExternalDebugCallback(PFN_vkDebugReportCallbackEXT callback,
//                                      void *pUserData) {
//   externalDebugCallback = callback;
//   pExternalDebugCallbackUserData = pUserData;
// }
// inline PFN_vkDebugReportCallbackEXT getExternalDebugCallback() const {
//   return externalDebugCallback;
// }
// inline void *getExternalDebugCallbackUserData() const {
//   return pExternalDebugCallbackUserData;
// }

static bool
validateExtensions(const std::vector<const char *> &required,
                   const std::vector<VkExtensionProperties> &available) {
  for (auto extension : required) {
    bool found = false;
    for (auto &availableExtension : available) {
      if (strcmp(availableExtension.extensionName, extension) == 0) {
        found = true;
        break;
      }
    }

    if (!found)
      return false;
  }

  return true;
}

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
  if (Instance) {
    vkDestroyInstance(Instance, nullptr);
  }
}

/// create instance and validation layer
std::shared_ptr<DeviceManager> DeviceManager::create() {
  const std::vector<const char *> requiredInstanceExtensions{
      "VK_KHR_surface", "VK_KHR_android_surface"};

  uint32_t instanceExtensionCount;
  std::vector<const char *> activeInstanceExtensions;
  VK_CHECK(vkEnumerateInstanceExtensionProperties(
      nullptr, &instanceExtensionCount, nullptr));
  std::vector<VkExtensionProperties> instanceExtensions(instanceExtensionCount);
  VK_CHECK(vkEnumerateInstanceExtensionProperties(
      nullptr, &instanceExtensionCount, instanceExtensions.data()));
  // for (auto &instanceExt : instanceExtensions)
  //   LOGI("Instance extension: %s\n", instanceExt.extensionName);

  bool useInstanceExtensions = true;
  if (!validateExtensions(requiredInstanceExtensions, instanceExtensions)) {
    LOGI("Required instance extensions are missing, will try without.\n");
    useInstanceExtensions = false;
  } else
    activeInstanceExtensions = requiredInstanceExtensions;

  bool haveDebugReport = false;
  for (auto &ext : instanceExtensions) {
    if (strcmp(ext.extensionName, "VK_EXT_debug_utils") == 0) {
      haveDebugReport = true;
      useInstanceExtensions = true;
      activeInstanceExtensions.push_back("VK_EXT_debug_utils");
      break;
    }
  }

  VkApplicationInfo app = {VK_STRUCTURE_TYPE_APPLICATION_INFO};
  app.pApplicationName = "Mali SDK";
  app.applicationVersion = 0;
  app.pEngineName = "Mali SDK";
  app.engineVersion = 0;
  app.apiVersion = VK_MAKE_VERSION(1, 0, 24);

  VkInstanceCreateInfo instanceInfo = {VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO};
  instanceInfo.pApplicationInfo = &app;
  if (useInstanceExtensions) {
    instanceInfo.enabledExtensionCount = activeInstanceExtensions.size();
    instanceInfo.ppEnabledExtensionNames = activeInstanceExtensions.data();
  }

  // Create the Vulkan instance

  // for (auto name : activeLayers) {
  //   LOGI("[layer] %s", name);
  // }
  // for (auto name : activeInstanceExtensions) {
  //   LOGI("[instance extension] %s", name);
  // }
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

bool DeviceManager::createLogicalDevice() {
  uint32_t deviceExtensionCount;
  VK_CHECK(vkEnumerateDeviceExtensionProperties(
      Selected.Gpu, nullptr, &deviceExtensionCount, nullptr));
  std::vector<VkExtensionProperties> deviceExtensions(deviceExtensionCount);
  VK_CHECK(vkEnumerateDeviceExtensionProperties(
      Selected.Gpu, nullptr, &deviceExtensionCount, deviceExtensions.data()));
  // for (auto &deviceExt : deviceExtensions)
  //   LOGI("Device extension: %s\n", deviceExt.extensionName);
  const std::vector<const char *> requiredDeviceExtensions{"VK_KHR_swapchain"};
  bool useDeviceExtensions = true;
  if (!validateExtensions(requiredDeviceExtensions, deviceExtensions)) {
    LOGI("Required device extensions are missing, will try without.\n");
    useDeviceExtensions = false;
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
      .enabledExtensionCount =
          static_cast<uint32_t>(requiredDeviceExtensions.size()),
      .ppEnabledExtensionNames = requiredDeviceExtensions.data(),
      .pEnabledFeatures = &features,
  };

  if (vkCreateDevice(Selected.Gpu, &deviceInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    return false;
  }

  vkGetDeviceQueue(Device, Selected.SelectedQueueFamilyIndex, 0, &Queue);

  return true;
}
