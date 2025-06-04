#include <climits>
#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include "common.hpp"
#include <GLFW/glfw3.h>

#include <optional>
#include <set>
#include <vector>

auto WINDOW_TITLE = "vko";

namespace vko {

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Physical_devices_and_queue_families
struct PhysicalDevice {
  VkPhysicalDevice _physicaldevice;
  VkPhysicalDeviceProperties _deviceProperties;
  VkPhysicalDeviceFeatures _deviceFeatures;
  std::vector<VkQueueFamilyProperties> _queueFamilies;
  uint32_t _graphicsFamily = UINT_MAX;
  PhysicalDevice(VkPhysicalDevice physicalDevice)
      : _physicaldevice(physicalDevice) {
    vkGetPhysicalDeviceProperties(_physicaldevice, &_deviceProperties);
    vkGetPhysicalDeviceFeatures(_physicaldevice, &_deviceFeatures);
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             nullptr);
    _queueFamilies.resize(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount,
                                             _queueFamilies.data());

    for (uint32_t i = 0; i < _queueFamilies.size(); ++i) {
      if (_queueFamilies[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) {
        _graphicsFamily = i;
        break;
      }
    }
  }
  std::optional<uint32_t> getPresentQueueFamily(VkSurfaceKHR surface) {
    for (uint32_t i = 0; i < _queueFamilies.size(); ++i) {
      VkBool32 presentSupport = false;
      vkGetPhysicalDeviceSurfaceSupportKHR(_physicaldevice, i, surface,
                                           &presentSupport);
      if (presentSupport) {
        return i;
      }
    }
    return {};
  }
};

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
struct Instance {
  VkInstance _instance = VK_NULL_HANDLE;
  operator VkInstance() { return _instance; }
  ~Instance() {
    if (_debugUtilsMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, nullptr);
    }
    if (_instance != VK_NULL_HANDLE) {
      vkDestroyInstance(_instance, nullptr);
    }
  }

  std::vector<PhysicalDevice> _devices;

  std::vector<const char *> _validationLayers;
  std::vector<const char *> _instanceExtensions;

  // VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  VkDebugUtilsMessengerEXT _debugUtilsMessenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT _debug_messenger_create_info = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback =
          [](VkDebugUtilsMessageSeverityFlagBitsEXT message_severity,
             VkDebugUtilsMessageTypeFlagsEXT message_types,
             const VkDebugUtilsMessengerCallbackDataEXT *callback_data,
             void *user_data) {
            if (message_severity >=
                VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
              LOGE("%s %s\n", callback_data->pMessageIdName,
                   callback_data->pMessage);
            }
            return VK_FALSE;
          },
  };

  static VkResult CreateDebugUtilsMessengerEXT(
      VkInstance instance,
      const VkDebugUtilsMessengerCreateInfoEXT *pCreateInfo,
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
  DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                VkDebugUtilsMessengerEXT debugMessenger,
                                const VkAllocationCallbacks *pAllocator) {
    auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
        instance, "vkDestroyDebugUtilsMessengerEXT");
    if (func != nullptr) {
      func(instance, debugMessenger, pAllocator);
    }
  }

  VkApplicationInfo _appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "VKO",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo _createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &_appInfo,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
  };

  VkResult create() {
    if (_validationLayers.size() > 0) {
      for (auto name : _validationLayers) {
        LOGI("instance layer: %s\n", name);
      }
      _createInfo.enabledLayerCount = _validationLayers.size();
      _createInfo.ppEnabledLayerNames = _validationLayers.data();
    }
    if (_instanceExtensions.size() > 0) {
      for (auto name : _instanceExtensions) {
        LOGI("instance extension: %s\n", name);
        if (strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
          _createInfo.pNext = &_debug_messenger_create_info;
        }
      }
      _createInfo.enabledExtensionCount = _instanceExtensions.size();
      _createInfo.ppEnabledExtensionNames = _instanceExtensions.data();
    }

    auto result = vkCreateInstance(&_createInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
      return result;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      LOGE("no physical device\n");
    } else {
      std::vector<VkPhysicalDevice> devices(deviceCount);
      vkEnumeratePhysicalDevices(_instance, &deviceCount, devices.data());
      for (auto d : devices) {
        _devices.push_back(PhysicalDevice(d));
      }
    }

    result =
        CreateDebugUtilsMessengerEXT(_instance, &_debug_messenger_create_info,
                                     nullptr, &_debugUtilsMessenger);
    return result;
  }

  struct SelectedPhysicalDevice {
    VkPhysicalDevice physicalDevice;
    uint32_t graphicsFamily;
    uint32_t presentFamily;
  };

  SelectedPhysicalDevice pickPhysicakDevice(VkSurfaceKHR surface) {
    SelectedPhysicalDevice ret{
        .physicalDevice = VK_NULL_HANDLE,
        .graphicsFamily = UINT_MAX,
        .presentFamily = UINT_MAX,
    };
    for (auto d : _devices) {
      auto _presentFamily = d.getPresentQueueFamily(surface);
      if (d._deviceProperties.deviceType ==
              VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
          d._deviceFeatures.geometryShader && d._graphicsFamily != UINT_MAX &&
          _presentFamily) {
        auto selected = ret.physicalDevice == VK_NULL_HANDLE;
        if (selected) {
          ret.physicalDevice = d._physicaldevice;
          ret.graphicsFamily = d._graphicsFamily;
          ret.presentFamily = _presentFamily.value();
        }
        LOGI("physical device: %s: graphics => %d, present => %d, %s\n",
             d._deviceProperties.deviceName, d._graphicsFamily,
             _presentFamily.value(), (selected ? "select" : ""));
      } else {
        LOGE("physical device: %s: not supported\n",
             d._deviceProperties.deviceName);
      }
    }
    return ret;
  }
};

struct Device {
  VkDevice _device = VK_NULL_HANDLE;
  operator VkDevice() { return _device; }
  ~Device() {
    if (_device != VK_NULL_HANDLE) {
      vkDestroyDevice(_device, nullptr);
    }
  }
  std::vector<const char *> _validationLayers;
  std::vector<const char *> _deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
  float _queuePriority = 1.0f;
  std::vector<VkDeviceQueueCreateInfo> _queueCreateInfos;
  VkPhysicalDeviceFeatures _deviceFeatures{};
  VkDeviceCreateInfo _createInfo{
      .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
      .queueCreateInfoCount = 0,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
      .pEnabledFeatures = &_deviceFeatures,
  };

  VkResult create(VkPhysicalDevice physicalDevice, uint32_t graphics,
                  uint32_t present) {
    if (_validationLayers.size() > 0) {
      for (auto name : _validationLayers) {
        LOGI("device layer: %s\n", name);
      }
      _createInfo.enabledLayerCount = _validationLayers.size();
      _createInfo.ppEnabledLayerNames = _validationLayers.data();
    }
    if (_deviceExtensions.size() > 0) {
      for (auto name : _deviceExtensions) {
        LOGI("device extension: %s\n", name);
      }
      _createInfo.enabledExtensionCount = _deviceExtensions.size();
      _createInfo.ppEnabledExtensionNames = _deviceExtensions.data();
    }

    std::set<uint32_t> uniqueQueueFamilies = {graphics, present};
    for (uint32_t queueFamily : uniqueQueueFamilies) {
      VkDeviceQueueCreateInfo queueCreateInfo{
          .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
          .queueFamilyIndex = queueFamily,
          .queueCount = 1,
          .pQueuePriorities = &_queuePriority,
      };
      _queueCreateInfos.push_back(queueCreateInfo);
    }
    _createInfo.queueCreateInfoCount =
        static_cast<uint32_t>(_queueCreateInfos.size());
    _createInfo.pQueueCreateInfos = _queueCreateInfos.data();

    return vkCreateDevice(physicalDevice, &_createInfo, nullptr, &_device);
  }
};

struct Surface {
  VkInstance _instance;
  VkSurfaceKHR _surface;
  VkPhysicalDevice _physicalDevice;
  VkSurfaceCapabilitiesKHR _capabilities;
  std::vector<VkSurfaceFormatKHR> _formats;
  std::vector<VkPresentModeKHR> _presentModes;

  Surface(VkInstance instance, VkSurfaceKHR surface, VkPhysicalDevice gpu)
      : _instance(instance), _surface(surface), _physicalDevice(gpu) {

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(gpu, surface, &_capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount, nullptr);
    if (formatCount != 0) {
      _formats.resize(formatCount);
      vkGetPhysicalDeviceSurfaceFormatsKHR(gpu, surface, &formatCount,
                                           _formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount,
                                              nullptr);
    if (presentModeCount != 0) {
      _presentModes.resize(presentModeCount);
      vkGetPhysicalDeviceSurfacePresentModesKHR(gpu, surface, &presentModeCount,
                                                _presentModes.data());
    }
  }

  ~Surface() { vkDestroySurfaceKHR(_instance, _surface, nullptr); }

  VkSurfaceFormatKHR chooseSwapSurfaceFormat() const {
    for (const auto &availableFormat : _formats) {
      if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
          availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
        return availableFormat;
      }
    }
    return _formats[0];
  }

  VkPresentModeKHR chooseSwapPresentMode() const {
    for (const auto &availablePresentMode : _presentModes) {
      if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
        return availablePresentMode;
      }
    }
    return VK_PRESENT_MODE_FIFO_KHR;
  }
};

struct Swapchain {
  VkDevice _device;
  VkSwapchainKHR _swapchain = VK_NULL_HANDLE;
  Swapchain(VkDevice device) : _device(device) {}
  ~Swapchain() {
    if (_swapchain != VK_NULL_HANDLE) {
      vkDestroySwapchainKHR(_device, _swapchain, nullptr);
    }
  }

  uint32_t _queueFamilyIndices[2] = {};
  VkSwapchainCreateInfoKHR _createInfo{
      .sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR,
      .imageArrayLayers = 1,
      .imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
      .compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
      .clipped = VK_TRUE,
  };

  VkResult create(VkSurfaceKHR surface, uint32_t imageCount,
                  VkSurfaceFormatKHR surfaceFormat,
                  VkPresentModeKHR presentMode,
                  const VkSurfaceCapabilitiesKHR &capabilities,
                  uint32_t graphicsFamily, uint32_t presentFamily,
                  VkSwapchainKHR oldSwapchain = VK_NULL_HANDLE) {
    _createInfo.surface = surface;
    _createInfo.minImageCount = imageCount;
    _createInfo.imageFormat = surfaceFormat.format;
    _createInfo.imageColorSpace = surfaceFormat.colorSpace;
    _createInfo.imageExtent = capabilities.currentExtent;
    if (graphicsFamily == presentFamily) {
      _createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
      _createInfo.queueFamilyIndexCount = 0;     // Optional
      _createInfo.pQueueFamilyIndices = nullptr; // Optional
    } else {
      _createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
      _createInfo.queueFamilyIndexCount = 2;
      _queueFamilyIndices[0] = graphicsFamily;
      _queueFamilyIndices[1] = presentFamily;
      _createInfo.pQueueFamilyIndices = _queueFamilyIndices;
    }
    _createInfo.preTransform = capabilities.currentTransform;
    _createInfo.presentMode = presentMode;
    _createInfo.oldSwapchain = oldSwapchain;
    return vkCreateSwapchainKHR(_device, &_createInfo, nullptr, &_swapchain);
  }
};

} // namespace vko

// https://github.com/ocornut/imgui/blob/master/examples/example_glfw_vulkan/main.cpp
int main(int argc, char **argv) {
  glfwSetErrorCallback([](int error, const char *description) {
    LOGE("GLFW Error %d: %s\n", error, description);
  });
  if (!glfwInit()) {
    return 1;
  }
  // Create window with Vulkan context
  glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
  GLFWwindow *window =
      glfwCreateWindow(1280, 720, WINDOW_TITLE, nullptr, nullptr);
  if (!glfwVulkanSupported()) {
    printf("GLFW: Vulkan Not Supported\n");
    return 1;
  }

  {
    //
    // vulkan instance with debug layer
    //
    vko::Instance instance;
    // vulkan extension for glfw surface
    uint32_t extensions_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++) {
      instance._instanceExtensions.push_back(glfw_extensions[i]);
    }
#ifndef NDEBUG
    instance._validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instance._instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif
    VK_CHECK(instance.create());

    //
    // vulkan device with glfw surface
    //
    VkSurfaceKHR _surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &_surface));
    auto picked = instance.pickPhysicakDevice(_surface);
    vko::Surface surface(instance, _surface, picked.physicalDevice);

    vko::Device device;
    device._validationLayers = instance._validationLayers;
    VK_CHECK(device.create(picked.physicalDevice, picked.graphicsFamily,
                           picked.presentFamily));

    vko::Swapchain swapchain(device);
    VK_CHECK(swapchain.create(
        surface._surface, surface._capabilities.minImageCount + 1,
        surface.chooseSwapSurfaceFormat(), surface.chooseSwapPresentMode(),
        surface._capabilities, picked.graphicsFamily, picked.presentFamily));

    //
    // main loop
    //
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
