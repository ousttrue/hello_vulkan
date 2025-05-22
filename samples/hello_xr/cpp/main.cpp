#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "options.h"
#include "platformplugin_win32.h"
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

  //
  // Create platform-specific implementation.
  //
  auto platformPlugin = CreatePlatformPlugin_Win32(options);

  // Create graphics API implementation.
  auto graphicsPlugin = std::make_shared<VulkanGraphicsPlugin>();

  // Initialize the OpenXR program.
  auto program =
      std::make_shared<OpenXrProgram>(options, platformPlugin, graphicsPlugin);

  program->CreateInstance();
  program->InitializeSystem();

  options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
    ShowHelp();
  }

  program->InitializeDevice(getVulkanLayers(), getVulkanInstanceExtensions(),
                            getVulkanDeviceExtensions());
  program->InitializeSession();
  program->CreateSwapchains();

  while (!quitKeyPressed) {
    bool exitRenderLoop = false;
    bool requestRestart = false;
    program->PollEvents(&exitRenderLoop, &requestRestart);
    if (exitRenderLoop) {
      break;
    }

    if (program->IsSessionRunning()) {
      program->PollActions();
      program->RenderFrame(options.GetBackgroundClearColor());
    } else {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }

  return 0;
}
