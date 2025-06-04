#include <vulkan/vulkan_core.h>
#define GLFW_INCLUDE_NONE
#define GLFW_INCLUDE_VULKAN
#include "common.hpp"
#include <GLFW/glfw3.h>

#include <vector>

auto WINDOW_TITLE = "vko";

namespace vko {

// https://vulkan-tutorial.com/Drawing_a_triangle/Setup/Validation_layers
struct Instance {
  VkInstance _instance = VK_NULL_HANDLE;
  std::vector<VkPhysicalDevice> _devices;

  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;

  // VK_EXT_DEBUG_UTILS_EXTENSION_NAME
  VkDebugUtilsMessengerEXT _debugUtilsMessenger = VK_NULL_HANDLE;
  VkDebugUtilsMessengerCreateInfoEXT debug_messenger_create_info = {
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

  VkApplicationInfo appInfo{
      .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
      .pApplicationName = "VKO",
      .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
      .pEngineName = "No Engine",
      .engineVersion = VK_MAKE_VERSION(1, 0, 0),
      .apiVersion = VK_API_VERSION_1_0,
  };

  VkInstanceCreateInfo createInfo{
      .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
      .pNext = nullptr,
      .pApplicationInfo = &appInfo,
      .enabledLayerCount = 0,
      .enabledExtensionCount = 0,
  };

  ~Instance() {
    if (_debugUtilsMessenger != VK_NULL_HANDLE) {
      DestroyDebugUtilsMessengerEXT(_instance, _debugUtilsMessenger, nullptr);
    }
    if (_instance != VK_NULL_HANDLE) {
      vkDestroyInstance(_instance, nullptr);
    }
  }

  VkResult create() {
    if (validationLayers.size() > 0) {
      for (auto name : validationLayers) {
        LOGI("layer: %s\n", name);
      }
      createInfo.enabledLayerCount = validationLayers.size();
      createInfo.ppEnabledLayerNames = validationLayers.data();
    }
    if (instanceExtensions.size() > 0) {
      for (auto name : instanceExtensions) {
        LOGI("instance extension: %s\n", name);
        if (strcmp(name, VK_EXT_DEBUG_UTILS_EXTENSION_NAME) == 0) {
          createInfo.pNext = &debug_messenger_create_info;
        }
      }
      createInfo.enabledExtensionCount = instanceExtensions.size();
      createInfo.ppEnabledExtensionNames = instanceExtensions.data();
    }

    auto result = vkCreateInstance(&createInfo, nullptr, &_instance);
    if (result != VK_SUCCESS) {
      return result;
    }

    uint32_t deviceCount = 0;
    vkEnumeratePhysicalDevices(_instance, &deviceCount, nullptr);
    if (deviceCount == 0) {
      LOGE("no physical device\n");
    } else {
      _devices.resize(deviceCount);
      vkEnumeratePhysicalDevices(_instance, &deviceCount, _devices.data());
    }

    result =
        CreateDebugUtilsMessengerEXT(_instance, &debug_messenger_create_info,
                                     nullptr, &_debugUtilsMessenger);
    return result;
  }

  void debugPrint() {
    for (auto device : _devices) {
      VkPhysicalDeviceProperties prop;
      vkGetPhysicalDeviceProperties(device, &prop);
      LOGI("device: %s\n", prop.deviceName);
    }
  }
};

} // namespace vko

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
    vko::Instance instance;

    // vulkan extension for glfw surface
    uint32_t extensions_count = 0;
    auto glfw_extensions = glfwGetRequiredInstanceExtensions(&extensions_count);
    for (uint32_t i = 0; i < extensions_count; i++) {
      instance.instanceExtensions.push_back(glfw_extensions[i]);
    }

#ifndef NDEBUG
    instance.validationLayers.push_back("VK_LAYER_KHRONOS_validation");
    instance.instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

    VK_CHECK(instance.create());
    instance.debugPrint();

    while (!glfwWindowShouldClose(window)) {
      glfwPollEvents();
    }
  }

  glfwDestroyWindow(window);
  glfwTerminate();
  return 0;
}
