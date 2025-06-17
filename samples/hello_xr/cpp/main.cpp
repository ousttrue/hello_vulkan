#include "openxr_program/VulkanDebugMessageThunk.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/options.h"
#include "xr_loop.h"
#include <common/logger.h>
#include <thread>
#include <vko/vko.h>
#include <xro/xro.h>

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

  xro::Instance xr_instance;
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  xr_instance.systemInfo.formFactor = options.Parsed.FormFactor;
  XRO_CHECK(xr_instance.create(nullptr));
  //   options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  //   if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
  //     ShowHelp();
  //   }

  auto [instance, physicalDevice, device] =
      xr_instance.createVulkan(debugMessageThunk);

  // XrSession
  auto session = OpenXrSession ::create(
      options, xr_instance.instance, xr_instance.systemId, instance,
      physicalDevice, physicalDevice.graphicsFamilyIndex, device);

  xr_loop(
      [pQuit = &quitKeyPressed](bool isSessionRunning) {
        if (*pQuit) {
          return false;
        }
        return true;
      },
      options, session, physicalDevice, physicalDevice.graphicsFamilyIndex,
      device);

  return 0;
}
