#include "CubeScene.h"
#include "xr_linear.h"
// #include <vuloxr/xr/session.h>

inline XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
  XrPosef t{
      .orientation = {0, 0, 0, 1.0f},
      .position = {0, 0, 0},
  };
  t.orientation.x = 0.f;
  t.orientation.y = std::sin(radians * 0.5f);
  t.orientation.z = 0.f;
  t.orientation.w = std::cos(radians * 0.5f);
  t.position = translation;
  return t;
}

CubeScene::CubeScene(XrSession session) {
  // ViewFront
  addSpace(session, XR_REFERENCE_SPACE_TYPE_VIEW,
           {.orientation = {0, 0, 0, 1.0f}, .position = {0, 0, -2.0f}});
  // Local
  addSpace(session, XR_REFERENCE_SPACE_TYPE_LOCAL);
  // Stage
  addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE);
  // StageLeft
  addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
           RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f}));
  // StageRight
  addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
           RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f}));
  // StageLeftRotated
  addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
           RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f}));
  // StageRightRotated
  addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
           RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f}));
}

CubeScene::~CubeScene() {
  for (XrSpace visualizedSpace : this->spaces) {
    xrDestroySpace(visualizedSpace);
  }
}

void CubeScene::addSpace(XrSession session, XrReferenceSpaceType spaceType,
                         const XrPosef &pose) {
  XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
      .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
      .referenceSpaceType = spaceType,
      .poseInReferenceSpace = pose,
  };
  XrSpace space;
  XrResult res =
      xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);
  if (XR_SUCCEEDED(res)) {
    this->spaces.push_back(space);
  } else {
    vuloxr::Logger::Error("Failed to create reference space");
  }
}

void CubeScene::beginFrame() { this->cubes.clear(); }

std::span<const Mat4> CubeScene::endFrame(XrPosef hmdPose, XrFovf fov) {
  this->matrices.resize(this->cubes.size());
  for (int i = 0; i < this->cubes.size(); ++i) {
    auto &cube = this->cubes[i];

    // Compute the view-projection transform. Note all matrixes
    // (including OpenXR's) are column-major, right-handed.
    XrMatrix4x4f proj;
    XrMatrix4x4f_CreateProjectionFov(&proj, GRAPHICS_VULKAN, fov, 0.05f,
                                     100.0f);
    XrMatrix4x4f toView;
    XrMatrix4x4f_CreateFromRigidTransform(&toView, &hmdPose);
    XrMatrix4x4f view;
    XrMatrix4x4f_InvertRigidBody(&view, &toView);
    XrMatrix4x4f vp;
    XrMatrix4x4f_Multiply(&vp, &proj, &view);

    // Compute the model-view-projection transform and push it.
    XrMatrix4x4f model;
    XrMatrix4x4f_CreateTranslationRotationScale(
        &model, &cube.pose.position, &cube.pose.orientation, &cube.scale);

    auto &m = this->matrices[i];
    auto mvp = (XrMatrix4x4f *)&m;
    XrMatrix4x4f_Multiply(mvp, &vp, &model);
  }
  return this->matrices;
}
