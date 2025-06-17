#pragma once
#include "openxr_program/openxr_session.h"
#include <functional>

void xr_loop(const std::function<bool(bool)> &runLoop, const Options &options,
             OpenXrSession &session, VkPhysicalDevice physicalDevice,
             uint32_t queueFamilyIndex, VkDevice device);
