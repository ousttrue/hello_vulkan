#pragma once
#include <openxr/openxr.h>
#include <string>

XrReferenceSpaceCreateInfo
GetXrReferenceSpaceCreateInfo(const std::string &referenceSpaceTypeStr);
