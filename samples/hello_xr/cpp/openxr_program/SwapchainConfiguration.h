#pragma once
#include <openxr/openxr.h>
#include <vector>

struct SwapchainConfiguration {
  std::vector<XrViewConfigurationView> Views;
  int64_t Format;
};
