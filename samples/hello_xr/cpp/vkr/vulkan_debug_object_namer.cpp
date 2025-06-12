#include "vulkan_debug_object_namer.hpp"
#include <vulkan/vulkan_core.h>
#include <assert.h>

static PFN_vkSetDebugUtilsObjectNameEXT g_vkSetDebugUtilsObjectNameEXT{nullptr};

void SetDebugUtilsObjectNameEXT_GetProc(VkInstance instance) {
  g_vkSetDebugUtilsObjectNameEXT =
      (PFN_vkSetDebugUtilsObjectNameEXT)vkGetInstanceProcAddr(
          instance, "vkSetDebugUtilsObjectNameEXT");
}

VkResult SetDebugUtilsObjectNameEXT(VkDevice device, VkObjectType objectType,
                                    uint64_t objectHandle,
                                    const char *pObjectName) {
  VkDebugUtilsObjectNameInfoEXT nameInfo{
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_OBJECT_NAME_INFO_EXT,
      .pNext = nullptr,
      .objectType = objectType,
      .objectHandle = objectHandle,
      .pObjectName = pObjectName,
  };
  assert(g_vkSetDebugUtilsObjectNameEXT);
  return g_vkSetDebugUtilsObjectNameEXT(device, &nameInfo);
}
