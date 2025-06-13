#include "xr_loop.h"

#include "openxr/openxr.h"
#include "openxr_program/VulkanDebugMessageThunk.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/options.h"
#include "vko/vko.h"
#include <common/logger.h>
#include <thread>
#include <vko/vko.h>

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
  vko::Instance instance;
  instance.debug_messenger_create_info.pfnUserCallback = debugMessageThunk;

  vko::Device device;

#ifndef NDEBUG
  instance.debug_messenger_create_info.messageSeverity |=
      (VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT
       // | VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT
      );
  vko::Logger::Info("[debug build]");
  const char *layerNames[] = {
      "VK_LAYER_KHRONOS_validation",
      "VK_LAYER_LUNARG_standard_validation",
  };
  for (auto name : layerNames) {
    if (vko::layerIsSupported(name)) {
      instance.validationLayers.push_back(name);
      break;
    }
  }
  const char *instanceExtensionNames[] = {
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
  };
  for (auto name : instanceExtensionNames) {
    if (vko::instanceExtensionIsSupported(name)) {
      instance.instanceExtensions.push_back(name);
      break;
    }
  }
#endif

  // Parse command-line arguments into Options.
  Options options;
  if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
    ShowHelp();
    return 1;
  }

  // Spawn a thread to wait for a keypress
  static bool quitKeyPressed = false;
  auto exitPollingThread = std::thread{[] {
    Log::Write(Log::Level::Info, "Press [enter key] to shutdown...");
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
  auto vulkan = program->InitializeVulkan(
      instance.validationLayers, instance.instanceExtensions,
      device.deviceExtensions, &instance.debug_messenger_create_info);
  instance.setInstance(vulkan.Instance);
  device.setDevice(vulkan.Device);

  // XrSession
  auto session =
      program->InitializeSession(vulkan.Instance, vulkan.PhysicalDevice,
                                 vulkan.Device, vulkan.QueueFamilyIndex);

  xr_loop(
      [session, &options]() -> std::tuple<bool, XrFrameState> {
        while (true) {
          if (quitKeyPressed) {
            return {false, {}};
          }

          bool exitRenderLoop = false;
          bool requestRestart = false;
          session->PollEvents(&exitRenderLoop, &requestRestart);
          if (exitRenderLoop) {
            return {false, {}};
          }

          if (!session->IsSessionRunning()) {
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
          }

          break;
        }

        session->PollActions();

        auto frameState = session->BeginFrame();
        return {true, frameState};
      },
      session, options, vulkan.PhysicalDevice, vulkan.QueueFamilyIndex,
      vulkan.Device);

  return 0;
}
