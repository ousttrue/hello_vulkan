#include "platform_win32.h"

#include <iostream>

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
  std::cerr << "validation layer: " << pCallbackData->pMessage << std::endl;

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

static void populateDebugMessengerCreateInfo(
    VkDebugUtilsMessengerCreateInfoEXT &createInfo) {
  createInfo = {};
  createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
  createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                               VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
  createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                           VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
  createInfo.pfnUserCallback = debugCallback;
}

class VulkanFramework {
  std::string _appName;
  std::string _engineName;
  VkApplicationInfo ApplicationInfo{};
  VkInstanceCreateInfo InstanceCreateInfo{};
  VkInstance Instance = VK_NULL_HANDLE;

public:
  VulkanFramework(const char *appName, const char *engineName) {
    _appName = appName;
    _engineName = engineName;
    std::cout << "*** VulkanFramewor::VulkanFramework ***" << std::endl;
  }
  ~VulkanFramework() {
    std::cout << "*** VulkanFramewor::~VulkanFramework ***" << std::endl;
    cleanup();
  }

  bool initialize(const std::vector<const char *> &layerNames,
                  const VkDebugUtilsMessengerCreateInfoEXT
                      *pDebugUtilsMessengerCreateInfoEXT,
                  const std::vector<const char *> &extensionNames) {
    for (auto name : layerNames) {
      std::cout << "[layer]" << name << std::endl;
    }
    if (!checkValidationLayerSupport(layerNames)) {
      std::cout << "validation layers requested, but not available!"
                << std::endl;
      return false;
    }
    for (auto name : extensionNames) {
      std::cout << "[extension]" << name << std::endl;
    }
    if (!createInstance(layerNames, pDebugUtilsMessengerCreateInfoEXT,
                        extensionNames)) {
      return false;
    }
    return true;
  }

  void cleanup() {
    if (Instance != VK_NULL_HANDLE) {
      vkDestroyInstance(Instance, nullptr);
    }
  }

private:
  bool createInstance(const std::vector<const char *> &layerNames,
                      const VkDebugUtilsMessengerCreateInfoEXT
                          *pDebugUtilsMessengerCreateInfoEXT,
                      const std::vector<const char *> &extensionNames) {
    ApplicationInfo = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = _appName.c_str(),
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = _engineName.c_str(),
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_0,
    };

    InstanceCreateInfo = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pNext = pDebugUtilsMessengerCreateInfoEXT,
        .pApplicationInfo = &ApplicationInfo,
        .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
        .ppEnabledLayerNames = layerNames.data(),
        .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
        .ppEnabledExtensionNames = extensionNames.data(),
    };
    if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) !=
        VK_SUCCESS) {
      return false;
    }
    return true;
  }
};

int main(int argc, char **argv) {

  std::vector<const char *> validationLayers;
  std::vector<const char *> extensions;
  VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

#ifndef NDEBUG
  std::cout << "[debug build]" << std::endl;
  validationLayers.push_back("VK_LAYER_KHRONOS_validation");
  populateDebugMessengerCreateInfo(debugCreateInfo);
  extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
#endif

  Glfw glfw;
  glfw.createWindow(640, 480, "vulfwk");
  glfw.addRequiredExtensions(extensions);

  VulkanFramework vulfwk("vulfwk", "No Engine");
  if (!vulfwk.initialize(validationLayers, &debugCreateInfo, extensions)) {
    std::cout << "failed" << std::endl;
    return 1;
  }
  std::cout << "ok" << std::endl;

  return 0;
}
