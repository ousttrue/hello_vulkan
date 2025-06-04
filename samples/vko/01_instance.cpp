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
};

struct Device {
  VkDevice _device;
  ~Device() { vkDestroyDevice(_device, nullptr); }
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
    VkSurfaceKHR surface;
    VK_CHECK(glfwCreateWindowSurface(instance, window, nullptr, &surface));

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t graphicsFamiliy = UINT_MAX;
    uint32_t presentFamily = UINT_MAX;
    for (auto d : instance._devices) {
      auto _presentFamily = d.getPresentQueueFamily(surface);
      if (d._deviceProperties.deviceType ==
              VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU &&
          d._deviceFeatures.geometryShader && d._graphicsFamily != UINT_MAX &&
          _presentFamily) {
        if (physicalDevice == VK_NULL_HANDLE) {
          physicalDevice = d._physicaldevice;
          graphicsFamiliy = d._graphicsFamily;
          presentFamily = _presentFamily.value();
          LOGI("device: %s: graphics => %d, present => %d\n",
               d._deviceProperties.deviceName, d._graphicsFamily,
               presentFamily);
        } else {
          LOGI("device: %s\n", d._deviceProperties.deviceName);
        }
      } else {
        LOGE("device: %s: not supported\n", d._deviceProperties.deviceName);
      }
    }
    if (physicalDevice == VK_NULL_HANDLE || presentFamily == UINT_MAX) {
      return 2;
    }

    vko::Device device;
    device._validationLayers = instance._validationLayers;
    VK_CHECK(device.create(physicalDevice, graphicsFamiliy, presentFamily));

    //
    // main loop
    //
    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
