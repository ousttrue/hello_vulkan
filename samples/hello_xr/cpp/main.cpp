#include "openxr_program/CubeScene.h"
#include "openxr_program/openxr_program.h"
#include "openxr_program/openxr_session.h"
#include "openxr_program/openxr_swapchain.h"
#include "openxr_program/options.h"
#include <common/logger.h>
#include <thread>
#include <vkr/vulkan_debug_object_namer.hpp>
#include <vkr/vulkan_renderer.h>

// std::vector<const char *> getVulkanLayers() {
// #ifndef NDEBUG
//   // debug
//   if (auto layer = vko::getValidationLayerName()) {
//     // has layer
//     return {layer};
//   }
//   Log::Write(Log::Level::Warning,
//              "No validation layers found in the system, skipping");
// #endif
//
//   // no debug layer
//   return {};
// }

std::vector<const char *> getVulkanInstanceExtensions() {
  std::vector<const char *> extensions;

  uint32_t extensionCount = 0;
  if (vkEnumerateInstanceExtensionProperties(nullptr, &extensionCount,
                                             nullptr) != VK_SUCCESS) {
    throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
  }

  std::vector<VkExtensionProperties> availableExtensions(extensionCount);
  if (vkEnumerateInstanceExtensionProperties(
          nullptr, &extensionCount, availableExtensions.data()) != VK_SUCCESS) {
    throw std::runtime_error("vkEnumerateInstanceExtensionProperties");
  }
  const auto b = availableExtensions.begin();
  const auto e = availableExtensions.end();

  auto isExtSupported = [&](const char *extName) -> bool {
    auto it = std::find_if(b, e, [&](const VkExtensionProperties &properties) {
      return (0 == strcmp(extName, properties.extensionName));
    });
    return (it != e);
  };

  // Debug utils is optional and not always available
  if (isExtSupported(VK_EXT_DEBUG_UTILS_EXTENSION_NAME)) {
    extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
  }
  // TODO add back VK_EXT_debug_report code for compatibility with older
  // systems? (Android)

#if defined(USE_MIRROR_WINDOW)
  extensions.push_back("VK_KHR_surface");
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  extensions.push_back("VK_KHR_win32_surface");
#else
#error CreateSurface not supported on this OS
#endif // defined(VK_USE_PLATFORM_WIN32_KHR)
#endif // defined(USE_MIRROR_WINDOW)

  return extensions;
}

std::vector<const char *> getVulkanDeviceExtensions() {
  std::vector<const char *> deviceExtensions;
#if defined(USE_MIRROR_WINDOW)
  deviceExtensions.push_back(VK_KHR_SWAPCHAIN_EXTENSION_NAME);
#endif
  return deviceExtensions;
}

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
  std::vector<const char *> validationLayers;
  std::vector<const char *> instanceExtensions;
  std::vector<const char *> deviceExtensions = {
      VK_KHR_SWAPCHAIN_EXTENSION_NAME,
  };
#ifndef NDEBUG
  vko::Logger::Info("[debug build]");
  validationLayers.push_back(vko::getValidationLayerName());
  instanceExtensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
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
  auto vulkan = program->InitializeVulkan(validationLayers,
                                          getVulkanInstanceExtensions(),
                                          getVulkanDeviceExtensions());
  SetDebugUtilsObjectNameEXT_GetProc(vulkan.Instance);

  // XrSession
  auto session =
      program->InitializeSession(vulkan.Instance, vulkan.PhysicalDevice,
                                 vulkan.Device, vulkan.QueueFamilyIndex);

  // Create resources for each view.
  auto config = session->GetSwapchainConfiguration();
  std::vector<std::shared_ptr<OpenXrSwapchain>> swapchains;
  std::vector<std::shared_ptr<VulkanRenderer>> renderers;
  for (uint32_t i = 0; i < config.Views.size(); i++) {
    // XrSwapchain
    auto swapchain = OpenXrSwapchain::Create(session->m_session, i,
                                             config.Views[i], config.Format);
    swapchains.push_back(swapchain);

    // VkPipeline... etc
    auto renderer = std::make_shared<VulkanRenderer>(
        vulkan.PhysicalDevice, vulkan.Device, vulkan.QueueFamilyIndex,
        VkExtent2D{swapchain->m_swapchainCreateInfo.width,
                   swapchain->m_swapchainCreateInfo.height},
        static_cast<VkFormat>(swapchain->m_swapchainCreateInfo.format),
        static_cast<VkSampleCountFlagBits>(
            swapchain->m_swapchainCreateInfo.sampleCount));
    renderers.push_back(renderer);
  }

  // mainloop
  while (!quitKeyPressed) {
    bool exitRenderLoop = false;
    bool requestRestart = false;
    session->PollEvents(&exitRenderLoop, &requestRestart);
    if (exitRenderLoop) {
      break;
    }

    if (session->IsSessionRunning()) {
      session->PollActions();

      auto frameState = session->BeginFrame();
      LayerComposition composition(options.Parsed.EnvironmentBlendMode,
                                   session->m_appSpace);

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
            auto info = swapchain->AcquireSwapchain(session->m_views[i]);
            composition.pushView(info.CompositionLayer);

            {
              // render vulkan
              auto renderer = renderers[i];
              VkCommandBuffer cmd = renderer->BeginCommand();
              renderer->RenderView(
                  cmd, info.Image, options.GetBackgroundClearColor(),
                  scene.CalcCubeMatrices(info.calcViewProjection()));
              renderer->EndCommand(cmd);
            }

            swapchain->EndSwapchain();
          }
        }
      }

      // std::vector<XrCompositionLayerBaseHeader *>
      auto &layers = composition.commitLayers();
      session->EndFrame(frameState.predictedDisplayTime, layers);

    } else {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
    }
  }

  return 0;
}
