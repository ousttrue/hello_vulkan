#pragma once
#include <functional>
#include <openxr/openxr.h>

void xr_session(const std::function<bool(bool)> &runLoop, XrInstance instance,
                XrSystemId systemId, XrSession session, XrSpace appSpace);
