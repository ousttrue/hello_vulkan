#pragma once

#include "util_egl.h"
#include "util_oxr.h"

struct AppEngine {
  struct android_app *m_app;
  XrInstance m_instance;
  XrSession m_session;
  XrSpace m_appSpace;
  XrSystemId m_systemId;
  std::vector<viewsurface_t> m_viewSurface;

  AppEngine(android_app *app, XrInstance instance, XrSystemId systemId,
            XrSession session, XrSpace appSpace);
  void UpdateFrame();
  void RenderFrame();
  bool RenderLayer(XrTime dpy_time,
                   std::vector<XrCompositionLayerProjectionView> &layerViews,
                   XrCompositionLayerProjection &layer);
};
