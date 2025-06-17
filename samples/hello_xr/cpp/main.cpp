#include "GetXrReferenceSpaceCreateInfo.h"
#include "VulkanDebugMessageThunk.h"
#include "logger.h"
#include "options.h"
#include "xr_loop.h"
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

  {
    auto [instance, physicalDevice, device] =
        xr_instance.createVulkan(debugMessageThunk);

    {
      // XrSession
      xro::Session session(xr_instance.instance, xr_instance.systemId, instance,
                           physicalDevice, physicalDevice.graphicsFamilyIndex,
                           device);
      XrReferenceSpaceCreateInfo referenceSpaceCreateInfo =
          GetXrReferenceSpaceCreateInfo(options.AppSpace);
      XrSpace appSpace;
      XRO_CHECK(xrCreateReferenceSpace(session, &referenceSpaceCreateInfo,
                                       &appSpace));
      auto clearColor = options.GetBackgroundClearColor();

      xr_loop(
          [pQuit = &quitKeyPressed](bool isSessionRunning) {
            if (*pQuit) {
              return false;
            }
            return true;
          },
          xr_instance.instance, xr_instance.systemId, session, appSpace,
          options.Parsed.EnvironmentBlendMode,
          {
              .float32 = {clearColor.x, clearColor.y, clearColor.z,
                          clearColor.w},
          },
          options.Parsed.ViewConfigType, session.selectColorSwapchainFormat(),
          physicalDevice, physicalDevice.graphicsFamilyIndex, device);

      // session
    }
    // vulkan
  }

  return 0;
}
