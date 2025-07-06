#include "../xr_main_loop.h"
#include "../xr_linear.h"

#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

#include <DirectXMath.h>

#include <thread>

struct Cube {
  XrPosef pose;
  XrVector3f scale;
};

static XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
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

struct CubeScene : vuloxr::NonCopyable {
  std::vector<XrSpace> spaces;
  std::vector<Cube> cubes;
  std::vector<DirectX::XMFLOAT4X4> matrices;

  CubeScene(XrSession session) {
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

  ~CubeScene() {
    for (XrSpace visualizedSpace : this->spaces) {
      xrDestroySpace(visualizedSpace);
    }
  }

  void addSpace(XrSession session, XrReferenceSpaceType spaceType,
                const XrPosef &pose = {.orientation = {0, 0, 0, 1.0f},
                                       .position = {0, 0, 0}}) {
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

  void clear() { this->cubes.clear(); }

  std::span<const DirectX::XMFLOAT4X4>
  calcMatrix(const DirectX::XMFLOAT4X4 &viewProjection) {
    this->matrices.resize(this->cubes.size());

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
          DirectX::XMLoadFloat3(
              (const DirectX::XMFLOAT3 *)&cube.pose.position));
      // XrMatrix4x4f_CreateTranslationRotationScale(
      //     &model, &cube.pose.position, &cube.pose.orientation, &cube.scale);

      // auto mvp = (XrMatrix4x4f *)&m;
      // XrMatrix4x4f_Multiply(mvp, &vp, &model);
      auto mvp = DirectX::XMMatrixMultiply(model, vp);

      DirectX::XMStoreFloat4x4(&this->matrices[i], mvp);
    }

    return this->matrices;
  }
};

char VS[] = {
#embed "shader.vert"
    , 0};

char FS[] = {
#embed "shader.frag"
    , 0};

struct Mat4 {
  float m[16];
};

struct Vec3 {
  float x, y, z;
};

struct Vertex {
  Vec3 Position;
  Vec3 Color;
};

static VertexAttributeLayout layouts[]{
    {.components = 3, .offset = offsetof(Vertex, Position)},
    {.components = 3, .offset = offsetof(Vertex, Color)},
};

constexpr Vec3 Red{1, 0, 0};
constexpr Vec3 DarkRed{0.25f, 0, 0};
constexpr Vec3 Green{0, 1, 0};
constexpr Vec3 DarkGreen{0, 0.25f, 0};
constexpr Vec3 Blue{0, 0, 1};
constexpr Vec3 DarkBlue{0, 0, 0.25f};

// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
constexpr Vec3 LBB{-0.5f, -0.5f, -0.5f};
constexpr Vec3 LBF{-0.5f, -0.5f, 0.5f};
constexpr Vec3 LTB{-0.5f, 0.5f, -0.5f};
constexpr Vec3 LTF{-0.5f, 0.5f, 0.5f};
constexpr Vec3 RBB{0.5f, -0.5f, -0.5f};
constexpr Vec3 RBF{0.5f, -0.5f, 0.5f};
constexpr Vec3 RTB{0.5f, 0.5f, -0.5f};
constexpr Vec3 RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                               \
  {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

constexpr Vertex vertices[] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)   // -X
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)       // +X
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen) // -Y
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)     // +Y
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)  // -Z
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)      // +Z
};

// Winding order is clockwise. Each side uses a different color.
constexpr uint16_t indices[] = {
    0,  1,  2,  3,  4,  5,  // -X
    6,  7,  8,  9,  10, 11, // +X
    12, 13, 14, 15, 16, 17, // -Y
    18, 19, 20, 21, 22, 23, // +Y
    24, 25, 26, 27, 28, 29, // -Z
    30, 31, 32, 33, 34, 35, // +Z
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  std::span<const int64_t> formats, const Graphics &graphics,
                  const XrColor4f &clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);

  auto format = Graphics::selectColorSwapchainFormat(formats);

  // Create resources for each view.
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);

    auto r = std::make_shared<ViewRenderer>(&graphics, swapchain);

    r->initScene(VS, FS, layouts,
                 {
                     .data = vertices,
                     .stride = static_cast<uint32_t>(sizeof(Vertex)),
                     .drawCount = std::size(vertices),
                 },
                 {
                     .data = indices,
                     .stride = static_cast<uint32_t>(sizeof(uint16_t)),
                     .drawCount = std::size(indices),
                 });

    r->initSwapchain(swapchain->swapchainCreateInfo.width,
                     swapchain->swapchainCreateInfo.height,
                     swapchain->swapchainImages);

    renderers.push_back(r);
  }

  CubeScene scene(session);

  // mainloop
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::InputState input(instance, session);
  while (runLoop(state.m_sessionRunning)) {
    state.PollEvents();
    if (state.m_exitRenderLoop) {
      break;
    }

    if (!state.m_sessionRunning) {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }

    input.PollActions();

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace, blendMode);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {

        scene.clear();
        for (XrSpace visualizedSpace : scene.spaces) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, visualizedSpace);
          if (vuloxr::xr::locationIsValid(res, location)) {
            scene.cubes.push_back({location.pose, {0.25f, 0.25f, 0.25f}});
          }
        }
        for (auto &hand : input.hands) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, hand.space);
          auto s = hand.scale * 0.1f;
          if (vuloxr::xr::locationIsValid(res, location)) {
            scene.cubes.push_back({location.pose, {s, s, s}});
          }
        }

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          // Compute the view-projection transform. Note all matrixes (including
          // OpenXR's) are column-major, right-handed.
          XrMatrix4x4f proj;

          XrMatrix4x4f_CreateProjectionFov(&proj,
#ifdef XR_USE_GRAPHICS_API_VULKAN
                                           GRAPHICS_VULKAN,
#elif defined(XR_USE_GRAPHICS_API_OPENGL_ES)
                                           GRAPHICS_OPENGL_ES,
#elif defined(XR_USE_GRAPHICS_API_OPENGL)
                                           GRAPHICS_OPENGL,
#else
                                           static_assert(false, "no XR_USE_");
#endif

                                           projectionLayer.fov, 0.05f, 100.0f);
          XrMatrix4x4f toView;
          XrMatrix4x4f_CreateFromRigidTransform(&toView, &projectionLayer.pose);
          XrMatrix4x4f view;
          XrMatrix4x4f_InvertRigidBody(&view, &toView);
          XrMatrix4x4f vp;
          XrMatrix4x4f_Multiply(&vp, &proj, &view);

          auto matrices = scene.calcMatrix(*((DirectX::XMFLOAT4X4 *)&vp));
          renderers[i]->render(index, clearColor, matrices);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }

  // vkDeviceWaitIdle(graphics.device);
}
