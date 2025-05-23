#pragma once
#include <vulkan/vulkan_core.h>

VKAPI_ATTR VkBool32 VKAPI_CALL debugMessageThunk(
    VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
    VkDebugUtilsMessageTypeFlagsEXT messageTypes,
    const VkDebugUtilsMessengerCallbackDataEXT *pCallbackData, void *pUserData);
