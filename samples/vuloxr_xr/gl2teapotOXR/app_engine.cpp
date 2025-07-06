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

  // void *vm = m_app->activity->vm;
  // void *clazz = m_app->activity->clazz;

  // oxr_initialize_loader(vm, clazz);

  // m_instance = oxr_create_instance(vm, clazz);
  // m_systemId = oxr_get_system(m_instance);

  // egl_init_with_pbuffer_surface(3, 24, 0, 0, 16, 16);
  // oxr_confirm_gfx_requirements(m_instance, m_systemId);

  init_gles_scene();

  // m_session = oxr_create_session(m_instance, m_systemId);
  // m_appSpace = oxr_create_ref_space(m_session,
  // XR_REFERENCE_SPACE_TYPE_LOCAL);
  m_stageSpace = oxr_create_ref_space(m_session, XR_REFERENCE_SPACE_TYPE_STAGE);

  // m_viewSurface = oxr_create_viewsurface(m_instance, m_systemId, m_session);
}

AppEngine::~AppEngine() {}

void AppEngine::RenderLayer(XrTime dpy_time, XrTime elapsed_us, uint32_t i,
                            const XrCompositionLayerProjectionView &layerView,
                            uint32_t fbo_id) {
  /* Acquire View Location */
  // uint32_t viewCount = (uint32_t)m_viewSurface.size();

  // std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
  // oxr_locate_views(m_session, dpy_time, m_appSpace, &viewCount,
  // views.data());

  // layerViews.resize(viewCount);

  /* Acquire Stage Location (rerative to the View Location) */
  XrSpaceLocation stageLoc{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(m_stageSpace, m_appSpace, dpy_time, &stageLoc);

  /* Render each view */
  // for (uint32_t i = 0; i < viewCount; i++)
  {
    // XrSwapchainSubImage subImg;
    // render_target_t rtarget;

    // oxr_acquire_viewsurface(m_viewSurface[i], rtarget, subImg);

    // layerViews[i] = {XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW};
    // layerViews[i].pose = views[i].pose;
    // layerViews[i].fov = views[i].fov;
    // layerViews[i].subImage = subImg;

    render_gles_scene(layerView, fbo_id, stageLoc.pose, elapsed_us, i);

    // oxr_release_viewsurface(m_viewSurface[i]);
  }
  // layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  // layer.space = m_appSpace;
  // layer.viewCount = (uint32_t)layerViews.size();
  // layer.views = layerViews.data();
  //
  // return true;
}
