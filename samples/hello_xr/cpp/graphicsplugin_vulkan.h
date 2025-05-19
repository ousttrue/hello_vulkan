#pragma once
#include "graphicsplugin.h"
#include "platformplugin.h"

std::shared_ptr<IGraphicsPlugin>
CreateGraphicsPlugin_Vulkan(const std::shared_ptr<Options> &options,
                            std::shared_ptr<IPlatformPlugin> platformPlugin);
