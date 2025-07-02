#include "../main_loop.h"
#include "vuloxr/vk.h"
#include "vuloxr/vk/command.h"

#include <assert.h>
#include <chrono>
#include <cmath>

static VkClearColorValue getColorForTime(std::chrono::nanoseconds nano) {
  auto sec = std::chrono::duration_cast<std::chrono::duration<double>>(nano);
  const auto SPD = 3.0f;
  float v = (std::sin(sec.count() * SPD) + 1.0f) * 0.5;
  return {v, 0.0, 0.0, 0.0};
}

void main_loop(const vuloxr::gui::WindowLoopOnce &windowLoopOnce,
               const vuloxr::vk::Instance &instance,
               vuloxr::vk::Swapchain &swapchain,
               const vuloxr::vk::PhysicalDevice &physicalDevice,
               const vuloxr::vk::Device &device, void *) {

  vuloxr::vk::AcquireSemaphorePool semaphorePool(device);
  vuloxr::vk::CommandBufferPool pool(device, physicalDevice.graphicsFamilyIndex,
                                     swapchain.images.size());

  while (auto state = windowLoopOnce()) {
    // acquire
    auto acquireSemaphore = semaphorePool.getOrCreate();
    auto [res, acquired] = swapchain.acquireNextImage(acquireSemaphore);
    vuloxr::vk::CheckVkResult(res);

    // update
    auto color =
        getColorForTime(std::chrono::nanoseconds(acquired.presentTimeNano));

    // command
    auto cmd = &pool[acquired.imageIndex];
    semaphorePool.resetFenceAndMakePairSemaphore(cmd->submitFence,
                                                 acquireSemaphore);

    vuloxr::vk::CommandScope(physicalDevice.graphicsFamilyIndex,
                             cmd->commandBuffer)
        .clearImage(color, acquired.image);

    vuloxr::vk::CheckVkResult(cmd->submit(acquireSemaphore));

    // present
    vuloxr::vk::CheckVkResult(
        swapchain.present(acquired.imageIndex, cmd->submitSemaphore));
  }

  vkDeviceWaitIdle(device);
}
