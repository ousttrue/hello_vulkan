#pragma once
#include <functional>
#include <memory>
#include <openxr/openxr.h>
#include <vulkan/vulkan.h>

void xr_loop(const std::function<std::tuple<bool, XrFrameState>()> &runLoop,
             const std::shared_ptr<class OpenXrSession> &session,
             const struct Options &options, VkPhysicalDevice physicalDevice,
             uint32_t queueFamilyIndex, VkDevice device);
