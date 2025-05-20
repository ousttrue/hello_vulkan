#include "VulkanGraphicsPlugin.h"
#include "logger.h"
#include "openxr_program.h"
#include "options.h"
#include "platformplugin_win32.h"
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

  bool requestRestart = false;
  do {
    // Create platform-specific implementation.
    std::shared_ptr<IPlatformPlugin> platformPlugin =
        CreatePlatformPlugin_Win32(options);

    // Create graphics API implementation.
    std::shared_ptr<VulkanGraphicsPlugin> graphicsPlugin =
        std::make_shared<VulkanGraphicsPlugin>(options, platformPlugin);

    // Initialize the OpenXR program.
    auto program = std::make_shared<OpenXrProgram>(options, platformPlugin,
                                                   graphicsPlugin);

    program->CreateInstance();
    program->InitializeSystem();

    options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
    if(!options.UpdateOptionsFromCommandLine(argc, argv)){
      ShowHelp();
    }
    platformPlugin->UpdateOptions(options);
    graphicsPlugin->UpdateOptions(options);

    program->InitializeDevice();
    program->InitializeSession();
    program->CreateSwapchains();

    while (!quitKeyPressed) {
      bool exitRenderLoop = false;
      program->PollEvents(&exitRenderLoop, &requestRestart);
      if (exitRenderLoop) {
        break;
      }

      if (program->IsSessionRunning()) {
        program->PollActions();
        program->RenderFrame();
      } else {
        // Throttle loop since xrWaitFrame won't be called.
        std::this_thread::sleep_for(std::chrono::milliseconds(250));
      }
    }

  } while (!quitKeyPressed && requestRestart);

  return 0;
}
