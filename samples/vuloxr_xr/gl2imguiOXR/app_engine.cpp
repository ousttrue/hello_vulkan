// #include "util_egl.h"
// #include "util_oxr.h"
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

static std::string oxr_get_system_name(XrInstance instance, XrSystemId sysid) {
  XrSystemProperties prop = {XR_TYPE_SYSTEM_PROPERTIES};
  xrGetSystemProperties(instance, sysid, &prop);

  std::string sys_name = prop.systemName;
  return sys_name;
}

static std::string oxr_get_runtime_name(XrInstance instance) {
  XrInstanceProperties prop = {XR_TYPE_INSTANCE_PROPERTIES};
  xrGetInstanceProperties(instance, &prop);

  char strbuf[128];
  snprintf(strbuf, 127, "%s (%u.%u.%u)", prop.runtimeName,
           XR_VERSION_MAJOR(prop.runtimeVersion),
           XR_VERSION_MINOR(prop.runtimeVersion),
           XR_VERSION_PATCH(prop.runtimeVersion));
  std::string runtime_name = strbuf;
  return runtime_name;
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
  m_viewSpace = oxr_create_ref_space(m_session, XR_REFERENCE_SPACE_TYPE_VIEW);

  // m_viewSurface = oxr_create_viewsurface(m_instance, m_systemId, m_session);

  m_runtime_name = oxr_get_runtime_name(m_instance);
  m_system_name = oxr_get_system_name(m_instance, m_systemId);
}

AppEngine::~AppEngine() {}

void AppEngine::RenderLayer(XrTime dpy_time, XrTime elapsed_us, uint32_t i,
                            const XrCompositionLayerProjectionView &layerView,
                            const XrView &view, uint32_t fbo_id) {
  /* Acquire View Location */
  // uint32_t viewCount = (uint32_t)m_viewSurface.size();

  // std::vector<XrView> views(viewCount, {XR_TYPE_VIEW});
  // oxr_locate_views(m_session, dpy_time, m_appSpace, &viewCount,
  // views.data());

  // layerViews.resize(viewCount);

  /* Acquire Stage Location, View Location (rerative to the View Location) */
  XrSpaceLocation stageLoc{XR_TYPE_SPACE_LOCATION};
  XrSpaceLocation viewLoc{XR_TYPE_SPACE_LOCATION};
  xrLocateSpace(m_stageSpace, m_appSpace, dpy_time, &stageLoc);
  xrLocateSpace(m_viewSpace, m_appSpace, dpy_time, &viewLoc);

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

    scene_data_t sceneData;
    sceneData.runtime_name = m_runtime_name;
    sceneData.system_name = m_system_name;
    sceneData.elapsed_us = elapsed_us;
    sceneData.viewID = i;
    sceneData.view = &view;
    render_gles_scene(layerView, fbo_id, viewLoc.pose, stageLoc.pose,
                      sceneData);

    // oxr_release_viewsurface(m_viewSurface[i]);
  }
  // layer = {XR_TYPE_COMPOSITION_LAYER_PROJECTION};
  // layer.space = m_appSpace;
  // layer.viewCount = (uint32_t)layerViews.size();
  // layer.views = layerViews.data();
  //
  // return true;
}
