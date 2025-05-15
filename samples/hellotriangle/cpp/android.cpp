/* Copyright (c) 2016-2017, ARM Limited and Contributors
 *
 * SPDX-License-Identifier: MIT
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without restriction,
 * including without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to permit
 * persons to whom the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE
 * OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include "android.hpp"
#include <cpu-features.h>

using namespace std;

namespace MaliSDK {

Platform::Status AndroidPlatform::getWindowStatus() { return STATUS_RUNNING; }

VkSurfaceKHR AndroidPlatform::createSurface() {
  VkSurfaceKHR surface;

  VkAndroidSurfaceCreateInfoKHR info = {
      VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR};
  info.window = pNativeWindow;

  VK_CHECK(vkCreateAndroidSurfaceKHR(instance, &info, nullptr, &surface));
  return surface;
}

AssetManager &MaliSDK::OS::getAssetManager() {
  static AssetManager manager;
  return manager;
}

unsigned MaliSDK::OS::getNumberOfCpuThreads() {
  unsigned count = android_getCpuCount();
  LOGI("Detected %u CPUs.\n", count);
  return count;
}

Platform::SwapchainDimensions AndroidPlatform::getPreferredSwapchain() {
  SwapchainDimensions chain = {1280, 720, VK_FORMAT_B8G8R8A8_UNORM};
  return chain;
}

Result AndroidPlatform::createWindow(const SwapchainDimensions &swapchain) {
  return initVulkan(swapchain, {"VK_KHR_surface", "VK_KHR_android_surface"},
                    {"VK_KHR_swapchain"});
}

void AndroidPlatform::onResume(const SwapchainDimensions &swapchain) {
  vkDeviceWaitIdle(device);
  initSwapchain(swapchain);
}

void AndroidPlatform::onPause() { destroySwapchain(); }

void AndroidPlatform::terminate() { WSIPlatform::terminate(); }
} // namespace MaliSDK
