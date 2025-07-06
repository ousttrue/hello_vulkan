#pragma once

#include <openxr/openxr.h>
#include <string>

class AppEngine {
  XrInstance m_instance;
  XrSystemId m_systemId;
  XrSession m_session;
  XrSpace m_appSpace;
  XrSpace m_stageSpace;
  XrSpace m_viewSpace;

  std::string m_runtime_name;
  std::string m_system_name;

public:
  AppEngine(XrInstance instance, XrSystemId systemId, XrSession session,
            XrSpace appSpace);
  ~AppEngine();

  void RenderLayer(XrTime dpy_time, XrTime elapsed_us, uint32_t i,
                   const XrCompositionLayerProjectionView &layerView,
                   const XrView &view,
                   uint32_t fbo_id);
};
