#pragma once
#include "platformplugin.h"
#include <memory>

std::shared_ptr<IPlatformPlugin>
CreatePlatformPlugin_Win32(const struct Options &options);
