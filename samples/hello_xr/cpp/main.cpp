#include "openxr_program/openxr_program.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/options.h"
#include <common/logger.h>
#include <thread>

#include <vkr/vulkan_layers.h>

#include "xr_loop.h"

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

  xr_loop(
      [pQuit = &quitKeyPressed, session]() {
        while (true) {
          if (*pQuit) {
            return false;
          }

          bool exitRenderLoop = false;
          bool requestRestart = false;
          session->PollEvents(&exitRenderLoop, &requestRestart);
          if (exitRenderLoop) {
            return false;
          }

          if (!session->IsSessionRunning()) {
            // Throttle loop since xrWaitFrame won't be called.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
          }

          session->PollActions();

          return true;
        }
      },
      options, session, vulkan.PhysicalDevice, vulkan.QueueFamilyIndex,
      vulkan.Device);

  return 0;
}
