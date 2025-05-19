#pragma once
#include "platformplugin.h"

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Android(const std::shared_ptr<struct Options> &options,
                             const std::shared_ptr<struct PlatformData> &data);
