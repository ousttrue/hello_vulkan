#pragma once
#include "openxr_program/openxr_session.h"
#include <functional>
#include <memory>

void xr_loop(const std::function<bool()> &runLoop, const Options &options,
             const std::shared_ptr<OpenXrSession> &session,
             VkPhysicalDevice physicalDevice, uint32_t queueFamilyIndex,
             VkDevice device);
