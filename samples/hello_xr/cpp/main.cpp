#include "CubeScene.h"
#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "openxr_session.h"
#include "options.h"
#include "vulkan_layers.h"
#include <thread>

void ShowHelp() {
  Log::Write(
      Log::Level::Info,
      "HelloXr [--formfactor|-ff <Form factor>] "
      "[--viewconfig|-vc <View config>] "
      "[--blendmode|-bm <Blend mode>] [--space|-s <Space>] [--verbose|-v]");
  Log::Write(Log::Level::Info, "Graphics APIs:            D3D11, D3D12, "
                               "OpenGLES, OpenGL, Vulkan2, Vulkan, Metal");
  Log::Write(Log::Level::Info, "Form factors:             Hmd, Handheld");
  Log::Write(Log::Level::Info, "View configurations:      Mono, Stereo");
  Log::Write(Log::Level::Info,
             "Environment blend modes:  Opaque, Additive, AlphaBlend");
  Log::Write(Log::Level::Info, "Spaces:                   View, Local, Stage");
}

int main(int argc, char *argv[]) {
  // Parse command-line arguments into Options.
  Options options;
  if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
    ShowHelp();
    return 1;
  }

  // Spawn a thread to wait for a keypress
  static bool quitKeyPressed = false;
  auto exitPollingThread = std::thread{[] {
    Log::Write(Log::Level::Info, "Press any key to shutdown...");
    (void)getchar();
    quitKeyPressed = true;
  }};
  exitPollingThread.detach();

  // Initialize the OpenXR program.
  auto program = OpenXrProgram::Create(options, {}, nullptr);
  if (!program) {
    Log::Write(Log::Level::Error, "No system. QuestLink not ready ?");
    return 1;
  }

  options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
    ShowHelp();
  }

  // Create VkDevice by OpenXR.
  auto vulkan = program->InitializeVulkan(getVulkanLayers(),
                                          getVulkanInstanceExtensions(),
                                          getVulkanDeviceExtensions());
  auto session = program->InitializeSession(vulkan);
  session->CreateSwapchains(vulkan);

  while (!quitKeyPressed) {
    bool exitRenderLoop = false;
    bool requestRestart = false;
    session->PollEvents(&exitRenderLoop, &requestRestart);
    if (exitRenderLoop) {
      break;
    }

    if (session->IsSessionRunning()) {
      session->PollActions();

      // program->RenderFrame(vulkan, options.GetBackgroundClearColor());
      std::vector<XrCompositionLayerBaseHeader *> layers;
      XrCompositionLayerProjection layer;
      std::vector<XrCompositionLayerProjectionView> projectionLayerViews;

      auto frameState = session->BeginFrame();
      if (frameState.shouldRender == XR_TRUE) {
        uint32_t viewCountOutput;
        if (session->LocateView(session->m_session, session->m_appSpace,
                                frameState.predictedDisplayTime,
                                options.Parsed.ViewConfigType,
                                &viewCountOutput)) {

          CubeScene scene;
          scene.addSpaceCubes(session->m_appSpace,
                              frameState.predictedDisplayTime,
                              session->m_visualizedSpaces);
          scene.addHandCubes(session->m_appSpace,
                             frameState.predictedDisplayTime, session->m_input);

          for (uint32_t i = 0; i < viewCountOutput; ++i) {
            auto info = session->AcquireSwapchainForView(i);
            projectionLayerViews.push_back(info.CompositionLayer);

            {
              auto cmd = vulkan->BeginCommand();

              vulkan->RenderView(
                  cmd, info.Swapchain, info.ImageIndex,
                  options.GetBackgroundClearColor(),
                  scene.CalcCubeMatrices(info.calcViewProjection()));

              vulkan->EndCommand(cmd);
            }

            session->EndSwapchain(info.CompositionLayer.subImage.swapchain);
          }

          layer = {
              .type = XR_TYPE_COMPOSITION_LAYER_PROJECTION,
              .layerFlags = static_cast<XrCompositionLayerFlags>(
                  options.Parsed.EnvironmentBlendMode ==
                          XR_ENVIRONMENT_BLEND_MODE_ALPHA_BLEND
                      ? XR_COMPOSITION_LAYER_BLEND_TEXTURE_SOURCE_ALPHA_BIT |
                            XR_COMPOSITION_LAYER_UNPREMULTIPLIED_ALPHA_BIT
                      : 0),
              .space = session->m_appSpace,
              .viewCount = static_cast<uint32_t>(projectionLayerViews.size()),
              .views = projectionLayerViews.data(),
          };

          layers.push_back(
              reinterpret_cast<XrCompositionLayerBaseHeader *>(&layer));
        }
      }
      session->EndFrame(frameState.predictedDisplayPeriod, layers);

    } else {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }

  return 0;
}
