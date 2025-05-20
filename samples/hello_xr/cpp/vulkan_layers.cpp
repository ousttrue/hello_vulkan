#include "vulkan_layers.h"
#include "logger.h"
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
