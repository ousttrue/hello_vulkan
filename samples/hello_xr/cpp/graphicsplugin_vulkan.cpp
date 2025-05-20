// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0
#include "CmdBuffer.h"
#include "CreateGraphicsPlugin_Vulkan.h"
#include "DepthBuffer.h"
#include "MemoryAllocator.h"
#include "Pipeline.h"
#include "RenderPass.h"
#include "SwapchainImageContext.h"
#include "VertexBuffer.h"
#include "vulkan_debug_object_namer.hpp"

#include <stdexcept>
#include <vulkan/vulkan_core.h>
#ifdef XR_USE_PLATFORM_WIN32
#include <unknwn.h>
#include <windows.h>
#endif

#include "check.h"
#include "graphicsplugin_vulkan.h"
#include "logger.h"
#include "options.h"

#include "vk_utils.h"
#include <vulkan/vulkan.h>

#ifdef XR_USE_PLATFORM_ANDROID
#include <android_native_app_glue.h>
#endif

#include <openxr/openxr_platform.h>

#include <algorithm>
#include <list>
#include <map>

#ifdef XR_USE_GRAPHICS_API_VULKAN
#include <common/xr_linear.h>

#ifdef USE_ONLINE_VULKAN_SHADERC
#include <shaderc/shaderc.hpp>
#endif

#if defined(VK_USE_PLATFORM_WIN32_KHR)
// Define USE_MIRROR_WINDOW to open a otherwise-unused window for e.g. RenderDoc
#define USE_MIRROR_WINDOW
#endif

#include "VulkanGraphicsPlugin.h"

std::shared_ptr<IGraphicsPlugin>
CreateGraphicsPlugin_Vulkan(const std::shared_ptr<Options> &options,
                            std::shared_ptr<IPlatformPlugin> platformPlugin) {
  return std::make_shared<VulkanGraphicsPlugin>(options,
                                                std::move(platformPlugin));
}

#endif // XR_USE_GRAPHICS_API_VULKAN
