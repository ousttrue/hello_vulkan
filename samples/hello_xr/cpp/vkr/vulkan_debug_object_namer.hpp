#pragma once
#include <vulkan/vulkan.h>

void SetDebugUtilsObjectNameEXT_GetProc(VkInstance instance);
VkResult SetDebugUtilsObjectNameEXT(VkDevice device, VkObjectType objectType,
                                    uint64_t objectHandle,
                                    const char *pObjectName);
