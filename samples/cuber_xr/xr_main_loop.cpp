#include "xr_main_loop.h"
#include "xr_linear.h"
#include <cuber/gl3/GlCubeRenderer.h>
#include <cuber/gl3/GlLineRenderer.h>
#include <grapho/gl3/texture.h>
#include <grapho/imgui/printfbuffer.h>
#include <thread>
#include <vuloxr/xr/session.h>

#include <vuloxr/gl.h>

struct rgba {
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

struct ViewSwapchain {
  std::vector<std::shared_ptr<vuloxr::gl::RenderTarget>> backbuffers;

  ViewSwapchain(std::shared_ptr<GraphicsSwapchain> &swapchain) {
    for (auto &image : swapchain->swapchainImages) {
      this->backbuffers.push_back(std::make_shared<vuloxr::gl::RenderTarget>(
          image.image, swapchain->swapchainCreateInfo.width,
          swapchain->swapchainCreateInfo.height));
    }
  }
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  std::span<const int64_t> formats,
                  //
                  const Graphics &graphics,
                  //
                  const XrColor4f &clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  auto format = Graphics::selectColorSwapchainFormat(formats);
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewSwapchain>> views;
  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);

    views.push_back(std::make_shared<ViewSwapchain>(swapchain));
  }

  // AppEngine engine(instance, systemId, session, appSpace);
  // cuber
  cuber::gl3::GlCubeRenderer cubeRenderer;
  cuber::gl3::GlLineRenderer lineRenderer;

  std::vector<cuber::LineVertex> lines;
  cuber::PushGrid(lines);

  // texture
  static rgba pixels[4] = {
      {255, 0, 0, 255},
      {0, 255, 0, 255},
      {0, 0, 255, 255},
      {255, 255, 255, 255},
  };
  auto texture = grapho::gl3::Texture::Create({
      .Width = 2,
      .Height = 2,
      .Format = grapho::PixelFormat::u8_RGBA,
      .Pixels = &pixels[0].r,
  });
  texture->SamplingPoint();

  std::vector<cuber::Instance> instances;
  instances.push_back({});
  auto t = DirectX::XMMatrixTranslation(0, 1, -1);
  auto s = DirectX::XMMatrixScaling(1.6f, 0.9f, 0.1f);
  DirectX::XMStoreFloat4x4(&instances.back().Matrix, s * t);

  const auto TextureBind = 0;
  const auto PalleteIndex = 9;

  // Pallete
  instances.back().PositiveFaceFlag = {PalleteIndex, PalleteIndex, PalleteIndex,
                                       0};
  instances.back().NegativeFaceFlag = {PalleteIndex, PalleteIndex, PalleteIndex,
                                       0};
  cubeRenderer.Pallete.Colors[PalleteIndex] = {1, 1, 1, 1};
  cubeRenderer.Pallete.Textures[PalleteIndex] = {TextureBind, TextureBind,
                                                 TextureBind, TextureBind};
  cubeRenderer.UploadPallete();

  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
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

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace, blendMode);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          // engine.RenderLayer(frameState.predictedDisplayTime, i,
          //                    projectionLayer,
          //                    views[i]->backbuffers[index]->fbo.id);
          glBindFramebuffer(GL_FRAMEBUFFER,
                            views[i]->backbuffers[index]->fbo.id);
          int view_x = projectionLayer.subImage.imageRect.offset.x;
          int view_y = projectionLayer.subImage.imageRect.offset.y;
          int view_w = projectionLayer.subImage.imageRect.extent.width;
          int view_h = projectionLayer.subImage.imageRect.extent.height;
          glViewport(view_x, view_y, view_w, view_h);

          glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
          glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

          /* ------------------------------------------- *
           *  Matrix Setup
           *    (matPV)  = (proj) x (view)
           *    (matPVM) = (proj) x (view) x (model)
           * ------------------------------------------- */
          XrMatrix4x4f matP, matV, matC, matM, matPV, matPVM;

          /* Projection Matrix */
          XrMatrix4x4f_CreateProjectionFov(&matP, GRAPHICS_OPENGL_ES,
                                           projectionLayer.fov, 0.05f, 100.0f);

          /* View Matrix (inverse of Camera matrix) */
          XrVector3f scale = {1.0f, 1.0f, 1.0f};
          const auto &vewPose = projectionLayer.pose;
          XrMatrix4x4f_CreateTranslationRotationScale(
              &matC, &vewPose.position, &vewPose.orientation, &scale);
          XrMatrix4x4f_InvertRigidBody(&matV, &matC);

          texture->Activate(TextureBind);
          cubeRenderer.Render(matP.m, matV.m, instances.data(),
                              instances.size());
          lineRenderer.Render(matP.m, matV.m, lines);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }
}
