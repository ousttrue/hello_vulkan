#pragma once
#include "platformplugin.h"

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Win32(const Options &options);
