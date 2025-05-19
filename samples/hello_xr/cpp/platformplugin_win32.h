#pragma once
#include "platformplugin.h"

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Win32(const std::shared_ptr<Options> &options);
