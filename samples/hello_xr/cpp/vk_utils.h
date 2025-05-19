#pragma once
#include <array>
#include <stdexcept>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_core.h>

std::string vkResultString(VkResult res);

[[noreturn]] void ThrowVkResult(VkResult res, const char *originator = nullptr,
                                const char *sourceLocation = nullptr);

VkResult CheckVkResult(VkResult res, const char *originator = nullptr,
                       const char *sourceLocation = nullptr);




