#include "app_engine.h"
#include "render_scene.h"

static XrSpace oxr_create_ref_space(XrSession session,
                             XrReferenceSpaceType ref_space_type) {
  XrSpace space;
  XrReferenceSpaceCreateInfo ci = {XR_TYPE_REFERENCE_SPACE_CREATE_INFO};
  ci.referenceSpaceType = ref_space_type;
  ci.poseInReferenceSpace.orientation.w = 1;
  xrCreateReferenceSpace(session, &ci, &space);

  return space;
}

AppEngine::AppEngine(XrInstance instance, XrSystemId systemId,
                     XrSession session, XrSpace appSpace)
    : m_instance(instance), m_systemId(systemId), m_session(session),
      m_appSpace(appSpace) {
  init_gles_scene();
  m_stageSpace = oxr_create_ref_space(m_session, XR_REFERENCE_SPACE_TYPE_STAGE);
}

AppEngine::~AppEngine() {}

void AppEngine::RenderLayer(XrTime dpy_time, uint32_t i,
                            const XrCompositionLayerProjectionView &layerView,
                            uint32_t fbo_id) {
  XrSpaceLocation stageLoc{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(m_stageSpace, m_appSpace, dpy_time, &stageLoc);

  render_gles_scene(layerView, fbo_id, stageLoc.pose, i);
}
