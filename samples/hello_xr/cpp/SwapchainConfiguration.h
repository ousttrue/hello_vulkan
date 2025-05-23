#pragma once
#include <openxr/openxr.h>
#include <vector>

struct SwapchainConfiguration {
  std::vector<XrViewConfigurationView> Views;
  std::vector<int64_t> Formats;
};
