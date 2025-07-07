#include "Renderer.h"
#include "OXR.h"

// #include "Input/SkeletonRenderer.h"
// #include "Input/ControllerRenderer.h"
// #include "Input/TinyUI.h"
#include "Input/AxisRenderer.h"
#include "Input/HandRenderer.h"
// #include "Render/SimpleBeamRenderer.h"
// #include "Render/GeometryRenderer.h"
#include <assert.h>

struct Impl {
  XrInstance Instance;

  // OVRFW::ControllerRenderer controllerRenderL_;
  // OVRFW::ControllerRenderer controllerRenderR_;

  /// Hands - FB mesh rendering extensions
  PFN_xrGetHandMeshFB xrGetHandMeshFB_ = nullptr;
  OVRFW::HandRenderer handRendererL_;
  OVRFW::HandRenderer handRendererR_;

  // OVRFW::TinyUI ui_;
  // OVRFW::SimpleBeamRenderer beamRenderer_;
  // std::vector<OVRFW::ovrBeamRenderer::handle_t> beams_;
  OVRFW::ovrAxisRenderer axisRendererL_;
  OVRFW::ovrAxisRenderer axisRendererR_;

  // OVRFW::VRMenuObject* renderMeshLButton_ = nullptr;
  // OVRFW::VRMenuObject* renderMeshRButton_ = nullptr;
  // OVRFW::VRMenuObject* renderJointsLButton_ = nullptr;
  // OVRFW::VRMenuObject* renderJointsRButton_ = nullptr;
  // OVRFW::VRMenuObject* renderCapsulesLButton_ = nullptr;
  // OVRFW::VRMenuObject* renderCapsulesRButton_ = nullptr;

  // std::vector<OVRFW::GeometryRenderer> handJointRenderersL_;
  // std::vector<OVRFW::GeometryRenderer> handJointRenderersR_;
  // std::vector<OVRFW::GeometryRenderer> handCapsuleRenderersL_;
  // std::vector<OVRFW::GeometryRenderer> handCapsuleRenderersR_;

  Impl(XrInstance instance) : Instance(instance) {}

  void appInit() {
    // if (false == ui_.Init(context, GetFileSys())) {
    //     ALOG("TinyUI::Init FAILED.");
    //     return false;
    // }

    /// Build UI
    CreateSampleDescriptionPanel();

    // ui_.AddLabel("OpenXR Hands + FB extensions Sample", {0.1f, 1.25f, -2.0f},
    // {1300.0f, 100.0f})
    //     ->SetSurfaceColor(0, {0.0f, 0.0f, 1.0f, 1.0f});
    // ui_.AddLabel("Left Hand", {-1.0f, 2.5f, -2.0f}, {200.0f, 100.0f})
    //     ->SetSurfaceColor(0, {0.0f, 0.0f, 1.0f, 1.0f});

    // renderMeshLButton_ =
    //     ui_.AddButton("Mesh: On", {-1.0f, 2.25f, -2.0f}, {200.0f, 100.0f},
    //     [this]() {
    //         renderMeshL_ = !renderMeshL_;
    //         if (renderMeshL_) {
    //             renderMeshLButton_->SetText("Mesh: On");
    //         } else {
    //             renderMeshLButton_->SetText("Mesh: Off");
    //         }
    //     });
    // renderJointsLButton_ =
    //     ui_.AddButton("Joints: Off", {-1.0f, 2.0f, -2.0f}, {200.0f, 100.0f},
    //     [this]() {
    //         renderJointsL_ = !renderJointsL_;
    //         if (renderJointsL_) {
    //             renderJointsLButton_->SetText("Joints: On");
    //         } else {
    //             renderJointsLButton_->SetText("Joints: Off");
    //         }
    //     });
    // renderCapsulesLButton_ =
    //     ui_.AddButton("Capsules: Off", {-1.0f, 1.75f, -2.0f}, {200.0f,
    //     100.0f}, [this]() {
    //         renderCapsulesL_ = !renderCapsulesL_;
    //         if (renderCapsulesL_) {
    //             renderCapsulesLButton_->SetText("Capsules: On");
    //         } else {
    //             renderCapsulesLButton_->SetText("Capsules: Off");
    //         }
    //     });
    //
    // ui_.AddLabel("Right Hand", {0.0f, 2.5f, -2.0f}, {200.0f, 100.0f})
    //     ->SetSurfaceColor(0, {0.0f, 0.0f, 1.0f, 1.0f});
    //
    // renderMeshRButton_ =
    //     ui_.AddButton("Mesh: On", {0.0f, 2.25f, -2.0f}, {200.0f, 100.0f},
    //     [this]() {
    //         renderMeshR_ = !renderMeshR_;
    //         if (renderMeshR_) {
    //             renderMeshRButton_->SetText("Mesh: On");
    //         } else {
    //             renderMeshRButton_->SetText("Mesh: Off");
    //         }
    //     });
    // renderJointsRButton_ =
    //     ui_.AddButton("Joints: Off", {0.0f, 2.0f, -2.0f}, {200.0f, 100.0f},
    //     [this]() {
    //         renderJointsR_ = !renderJointsR_;
    //         if (renderJointsR_) {
    //             renderJointsRButton_->SetText("Joints: On");
    //         } else {
    //             renderJointsRButton_->SetText("Joints: Off");
    //         }
    //     });
    // renderCapsulesRButton_ =
    //     ui_.AddButton("Capsules: Off", {0.0f, 1.75f, -2.0f}, {200.0f,
    //     100.0f}, [this]() {
    //         renderCapsulesR_ = !renderCapsulesR_;
    //         if (renderCapsulesR_) {
    //             renderCapsulesRButton_->SetText("Capsules: On");
    //         } else {
    //             renderCapsulesRButton_->SetText("Capsules: Off");
    //         }
    //     });

    /// Hook up extensions for hand rendering
    OXR(xrGetInstanceProcAddr(Instance, "xrGetHandMeshFB",
                              (PFN_xrVoidFunction *)(&xrGetHandMeshFB_)));
  }

  void CreateSampleDescriptionPanel() {
    // // Panel to provide sample description to the user for context
    // auto descriptionLabel = ui_.AddLabel(
    //     static_cast<std::string>(kSampleExplanation), {1.55f, 2.355f, -1.8f},
    //     {750.0f, 400.0f});
    //
    // // Align and size the description text for readability
    // OVRFW::VRMenuFontParms fontParams{};
    // fontParams.Scale = 0.5f;
    // fontParams.AlignHoriz = OVRFW::HORIZONTAL_LEFT;
    // descriptionLabel->SetFontParms(fontParams);
    // descriptionLabel->SetTextLocalPosition({-0.65f, 0, 0});
    //
    // // Tilt the description billboard 45 degrees towards the user
    // descriptionLabel->SetLocalRotation(
    //     OVR::Quat<float>::FromRotationVector({0, OVR::DegreeToRad(-30.0f),
    //     0}));
    // descriptionLabel->SetSurfaceColor(0, {0.0f, 0.0f, 1.0f, 1.0f});
  }

  void appShutdown() {
    // ui_.Shutdown();

    xrGetHandMeshFB_ = nullptr;
  }

  void sessionInit() {
    /// Init session bound objects
    // if (false == controllerRenderL_.Init(true)) {
    //     ALOG("AppInit::Init L controller renderer FAILED.");
    //     return false;
    // }
    // if (false == controllerRenderR_.Init(false)) {
    //     ALOG("AppInit::Init R controller renderer FAILED.");
    //     return false;
    // }
    // beamRenderer_.Init(GetFileSys(), nullptr, OVR::Vector4f(1.0f), 1.0f);

    /// Hand rendering
    axisRendererL_.Init();
    axisRendererR_.Init();
  }

  void sessionInitHand(
      int handIndex, XrHandTrackerEXT handTracker,
      XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]) {

    assert(xrGetHandMeshFB_);

    /// Alias everything for initialization
    const bool isLeft = (handIndex == 0);
    auto &handRenderer = isLeft ? handRendererL_ : handRendererR_;
    // auto& handJointRenderers = isLeft ? handJointRenderersL_ :
    // handJointRenderersR_;
    // auto& handCapsuleRenderers =
    //     isLeft ? handCapsuleRenderersL_ : handCapsuleRenderersR_;

    /// two-call pattern for mesh data
    /// call 1 - figure out sizes

    /// mesh
    XrHandTrackingMeshFB mesh{XR_TYPE_HAND_TRACKING_MESH_FB};
    mesh.next = nullptr;
    /// mesh - skeleton
    mesh.jointCapacityInput = 0;
    mesh.jointCountOutput = 0;
    mesh.jointBindPoses = nullptr;
    mesh.jointRadii = nullptr;
    mesh.jointParents = nullptr;
    /// mesh - vertex
    mesh.vertexCapacityInput = 0;
    mesh.vertexCountOutput = 0;
    mesh.vertexPositions = nullptr;
    mesh.vertexNormals = nullptr;
    mesh.vertexUVs = nullptr;
    mesh.vertexBlendIndices = nullptr;
    mesh.vertexBlendWeights = nullptr;
    /// mesh - index
    mesh.indexCapacityInput = 0;
    mesh.indexCountOutput = 0;
    mesh.indices = nullptr;
    /// get mesh sizes
    OXR(xrGetHandMeshFB_(handTracker, &mesh));

    /// mesh storage - update sizes
    mesh.jointCapacityInput = mesh.jointCountOutput;
    mesh.vertexCapacityInput = mesh.vertexCountOutput;
    mesh.indexCapacityInput = mesh.indexCountOutput;
    /// skeleton
    std::vector<XrPosef> jointBindLocations;
    std::vector<XrHandJointEXT> parentData;
    std::vector<float> jointRadii;
    jointBindLocations.resize(mesh.jointCountOutput);
    parentData.resize(mesh.jointCountOutput);
    jointRadii.resize(mesh.jointCountOutput);
    mesh.jointBindPoses = jointBindLocations.data();
    mesh.jointParents = parentData.data();
    mesh.jointRadii = jointRadii.data();
    /// vertex
    std::vector<XrVector3f> vertexPositions;
    std::vector<XrVector3f> vertexNormals;
    std::vector<XrVector2f> vertexUVs;
    std::vector<XrVector4sFB> vertexBlendIndices;
    std::vector<XrVector4f> vertexBlendWeights;
    vertexPositions.resize(mesh.vertexCountOutput);
    vertexNormals.resize(mesh.vertexCountOutput);
    vertexUVs.resize(mesh.vertexCountOutput);
    vertexBlendIndices.resize(mesh.vertexCountOutput);
    vertexBlendWeights.resize(mesh.vertexCountOutput);
    mesh.vertexPositions = vertexPositions.data();
    mesh.vertexNormals = vertexNormals.data();
    mesh.vertexUVs = vertexUVs.data();
    mesh.vertexBlendIndices = vertexBlendIndices.data();
    mesh.vertexBlendWeights = vertexBlendWeights.data();
    /// index
    std::vector<int16_t> indices;
    indices.resize(mesh.indexCountOutput);
    mesh.indices = indices.data();

    /// call 2 - fill in the data
    /// chain capsules
    XrHandTrackingCapsulesStateFB capsuleState{
        XR_TYPE_HAND_TRACKING_CAPSULES_STATE_FB};
    capsuleState.next = nullptr;
    mesh.next = &capsuleState;

    /// get mesh data
    OXR(xrGetHandMeshFB_(handTracker, &mesh));
    /// init renderer
    handRenderer.Init(&mesh, true);
    /// Render jointRadius for all left hand joints
    {
      //     handJointRenderers.resize(XR_HAND_JOINT_COUNT_EXT);
      //     for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
      //         const OVR::Posef pose =
      //         FromXrPosef(jointLocations[i].pose);
      //         OVRFW::GeometryRenderer& gr = handJointRenderers[i];
      //         gr.Init(OVRFW::BuildTesselatedCapsuleDescriptor(
      //             mesh.jointRadii[i], 0.0f, 7, 7));
      //         gr.SetPose(pose);
      //         gr.DiffuseColor = jointColor_;
      //     }
    }
    /// One time init for capsules
    {
      //     handCapsuleRenderers.resize(XR_FB_HAND_TRACKING_CAPSULE_COUNT);
      //     for (int i = 0; i < XR_FB_HAND_TRACKING_CAPSULE_COUNT; ++i) {
      //         const OVR::Vector3f p0 =
      //             FromXrVector3f(capsuleState.capsules[i].points[0]);
      //         const OVR::Vector3f p1 =
      //             FromXrVector3f(capsuleState.capsules[i].points[1]);
      //         const OVR::Vector3f d = (p1 - p0);
      //         const float h = d.Length();
      //         const float r = capsuleState.capsules[i].radius;
      //         const OVR::Quatf look = OVR::Quatf::LookRotation(d, {0, 1,
      //         0}); OVRFW::GeometryRenderer& gr = handCapsuleRenderers[i];
      //         gr.Init(OVRFW::BuildTesselatedCapsuleDescriptor(r, h, 7,
      //         7)); gr.SetPose(OVR::Posef(look, p0)); gr.DiffuseColor =
      //         capsuleColor_;
      //     }
    }
    /// Print hierarchy
    {
      for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
        const OVR::Posef pose = FromXrPosef(jointLocations[i].pose);
        ALOG(" { {%.6f, %.6f, %.6f},  {%.6f, %.6f, %.6f, %.6f} } // "
             "joint = %d, parent = %d",
             pose.Translation.x, pose.Translation.y, pose.Translation.z,
             pose.Rotation.x, pose.Rotation.y, pose.Rotation.z, pose.Rotation.w,
             i, (int)parentData[i]);
      }
    }
  }

  void sessionEnd() {
    // controllerRenderL_.Shutdown();
    // controllerRenderR_.Shutdown();
    // beamRenderer_.Shutdown();
    axisRendererL_.Shutdown();
    axisRendererR_.Shutdown();
    handRendererL_.Shutdown();
    handRendererR_.Shutdown();
  }

  void
  updateHand(int handIndex,
             XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]) {
    const bool isLeft = (handIndex == 0);
    auto &handRenderer = isLeft ? handRendererL_ : handRendererR_;
    handRenderer.Update(&jointLocations[0]);
  }

  void render(const OVRFW::ovrApplFrameIn &in, OVRFW::ovrRendererOutput &out,
              bool handTrackedL, bool handTrackedR) {
    /// Render UI
    // ui_.Render(in, out);

    /// Render controllers when hands are not tracked
    if (in.LeftRemoteTracked && !handTrackedL) {
      // controllerrenderl_.render(out.surfaces);
    }
    if (in.RightRemoteTracked && !handTrackedR) {
      // controllerRenderR_.Render(out.Surfaces);
    }

    /// Render hand axes
    if (handTrackedL) {
      axisRendererL_.Render(OVR::Matrix4f(), in, out);

      //     if (renderJointsL_) {
      //         for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
      //             OVRFW::GeometryRenderer& gr = handJointRenderersL_[i];
      //             gr.Render(out.Surfaces);
      //         }
      //     }
      //
      //     if (renderCapsulesL_) {
      //         for (int i = 0; i < XR_FB_HAND_TRACKING_CAPSULE_COUNT; ++i) {
      //             OVRFW::GeometryRenderer& gr = handCapsuleRenderersL_[i];
      //             gr.Render(out.Surfaces);
      //         }
      //     }
      //
      // if (renderMeshL_)
      {
        handRendererL_.Render(out.Surfaces);
      }
    }
    // if (handTrackedR_) {
    //     axisRendererR_.Render(OVR::Matrix4f(), in, out);
    //
    //     if (renderJointsR_) {
    //         for (int i = 0; i < XR_HAND_JOINT_COUNT_EXT; ++i) {
    //             OVRFW::GeometryRenderer& gr = handJointRenderersR_[i];
    //             gr.Render(out.Surfaces);
    //         }
    //     }
    //
    //     if (renderCapsulesR_) {
    //         for (int i = 0; i < XR_FB_HAND_TRACKING_CAPSULE_COUNT; ++i) {
    //             OVRFW::GeometryRenderer& gr = handCapsuleRenderersR_[i];
    //             gr.Render(out.Surfaces);
    //         }
    //     }
    //
    //     if (renderMeshR_) {
    //         handRendererR_.Render(out.Surfaces);
    //     }
    // }

    /// Render beams
    // beamRenderer_.Render(in, out);
  }
};

Renderer::Renderer() {}
Renderer::~Renderer() { delete _impl; }
void Renderer::appInit(XrInstance instance) {
  _impl = new Impl(instance);
  _impl->appInit();
}
void Renderer::appShutdown() { _impl->appShutdown(); }
void Renderer::sessionInit() { _impl->sessionInit(); }
void Renderer::sessionInitHand(
    int handIndex, XrHandTrackerEXT handTracker,
    XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]) {
  _impl->sessionInitHand(handIndex, handTracker, jointLocations);
}

void Renderer::sessionEnd() { _impl->sessionEnd(); }
void Renderer::updateHand(
    int handIndex,
    XrHandJointLocationEXT jointLocations[XR_HAND_JOINT_COUNT_EXT]) {
  _impl->updateHand(handIndex, jointLocations);
}

void Renderer::render(const OVRFW::ovrApplFrameIn &in,
                      OVRFW::ovrRendererOutput &out, bool handTrackedL,
                      bool handTrackedR) {
  _impl->render(in, out, handTrackedL, handTrackedR);
}
