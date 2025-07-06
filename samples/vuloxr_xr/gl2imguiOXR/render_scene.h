#pragma once
#include "app_engine.h"
#include <string>

typedef struct scene_data_t {
  std::string runtime_name;
  std::string system_name;
  const uint8_t *gl_version;
  const uint8_t *gl_vendor;
  const uint8_t *gl_render;

  uint32_t elapsed_us;
  float interval_ms;

  XrRect2Di viewport;
  const XrView *view;
  uint32_t viewID;
} scene_data_t;

int init_gles_scene();
int render_gles_scene(const XrCompositionLayerProjectionView &layerView,
                      uint32_t fbo_id, const XrPosef &viewPose,
                      const XrPosef &stagePose, scene_data_t &sceneData);
