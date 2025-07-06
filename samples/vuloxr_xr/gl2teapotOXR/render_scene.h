#pragma once
#include <openxr/openxr.h>

int init_gles_scene();
int render_gles_scene(const XrCompositionLayerProjectionView &layerView,
                      uint32_t fbo_id, const XrPosef &stagePose, XrTime elapsed_us,
                      uint32_t viewID);
