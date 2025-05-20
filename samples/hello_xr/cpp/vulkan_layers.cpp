#include "vulkan_layers.h"
#include "logger.h"
#include <stdexcept>
#include <vulkan/vulkan.h>

static const char *GetValidationLayerName() {
  uint32_t layerCount;
  vkEnumerateInstanceLayerProperties(&layerCount, nullptr);
  std::vector<VkLayerProperties> availableLayers(layerCount);
  vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());

  std::vector<const char *> validationLayerNames;
  validationLayerNames.push_back("VK_LAYER_KHRONOS_validation");
  validationLayerNames.push_back("VK_LAYER_LUNARG_standard_validation");

  // Enable only one validation layer from the list above. Prefer KHRONOS.
  for (auto &validationLayerName : validationLayerNames) {
    for (const auto &layerProperties : availableLayers) {
      if (0 == strcmp(validationLayerName, layerProperties.layerName)) {
        return validationLayerName;
      }
    }
  }

  return nullptr;
}

std::vector<const char *> getVulkanLayers() {
#ifndef NDEBUG
  // debug
  if (auto layer = GetValidationLayerName()) {
    // has layer
    return {layer};
  }
  Log::Write(Log::Level::Warning,
             "No validation layers found in the system, skipping");
#endif

  // no debug layer
  return {};
}

std::vector<const char *> getVulkanInstanceExtensions() {
  std::vector<const char *> extensions;

  uint32_t extensionCount = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                             nullptr) != VK_SUCCESS) {
    throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
  }

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  if (vkEnumerateInstanceExtensionProperties(
          nullptr, &extensionCount, availableExtensions.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
  }
  const auto b = availableExtensions.begin();
  const auto e = availableExtensions.end();

  auto isExtSupported = [&](const char *extName) -> bool {
    auto it = std::find_if(b, e, [&](const VkExtensionProperties &properties) {
      return (0 == strcmp(extName, properties.extensionName));
    });
    return (it != e);
  };

  // Debug utils is optional and not always available
  if (isExtSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  // TODO add back VK_EXT_debug_report code for compatibility with older
  // systems? (Android)

#if defined(USE_MIRROR_WINDOW)
  extensions.push_back("VK_KHR_surface");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  extensions.push_back("VK_KHR_win32_surface");
#else
#error CreateSurface not supported on this OS
#endif // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif // defined(USE_MIRROR_WINDOW)

  return extensions;
}

std::vector<const char *> getVulkanDeviceExtensions() {
  std::vector<const char *> deviceExtensions;
#if defined(USE_MIRROR_WINDOW)
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif
  return deviceExtensions;
}
