#include "CubeScene.h"
#include "Swapchain.h"
#include "VulkanGraphicsPlugin.h"
#include "VulkanRenderer.h"
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

  // XrSession
  auto session = program->InitializeSession(vulkan);

  // XrSwapchain
  auto config = session->GetSwapchainConfiguration();
  auto swapchainFormat = vulkan->SelectColorSwapchainFormat(config.Formats);

  // Create a swapchain for each view.
  std::vector<std::shared_ptr<Swapchain>> swapchains;
  std::vector<std::shared_ptr<VulkanRenderer>> renderers;
  for (uint32_t i = 0; i < config.Views.size(); i++) {
    auto swapchain = Swapchain::Create(session->m_session, i, config.Views[i],
                                       swapchainFormat);
    swapchains.push_back(swapchain);

    auto renderer = VulkanRenderer::Create(
        vulkan->m_vkDevice, vulkan->m_memAllocator,
        {swapchain->m_swapchainCreateInfo.width,
         swapchain->m_swapchainCreateInfo.height},
        static_cast<VkFormat>(swapchain->m_swapchainCreateInfo.format),
        static_cast<VkSampleCountFlagBits>(
            swapchain->m_swapchainCreateInfo.sampleCount),
        vulkan->m_shaderProgram);
    renderers.push_back(renderer);
  }

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
        if (session->LocateView(
                session->m_appSpace, frameState.predictedDisplayTime,
                options.Parsed.ViewConfigType, &viewCountOutput)) {
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
            auto swapchain = swapchains[i];
            auto info = swapchain->AcquireSwapchainForView(session->m_views[i]);
            composition.pushView(info.CompositionLayer);

            {
              // render vulkan
              VkCommandBuffer cmd = vulkan->BeginCommand();
              renderers[i]->RenderView(
                  cmd, info.Image, options.GetBackgroundClearColor(),
                  scene.CalcCubeMatrices(info.calcViewProjection()));
              vulkan->EndCommand(cmd);
            }

            swapchain->EndSwapchain();
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
