#include "VulkanDebugMessageThunk.h"
#include <common/fmt.h>
#include <common/logger.h>
#include <vulkan/vulkan_core.h>

static std::string vkObjectTypeToString(VkObjectType objectType) {
  std::string objName;

#define LIST_OBJECT_TYPES(_)                                                   \
  _(UNKNOWN)                                                                   \
  _(INSTANCE)                                                                  \
  _(PHYSICAL_DEVICE)                                                           \
  _(DEVICE)                                                                    \
  _(QUEUE)                                                                     \
  _(SEMAPHORE)                                                                 \
  _(COMMAND_BUFFER)                                                            \
  _(FENCE)                                                                     \
  _(DEVICE_MEMORY)                                                             \
  _(BUFFER)                                                                    \
  _(IMAGE)                                                                     \
  _(EVENT)                                                                     \
  _(QUERY_POOL)                                                                \
  _(BUFFER_VIEW)                                                               \
  _(IMAGE_VIEW)                                                                \
  _(SHADER_MODULE)                                                             \
  _(PIPELINE_CACHE)                                                            \
  _(PIPELINE_LAYOUT)                                                           \
  _(RENDER_PASS)                                                               \
  _(PIPELINE)                                                                  \
  _(DESCRIPTOR_SET_LAYOUT)                                                     \
  _(SAMPLER)                                                                   \
  _(DESCRIPTOR_POOL)                                                           \
  _(DESCRIPTOR_SET)                                                            \
  _(FRAMEBUFFER)                                                               \
  _(COMMAND_POOL)                                                              \
  _(SURFACE_KHR)                                                               \
  _(SWAPCHAIN_KHR)                                                             \
  _(DISPLAY_KHR)                                                               \
  _(DISPLAY_MODE_KHR)                                                          \
  _(DESCRIPTOR_UPDATE_TEMPLATE_KHR)                                            \
  _(DEBUG_UTILS_MESSENGER_EXT)

  switch (objectType) {
  default:
#define MK_OBJECT_TYPE_CASE(name)                                              \
  case VK_OBJECT_TYPE_##name:                                                  \
    objName = #name;                                                           \
    break;
    LIST_OBJECT_TYPES(MK_OBJECT_TYPE_CASE)
  }

  return objName;
}

static VkBool32
debugMessage(VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
             VkDebugUtilsMessageTypeFlagsEXT messageTypes,
             const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData) {
  std::string flagNames;
  std::string objName;
  Log::Level level = Log::Level::None;

  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT) !=
      0u) {
    flagNames += "DEBUG:";
    level = Log::Level::Verbose;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) != 0u) {
    flagNames += "INFO:";
    level = Log::Level::Info;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) !=
      0u) {
    flagNames += "WARN:";
    level = Log::Level::Warning;
  }
  if ((messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) != 0u) {
    flagNames += "ERROR:";
    level = Log::Level::Error;
  }
  if ((messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) != 0u) {
    flagNames += "PERF:";
    level = Log::Level::Warning;
  }

  if (level != Log::Level::None) {
    uint64_t object = 0;
    // skip loader messages about device extensions
    if (pCallbackData->objectCount > 0) {
      auto objectType = pCallbackData->pObjects[0].objectType;
      if ((objectType == VK_OBJECT_TYPE_INSTANCE) &&
          (strncmp(pCallbackData->pMessage, "Device Extension:", 17) == 0)) {
        return VK_FALSE;
      }
      objName = vkObjectTypeToString(objectType);
      object = pCallbackData->pObjects[0].objectHandle;
      if (pCallbackData->pObjects[0].pObjectName != nullptr) {
        objName += " " + std::string(pCallbackData->pObjects[0].pObjectName);
      }
    }
    Log::Write(level, Fmt("%s (%s 0x%llx) %s", flagNames.c_str(),
                          objName.c_str(), object, pCallbackData->pMessage));
  }

  return VK_FALSE;
}

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageThunk(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *) {
  return debugMessage(messageSeverity, messageTypes, pCallbackData);
}
