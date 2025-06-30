#include "CubeScene.h"
#include "xr_linear.h"

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

void CubeScene::clear() { this->cubes.clear(); }

void CubeScene::calcMatrix(const DirectX::XMFLOAT4X4 &viewProjection,
                           std::vector<DirectX::XMFLOAT4X4> &outMatrices) {
  outMatrices.resize(this->cubes.size());

  auto vp =
      DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4 *)&viewProjection);

  for (int i = 0; i < this->cubes.size(); ++i) {
    auto &cube = this->cubes[i];

    // Compute the model-view-projection transform and push it.
    // XrMatrix4x4f model;
    auto model = DirectX::XMMatrixTransformation(
        // S
        DirectX::XMVectorZero(), DirectX::XMQuaternionIdentity(),
        DirectX::XMLoadFloat3((const DirectX::XMFLOAT3 *)&cube.scale),
        // R
        DirectX::XMVectorZero(),
        DirectX::XMLoadFloat4(
            (const DirectX::XMFLOAT4 *)&cube.pose.orientation),
        // T
        DirectX::XMLoadFloat3((const DirectX::XMFLOAT3 *)&cube.pose.position));
    // XrMatrix4x4f_CreateTranslationRotationScale(
    //     &model, &cube.pose.position, &cube.pose.orientation, &cube.scale);

    // auto mvp = (XrMatrix4x4f *)&m;
    // XrMatrix4x4f_Multiply(mvp, &vp, &model);
    auto mvp = DirectX::XMMatrixMultiply(model, vp);

    DirectX::XMStoreFloat4x4(&outMatrices[i], mvp);
  }
}
