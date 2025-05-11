#include "vulfwk.h"
#include "logger.h"

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

VulkanFramework::VulkanFramework(const char *appName, const char *engineName) {
  _appName = appName;
  _engineName = engineName;
  LOGI("*** VulkanFramewor::VulkanFramework ***");
}

VulkanFramework::~VulkanFramework() {
  LOGI("*** VulkanFramewor::~VulkanFramework ***");
  cleanup();
}

bool VulkanFramework::initialize(
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
  if (DebugUtilsMessengerEXT) {
    DestroyDebugUtilsMessengerEXT(Instance, DebugUtilsMessengerEXT, nullptr);
  }
  if (Instance != VK_NULL_HANDLE) {
    vkDestroyInstance(Instance, nullptr);
  }
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
      .pNext = &DebugUtilsMessengerCreateInfoEXT,
      .pApplicationInfo = &ApplicationInfo,
      .enabledLayerCount = static_cast<uint32_t>(layerNames.size()),
      .ppEnabledLayerNames = layerNames.data(),
      .enabledExtensionCount = static_cast<uint32_t>(extensionNames.size()),
      .ppEnabledExtensionNames = extensionNames.data(),
  };

  if (vkCreateInstance(&InstanceCreateInfo, nullptr, &Instance) != VK_SUCCESS) {
    LOGE("failed to create instance!");
    return false;
  }

  if (CreateDebugUtilsMessengerEXT(Instance, &DebugUtilsMessengerCreateInfoEXT,
                                   nullptr,
                                   &DebugUtilsMessengerEXT) != VK_SUCCESS) {
    LOGE("failed to set up debug messenger!");
    return false;
  }

  return true;
}
