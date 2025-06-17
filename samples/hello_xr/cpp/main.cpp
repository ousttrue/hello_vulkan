#include "GetXrReferenceSpaceCreateInfo.h"
#include "options.h"
#include "xr_loop.h"
#include <thread>
#include <vko/vko.h>
#include <xro/xro.h>

void ShowHelp() {
  xro::Logger::Info(
      "HelloXr [--formfactor|-ff <Form factor>] "
      "[--viewconfig|-vc <View config>] "
      "[--blendmode|-bm <Blend mode>] [--space|-s <Space>] [--verbose|-v]");
  xro::Logger::Info("Graphics APIs:            D3D11, D3D12, "
                    "OpenGLES, OpenGL, Vulkan2, Vulkan, Metal");
  xro::Logger::Info("Form factors:             Hmd, Handheld");
  xro::Logger::Info("View configurations:      Mono, Stereo");
  xro::Logger::Info("Environment blend modes:  Opaque, Additive, AlphaBlend");
  xro::Logger::Info("Spaces:                   View, Local, Stage");
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
    xro::Logger::Info("Press [enter key] to shutdown...");
    (void)getchar();
    quitKeyPressed = true;
  }};
  exitPollingThread.detach();

  xro::Instance xr_instance;
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  xr_instance.systemInfo.formFactor = options.Parsed.FormFactor;
  if(xr_instance.create(nullptr)!=XR_SUCCESS){
    xro::Logger::Info("no xro::Instance. no Oculus link ? shutdown...");
    return 1;
  }
  //   options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  //   if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
  //     ShowHelp();
  //   }

  {
    auto [instance, physicalDevice, device] = xr_instance.createVulkan(nullptr);

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
          options.Parsed.EnvironmentBlendMode, clearColor,
          options.Parsed.ViewConfigType, session.selectColorSwapchainFormat(),
          physicalDevice, physicalDevice.graphicsFamilyIndex, device);

      // session
    }
    // vulkan
  }

  return 0;
}
