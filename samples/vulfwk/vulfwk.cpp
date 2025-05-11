
#include "vulfwk.h"
#include "logger.h"

#include <optional>
#include <set>
#include <string>

static bool
checkValidationLayerSupport(const std::vector<const char *> &validationLayers) {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  for (const char *layerName : validationLayers) {
    bool layerFound = false;

    for (const auto &layerProperties : availableLayers) {
      if (strcmp(layerName, layerProperties.layerName) == 0) {
        layerFound = true;
        break;
      }
    }

    if (!layerFound) {
      return false;
    }
  }

  return true;
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

static VKAPI_ATTR VkBool32 VKAPI_CALL
debugCallback(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
              VkDebugUtilsMessageTypeFlagsEXT messageType,
              const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData,
              void *pUserData) {
  LOGE("validation layer: %s", pCallbackData->pMessage);
  return VK_FALSE;
}

static void
DestroyDebugUtilsMessengerEXT(VkInstance instance,
                              VkDebugUtilsMessengerEXT debugMessenger,
                              const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

struct QueueFamilyIndices {
  std::optional<uint32_t> graphicsFamily;
  std::optional<uint32_t> presentFamily;

  bool isComplete() {
    return graphicsFamily.has_value() && presentFamily.has_value();
  }
};

static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device,
                                            VkSurfaceKHR surface) {
  QueueFamilyIndices indices;

  uint32_t queueFamilyCount = 0;
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

  std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
  vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount,
                                           queueFamilies.data());

  int i = 0;
  for (const auto &queueFamily : queueFamilies) {
    if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
      indices.graphicsFamily = i;
    }

    VkBool32 presentSupport = false;
    vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

    if (presentSupport) {
      indices.presentFamily = i;
    }

    if (indices.isComplete()) {
      break;
    }

    i++;
  }

  return indices;
}

static bool
checkDeviceExtensionSupport(VkPhysicalDevice device,
                            const std::vector<const char *> &deviceExtensions) {
  uint32_t extensionCount;
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       nullptr);

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount,
                                       availableExtensions.data());

  std::set<std::string> requiredExtensions(deviceExtensions.begin(),
                                           deviceExtensions.end());

  for (const auto &extension : availableExtensions) {
    requiredExtensions.erase(extension.extensionName);
  }

  return requiredExtensions.empty();
}

struct SwapChainSupportDetails {
  VkSurfaceCapabilitiesKHR capabilities;
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;
};

static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice device,
                                                     VkSurfaceKHR surface) {
  SwapChainSupportDetails details;

  vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface,
                                            &details.capabilities);

  uint32_t formatCount;
  vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, nullptr);

  if (formatCount != 0) {
    details.formats.resize(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         details.formats.data());
  }

  uint32_t presentModeCount;
  vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount,
                                            nullptr);

  if (presentModeCount != 0) {
    details.presentModes.resize(presentModeCount);
    vkGetPhysicalDeviceSurfacePresentModesKHR(
        device, surface, &presentModeCount, details.presentModes.data());
  }

  return details;
}

static bool
isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                 const std::vector<const char *> &deviceExtensions) {
  QueueFamilyIndices indices = findQueueFamilies(device, surface);

  bool extensionsSupported =
      checkDeviceExtensionSupport(device, deviceExtensions);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport =
        querySwapChainSupport(device, surface);
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

VulkanFramework::VulkanFramework(const char *appName, const char *engineName) {
  _appName = appName;
  _engineName = engineName;
  LOGI("*** VulkanFramewor::VulkanFramework ***");
}

VulkanFramework::~VulkanFramework() {
  LOGI("*** VulkanFramewor::~VulkanFramework ***");
  cleanup();
}

bool VulkanFramework::initializeInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  for (auto name : layerNames) {
    LOGI("[layer] %s", name);
  }
  if (!checkValidationLayerSupport(layerNames)) {
    LOGE("validation layers requested, but not available!");
    return false;
  }
  for (auto name : extensionNames) {
    LOGI("[extension] %s", name);
  }
  if (!createInstance(layerNames, extensionNames)) {
    return false;
  }
  return true;
}

void VulkanFramework::cleanup() {
  if (Device) {
    vkDestroyDevice(Device, nullptr);
  }
  if (Surface) {
    vkDestroySurfaceKHR(Instance, Surface, nullptr);
  }
  if (DebugUtilsMessengerEXT) {
    DestroyDebugUtilsMessengerEXT(Instance, DebugUtilsMessengerEXT, nullptr);
  }
  if (Instance != VK_NULL_HANDLE) {
    vkDestroyInstance(Instance, nullptr);
  }
}

bool VulkanFramework::createSurfaceWin32(void *hInstance, void *hWnd) {
  VkWin32SurfaceCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR,
      .hinstance = reinterpret_cast<HINSTANCE>(hInstance),
      .hwnd = reinterpret_cast<HWND>(hWnd),
  };
  if (vkCreateWin32SurfaceKHR(Instance, &createInfo, nullptr, &Surface) !=
      VK_SUCCESS) {
    LOGE("failed to create window surface!");
    return false;
  }
  return true;
}

bool VulkanFramework::initializeDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames) {
  if (!pickPhysicalDevice(deviceExtensionNames)) {
    return false;
  }
  if (!createLogicalDevice(layerNames, deviceExtensionNames)) {
    return false;
  }
  return true;
}

bool VulkanFramework::createInstance(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &extensionNames) {
  VkDebugUtilsMessengerCreateInfoEXT DebugUtilsMessengerCreateInfoEXT{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugCallback,
  };

  VkApplicationInfo ApplicationInfo = {
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = _appName.c_str(),
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = _engineName.c_str(),
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo InstanceCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = layerNames.size() ? &DebugUtilsMessengerCreateInfoEXT : nullptr,
      .pApplicationInfo = &ApplicationInfo,
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
      .ppEnabledExtensionNames = extensionNames.data(),
  };

  if (layerNames.size()) {
    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) !=
        VK_SUCCESS) {
      LOGE("failed to create instance!");
      return false;
    }
  }

  if (CreateDebugUtilsMessengerEXT(Instance, &DebugUtilsMessengerCreateInfoEXT,
                                   nullptr,
                                   &DebugUtilsMessengerEXT) != VK_SUCCESS) {
    LOGE("failed to set up debug messenger!");
    return false;
  }

  return true;
}

bool VulkanFramework::pickPhysicalDevice(
    const std::vector<const char *> &deviceExtensionNames) {
  uint32_t deviceCount = 0;
  vkEnumeratePhysicalDevices(Instance, &deviceCount, nullptr);
  if (deviceCount == 0) {
    LOGE("failed to find GPUs with Vulkan support!");
    return false;
  }

  std::vector<VkPhysicalDevice> devices(deviceCount);
  vkEnumeratePhysicalDevices(Instance, &deviceCount, devices.data());

  for (const auto &device : devices) {
    if (isDeviceSuitable(device, Surface, deviceExtensionNames)) {
      PhysicalDevice = device;
      break;
    }
  }

  if (PhysicalDevice == VK_NULL_HANDLE) {
    LOGE("failed to find a suitable GPU!");
    return false;
  }

  return true;
}

bool VulkanFramework::createLogicalDevice(
    const std::vector<const char *> &layerNames,
    const std::vector<const char *> &deviceExtensionNames) {
  QueueFamilyIndices indices = findQueueFamilies(PhysicalDevice, Surface);

  std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
  std::set<uint32_t> uniqueQueueFamilies = {indices.graphicsFamily.value(),
                                            indices.presentFamily.value()};

  float queuePriority = 1.0f;
  for (uint32_t queueFamily : uniqueQueueFamilies) {
    VkDeviceQueueCreateInfo queueCreateInfo{
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = queueFamily,
        .queueCount = 1,
        .pQueuePriorities = &queuePriority,
    };
    queueCreateInfos.push_back(queueCreateInfo);
  }

  VkPhysicalDeviceFeatures deviceFeatures{};

  VkDeviceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size()),
      .pQueueCreateInfos = queueCreateInfos.data(),
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount =
          static_cast<uint32_t>(deviceExtensionNames.size()),
      .ppEnabledExtensionNames = deviceExtensionNames.data(),
      .pEnabledFeatures = &deviceFeatures,
  };

  if (vkCreateDevice(PhysicalDevice, &createInfo, nullptr, &Device) !=
      VK_SUCCESS) {
    LOGE("failed to create logical device!");
    return false;
  }

  vkGetDeviceQueue(Device, indices.graphicsFamily.value(), 0, &GraphicsQueue);
  vkGetDeviceQueue(Device, indices.presentFamily.value(), 0, &PresentQueue);
  return true;
}
