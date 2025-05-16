#include "extensions.hpp"
#include "common.hpp"

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

VkDebugUtilsMessengerEXT CreateDebugUtilsMessengerEXT(VkInstance instance) {

  VkDebugUtilsMessengerEXT DebugMessenger;

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

  if (CreateDebugUtilsMessengerEXT(instance, &DebugUtilsMessengerCreateInfoEXT,
                                   nullptr, &DebugMessenger) == VK_SUCCESS) {
    return DebugMessenger;
  }

  LOGE("failed to set up debug messenger!");
  return VK_NULL_HANDLE;
}

void DestroyDebugUtilsMessengerEXT(VkInstance instance,
                                   VkDebugUtilsMessengerEXT debugMessenger,
                                   const VkAllocationCallbacks *pAllocator) {
  auto func = (PFN_vkDestroyDebugUtilsMessengerEXT)vkGetInstanceProcAddr(
      instance, "vkDestroyDebugUtilsMessengerEXT");
  if (func != nullptr) {
    func(instance, debugMessenger, pAllocator);
  }
}

///
/// InstanceExtensionManager
///
InstanceExtensionManager::InstanceExtensionManager(
    const std::vector<const char *> &requiredLayers,
    const std::vector<const char *> &requiredExtensions) {

  uint32_t instanceLayerCount;
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount, nullptr));
  InstanceLayers.resize(instanceLayerCount);
  VK_CHECK(vkEnumerateInstanceLayerProperties(&instanceLayerCount,
                                              InstanceLayers.data()));
  for (auto layer : requiredLayers) {
    pushLayer(layer);
  }

  uint32_t instanceExtensionCount;
  VK_CHECK(vkEnumerateInstanceExtensionProperties(
      nullptr, &instanceExtensionCount, nullptr));
  InstanceExtensions.resize(instanceExtensionCount);
  VK_CHECK(vkEnumerateInstanceExtensionProperties(
      nullptr, &instanceExtensionCount, InstanceExtensions.data()));
  for (auto extension : requiredExtensions) {
    pushExtension(extension);
  }
}

bool InstanceExtensionManager::pushLayer(const char *layerName) {
  for (auto available : InstanceLayers) {
    if (strcmp(available.layerName, layerName) == 0) {
      RequiredLayers.push_back(layerName);
      LOGI("[instance layer] %s: supported", layerName);
      return true;
    }
  }
  LOGE("[instance layer] %s: not supported", layerName);
  return false;
}

bool InstanceExtensionManager::pushExtension(const char *extensionName) {
  for (auto available : InstanceExtensions) {
    if (strcmp(available.extensionName, extensionName) == 0) {
      RequiredExtensions.push_back(extensionName);
      LOGI("[instance extension] %s: supported", extensionName);
      return true;
    }
  }
  LOGE("[instance extension] %s: not supported", extensionName);
  return false;
}

//
// DeviceExtensionManager
//
DeviceExtensionManager::DeviceExtensionManager(
    VkPhysicalDevice gpu, const std::vector<const char *> &requiredLayers,
    const std::vector<const char *> &requiredExtensions) {

  uint32_t deviceLayerCount;
  VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount, nullptr));
  DeviceLayers.resize(deviceLayerCount);
  VK_CHECK(vkEnumerateDeviceLayerProperties(gpu, &deviceLayerCount,
                                            DeviceLayers.data()));
  for (auto layer : requiredLayers) {
    pushLayer(layer);
  }

  uint32_t deviceExtensionCount;
  VK_CHECK(vkEnumerateDeviceExtensionProperties(
      gpu, nullptr, &deviceExtensionCount, nullptr));
  DeviceExtensions.resize(deviceExtensionCount);
  VK_CHECK(vkEnumerateDeviceExtensionProperties(
      gpu, nullptr, &deviceExtensionCount, DeviceExtensions.data()));
  for (auto extension : requiredExtensions) {
    pushExtension(extension);
  }
}

bool DeviceExtensionManager::pushLayer(const char *layerName) {
  for (auto available : DeviceLayers) {
    if (strcmp(available.layerName, layerName) == 0) {
      RequiredLayers.push_back(layerName);
      LOGI("[device layer] %s: supported", layerName);
      return true;
    }
  }
  LOGE("[device layer] %s: not supported", layerName);
  return false;
}

bool DeviceExtensionManager::pushExtension(const char *extensionName) {
  for (auto available : DeviceExtensions) {
    if (strcmp(available.extensionName, extensionName) == 0) {
      RequiredExtensions.push_back(extensionName);
      LOGI("[device extension] %s: supported", extensionName);
      return true;
    }
  }
  LOGE("[device extension] %s: not supported", extensionName);
  return false;
}

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

    if (!found) {
      LOGE("%s not supported", extension);
      return false;
    }
  }

  return true;
}
