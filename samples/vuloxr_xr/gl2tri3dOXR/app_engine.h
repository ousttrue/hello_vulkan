#pragma once
#include <openxr/openxr.h>

class AppEngine {
  XrInstance m_instance;
  XrSession m_session;
  XrSpace m_appSpace;
  XrSpace m_stageSpace;
  XrSystemId m_systemId;

public:
  AppEngine(XrInstance instance, XrSystemId systemId, XrSession session,
            XrSpace appSpace);
  ~AppEngine();
  void RenderLayer(XrTime dpy_time, uint32_t i,
                   const XrCompositionLayerProjectionView &layerView,
                   uint32_t fbo_id);
};
