#pragma once
#include "platformplugin.h"

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Android(const std::shared_ptr<Options> &options,
                             const std::shared_ptr<PlatformData> &data);
