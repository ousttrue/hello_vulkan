#pragma once
#include <memory>

std::shared_ptr<struct IGraphicsPlugin> CreateGraphicsPlugin_Vulkan(
    const std::shared_ptr<struct Options> &options,
    std::shared_ptr<struct IPlatformPlugin> platformPlugin);


