
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
  VkSurfaceCapabilitiesKHR capabilities = {};
  std::vector<VkSurfaceFormatKHR> formats;
  std::vector<VkPresentModeKHR> presentModes;

  bool querySwapChainSupport(VkPhysicalDevice device, VkSurfaceKHR surface) {
    if (vkGetPhysicalDeviceSurfaceCapabilitiesKHR(
            device, surface, &this->capabilities) != VK_SUCCESS) {
      LOGE("failed: vkGetPhysicalDeviceSurfaceCapabilitiesKHR");
      return false;
    }

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                         nullptr);

    if (formatCount != 0) {
      this->formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount,
                                           this->formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface,
                                              &presentModeCount, nullptr);

    if (presentModeCount != 0) {
      this->presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(
          device, surface, &presentModeCount, this->presentModes.data());
    }

    return true;
  }
};

static bool
isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface,
                 const std::vector<const char *> &deviceExtensions) {
  QueueFamilyIndices indices = findQueueFamilies(device, surface);

  bool extensionsSupported =
      checkDeviceExtensionSupport(device, deviceExtensions);

  bool swapChainAdequate = false;
  if (extensionsSupported) {
    SwapChainSupportDetails swapChainSupport;
    if (!swapChainSupport.querySwapChainSupport(device, surface)) {
      return false;
    }
    swapChainAdequate = !swapChainSupport.formats.empty() &&
                        !swapChainSupport.presentModes.empty();
  }

  return indices.isComplete() && extensionsSupported && swapChainAdequate;
}

static VkSurfaceFormatKHR chooseSwapSurfaceFormat(
    const std::vector<VkSurfaceFormatKHR> &availableFormats) {
  for (const auto &availableFormat : availableFormats) {
    if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
        availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
      return availableFormat;
    }
  }

  return availableFormats[0];
}

static VkPresentModeKHR chooseSwapPresentMode(
    const std::vector<VkPresentModeKHR> &availablePresentModes) {
  for (const auto &availablePresentMode : availablePresentModes) {
    if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
      return availablePresentMode;
    }
  }

  return VK_PRESENT_MODE_FIFO_KHR;
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
  if (Swapchain) {
    vkDestroySwapchainKHR(Device, Swapchain, nullptr);
  }
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
      .flags = 0,
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

bool VulkanFramework::createSwapChain(VkExtent2D imageExtent) {
  if (SwapchainExtent.width == imageExtent.width &&
      SwapchainExtent.height == imageExtent.height) {
    return true;
  }
  SwapChainSupportDetails swapChainSupport;
  if (!swapChainSupport.querySwapChainSupport(PhysicalDevice, Surface)) {
    return false;
  }

  VkSurfaceFormatKHR surfaceFormat =
      chooseSwapSurfaceFormat(swapChainSupport.formats);
  VkPresentModeKHR presentMode =
      chooseSwapPresentMode(swapChainSupport.presentModes);

  uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
  if (swapChainSupport.capabilities.maxImageCount > 0 &&
      imageCount > swapChainSupport.capabilities.maxImageCount) {
    imageCount = swapChainSupport.capabilities.maxImageCount;
  }

  VkSwapchainCreateInfoKHR createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .surface = Surface,
      .minImageCount = imageCount,
      .imageFormat = surfaceFormat.format,
      .imageColorSpace = surfaceFormat.colorSpace,
      .imageExtent = imageExtent,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
  };

  QueueFamilyIndices indices = findQueueFamilies(PhysicalDevice, Surface);
  uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(),
                                   indices.presentFamily.value()};

  if (indices.graphicsFamily != indices.presentFamily) {
    createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
    createInfo.queueFamilyIndexCount = 2;
    createInfo.pQueueFamilyIndices = queueFamilyIndices;
  } else {
    createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  }

  createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
  createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  createInfo.presentMode = presentMode;
  createInfo.clipped = VK_TRUE;

  createInfo.oldSwapchain = VK_NULL_HANDLE;

  if (vkCreateSwapchainKHR(Device, &createInfo, nullptr, &Swapchain) !=
      VK_SUCCESS) {
    LOGE("failed to create swap chain!");
    return false;
  }

  vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount, nullptr);
  SwapChainImages.resize(imageCount);
  vkGetSwapchainImagesKHR(Device, Swapchain, &imageCount,
                          SwapChainImages.data());

  SwapchainImageFormat = surfaceFormat.format;
  SwapchainExtent = imageExtent;
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
