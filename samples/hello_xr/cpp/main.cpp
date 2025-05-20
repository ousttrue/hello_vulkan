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

bool UpdateOptionsFromCommandLine(Options &options, int argc, char *argv[]) {
  int i = 1; // Index 0 is the program name and is skipped.

  auto getNextArg = [argc, argv, &i] {
    if (i >= argc) {
      throw std::invalid_argument("Argument parameter missing");
    }

    return std::string(argv[i++]);
  };

  while (i < argc) {
    const std::string arg = getNextArg();
    if (EqualsIgnoreCase(arg, "--formfactor") || EqualsIgnoreCase(arg, "-ff")) {
      options.FormFactor = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--viewconfig") ||
               EqualsIgnoreCase(arg, "-vc")) {
      options.ViewConfiguration = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--blendmode") ||
               EqualsIgnoreCase(arg, "-bm")) {
      options.EnvironmentBlendMode = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--space") ||
               EqualsIgnoreCase(arg, "-s")) {
      options.AppSpace = getNextArg();
    } else if (EqualsIgnoreCase(arg, "--verbose") ||
               EqualsIgnoreCase(arg, "-v")) {
      Log::SetLevel(Log::Level::Verbose);
    } else if (EqualsIgnoreCase(arg, "--help") || EqualsIgnoreCase(arg, "-h")) {
      ShowHelp();
      return false;
    } else {
      throw std::invalid_argument(Fmt("Unknown argument: %s", arg.c_str()));
    }
  }

  try {
    options.ParseStrings();
  } catch (std::invalid_argument &ia) {
    Log::Write(Log::Level::Error, ia.what());
    ShowHelp();
    return false;
  }
  return true;
}

int main(int argc, char *argv[]) {
  // Parse command-line arguments into Options.
  std::shared_ptr<Options> options = std::make_shared<Options>();
  if (!UpdateOptionsFromCommandLine(*options, argc, argv)) {
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

    options->SetEnvironmentBlendMode(program->GetPreferredBlendMode());
    UpdateOptionsFromCommandLine(*options, argc, argv);
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
