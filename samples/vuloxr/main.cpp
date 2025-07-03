#include "main_loop.h"
#include <vuloxr/gui/glfw.h>

auto NAME = "vuloxr";

int main(int argc, char **argv) {
  vuloxr::Logger::Verbose("Verbose");
  vuloxr::Logger::Info("Info");
  vuloxr::Logger::Warn("Warn");
  vuloxr::Logger::Error("Error");
#if NDEBUG
  bool useDebug = false;
  vuloxr::Logger::Info("## NDEBUG ##");
#else
  bool useDebug = true;
  vuloxr::Logger::Info("## DEBUG ##");
#endif

  vuloxr::gui::Glfw glfw;
  auto window = glfw.createWindow(1024, 768, NAME);

  try {
    vuloxr::vk::Vulkan vulkan = glfw.createVulkan(useDebug);

    main_loop(glfw.makeWindowLoopOnce(), vulkan.instance, vulkan.swapchain,
              vulkan.physicalDevice, vulkan.device, window);
  } catch (const std::exception &ex) {
    vuloxr::Logger::Error("%s", ex.what());
  } catch (...) {
    vuloxr::Logger::Error("Unknown Error");
  }

  return 0;
}
