#include "CubeScene.h"
#include "SwapchainImageContext.h"
#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "openxr_session.h"
#include "options.h"
#include "vulkan_layers.h"
#include <thread>
#include <vulkan/vulkan_core.h>

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

  auto config = session->GetSwapchainConfiguration();

  session->CreateSwapchains(vulkan, config);

  while (!quitKeyPressed) {
    bool exitRenderLoop = false;
    bool requestRestart = false;
    session->PollEvents(&exitRenderLoop, &requestRestart);
    if (exitRenderLoop) {
      break;
    }

    if (session->IsSessionRunning()) {
      session->PollActions();

      LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                   session->m_appSpace);

      auto frameState = session->BeginFrame();
      if (frameState.shouldRender == XR_TRUE) {
        uint32_t viewCountOutput;
        if (session->LocateView(session->m_session, session->m_appSpace,
                                frameState.predictedDisplayTime,
                                options.Parsed.ViewConfigType,
                                &viewCountOutput)) {
          // XrCompositionLayerProjection

          // update scene
          CubeScene scene;
          scene.addSpaceCubes(session->m_appSpace,
                              frameState.predictedDisplayTime,
                              session->m_visualizedSpaces);
          scene.addHandCubes(session->m_appSpace,
                             frameState.predictedDisplayTime, session->m_input);

          for (uint32_t i = 0; i < viewCountOutput; ++i) {
            // XrCompositionLayerProjectionView(left / right)
            auto info = session->AcquireSwapchainForView(i);
            composition.pushView(info.CompositionLayer);

            {
              VkCommandBuffer cmd = vulkan->BeginCommand();
              info.Swapchain->RenderView(
                  cmd, info.ImageIndex, options.GetBackgroundClearColor(),
                  scene.CalcCubeMatrices(info.calcViewProjection()));
              vulkan->EndCommand(cmd);
            }

            session->EndSwapchain(info.CompositionLayer.subImage.swapchain);
          }
        }
      }

      // std::vector<XrCompositionLayerBaseHeader *>
      auto &layers = composition.commitLayers();
      session->EndFrame(frameState.predictedDisplayPeriod, layers);

    } else {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }

  return 0;
}
