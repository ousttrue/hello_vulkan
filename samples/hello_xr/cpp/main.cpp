#include "openxr_program/VulkanDebugMessageThunk.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/options.h"
#include <common/logger.h>
#include <thread>

#include <vko/vko.h>
#include <xro/xro.h>

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

  xro::Instance xr_instance;
  xr_instance.extensions.push_back(XR_KHR_VULKAN_ENABLE2_EXTENSION_NAME);
  xr_instance.systemInfo.formFactor = options.Parsed.FormFactor;
  XRO_CHECK(xr_instance.create(nullptr));
  //   options.SetEnvironmentBlendMode(program->GetPreferredBlendMode());
  //   if (!options.UpdateOptionsFromCommandLine(argc, argv)) {
  //     ShowHelp();
  //   }

  // Create VkDevice by OpenXR.
  vko::Instance instance;
  instance.appInfo.pApplicationName = "hello_xr";
  instance.appInfo.pEngineName = "hello_xr";
  instance.debugUtilsMessengerCreateInfo = {
      .sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT,
      .messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
                         VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT,
      .messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                     VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT,
      .pfnUserCallback = debugMessageThunk,
      // .pUserData = program.get(),
  };
  vko::Device device;

#ifdef NDEBUG
#else
  instance.debugUtilsMessengerCreateInfo.messageSeverity =
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT |
      VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT;
  instance.pushFirstSupportedValidationLayer({
      "VK_LAYER_KHRONOS_validation",
      "VK_LAYER_LUNARG_standard_validation",
  });
  instance.pushFirstSupportedInstanceExtension({
      VK_EXT_DEBUG_UTILS_EXTENSION_NAME,
      VK_EXT_DEBUG_REPORT_EXTENSION_NAME,
  });
#endif

  auto vulkan = xr_instance.createVulkan(
      instance.validationLayers, instance.instanceExtensions,
      device.deviceExtensions, &instance.debugUtilsMessengerCreateInfo);
  instance.reset(vulkan.Instance);
  device.reset(vulkan.Device);

  // XrSession
  OpenXrSession session(options, xr_instance.instance, xr_instance.systemId,
                        vulkan.Instance, vulkan.PhysicalDevice,
                        vulkan.QueueFamilyIndex, vulkan.Device);

  xr_loop(
      [pQuit = &quitKeyPressed, &session]() {
        while (true) {
          if (*pQuit) {
            return false;
          }

          bool exitRenderLoop = false;
          bool requestRestart = false;
          session.PollEvents(&exitRenderLoop, &requestRestart);
          if (exitRenderLoop) {
            return false;
          }

          if (!session.IsSessionRunning()) {
            // Throttle loop since xrWaitFrame won't be called.
            std::this_thread::sleep_for(std::chrono::milliseconds(250));
            continue;
          }

          session.m_input.PollActions(session.m_session);

          return true;
        }
      },
      options, session, vulkan.PhysicalDevice, vulkan.QueueFamilyIndex,
      vulkan.Device);

  return 0;
}
