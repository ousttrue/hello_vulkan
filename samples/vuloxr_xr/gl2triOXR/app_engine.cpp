#include "app_engine.h"
#include "render_scene.h"

AppEngine::AppEngine(XrInstance instance, XrSystemId systemId,
                     XrSession session, XrSpace appSpace)
    : m_instance(instance), m_systemId(systemId), m_session(session),
      m_appSpace(appSpace) {
  init_gles_scene();
}

AppEngine::~AppEngine() {}

void AppEngine::RenderLayer(XrTime dpy_time, uint32_t i,
                            const XrCompositionLayerProjectionView &layerView,
                            uint32_t fbo_id) {
  render_gles_scene(fbo_id, layerView.subImage.imageRect);
}
