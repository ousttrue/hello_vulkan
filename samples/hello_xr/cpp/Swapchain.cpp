#if defined(USE_MIRROR_WINDOW)
// Swapchain
struct Swapchain {
  VkFormat format{VK_FORMAT_B8G8R8A8_SRGB};
  VkSurfaceKHR surface{VK_NULL_HANDLE};
  VkSwapchainKHR swapchain{VK_NULL_HANDLE};
  VkFence readyFence{VK_NULL_HANDLE};
  VkFence presentFence{VK_NULL_HANDLE};
  static const uint32_t maxImages = 4;
  uint32_t swapchainCount = 0;
  uint32_t renderImageIdx = 0;
  VkImage image[maxImages]{VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE,
                           VK_NULL_HANDLE};

  Swapchain() {}
  ~Swapchain() { Release(); }

  void Create(VkInstance instance, VkPhysicalDevice physDevice, VkDevice device,
              uint32_t queueFamilyIndex);
  void Prepare(VkCommandBuffer buf);
  void Wait();
  void Acquire(VkSemaphore readySemaphore = VK_NULL_HANDLE);
  void Present(VkQueue queue, VkSemaphore drawComplete = VK_NULL_HANDLE);
  void Release() {
    if (m_vkDevice) {
      // Flush any pending Present() calls which are using the fence
      Wait();
      if (swapchain)
        vkDestroySwapchainKHR(m_vkDevice, swapchain, nullptr);
      if (readyFence)
        vkDestroyFence(m_vkDevice, readyFence, nullptr);
    }

    if (m_vkInstance && surface)
      vkDestroySurfaceKHR(m_vkInstance, surface, nullptr);

    readyFence = VK_NULL_HANDLE;
    presentFence = VK_NULL_HANDLE;
    swapchain = VK_NULL_HANDLE;
    surface = VK_NULL_HANDLE;
    for (uint32_t i = 0; i < swapchainCount; ++i) {
      image[i] = VK_NULL_HANDLE;
    }
    swapchainCount = 0;

#if defined(VK_USE_PLATFORM_WIN32_KHR)
    if (hWnd) {
      DestroyWindow(hWnd);
      hWnd = nullptr;
      UnregisterClassW(L"hello_xr", hInst);
    }
    if (hUser32Dll != NULL) {
      ::FreeLibrary(hUser32Dll);
      hUser32Dll = NULL;
    }
#endif

    m_vkDevice = nullptr;
  }
  void Recreate() {
    Release();
    Create(m_vkInstance, m_vkPhysicalDevice, m_vkDevice, m_queueFamilyIndex);
  }

private:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  HINSTANCE hInst{NULL};
  HWND hWnd{NULL};
  HINSTANCE hUser32Dll{NULL};
#endif
  const VkExtent2D size{640, 480};
  VkInstance m_vkInstance{VK_NULL_HANDLE};
  VkPhysicalDevice m_vkPhysicalDevice{VK_NULL_HANDLE};
  VkDevice m_vkDevice{VK_NULL_HANDLE};
  uint32_t m_queueFamilyIndex = 0;
};

void Swapchain::Create(VkInstance instance, VkPhysicalDevice physDevice,
                       VkDevice device, uint32_t queueFamilyIndex) {
  m_vkInstance = instance;
  m_vkPhysicalDevice = physDevice;
  m_vkDevice = device;
  m_queueFamilyIndex = queueFamilyIndex;

// Create a WSI surface for the window:
#if defined(VK_USE_PLATFORM_WIN32_KHR)
  hInst = GetModuleHandle(NULL);

  WNDCLASSW wc{};
  wc.style = CS_CLASSDC;
  wc.lpfnWndProc = DefWindowProcW;
  wc.cbWndExtra = sizeof(this);
  wc.hInstance = hInst;
  wc.lpszClassName = L"hello_xr";
  RegisterClassW(&wc);

// adjust the window size and show at InitDevice time
#if defined(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2)
  typedef DPI_AWARENESS_CONTEXT(WINAPI * PFN_SetThreadDpiAwarenessContext)(
      DPI_AWARENESS_CONTEXT);
  hUser32Dll = ::LoadLibraryA("user32.dll");
  if (PFN_SetThreadDpiAwarenessContext SetThreadDpiAwarenessContextFn =
          reinterpret_cast<PFN_SetThreadDpiAwarenessContext>(
              ::GetProcAddress(hUser32Dll, "SetThreadDpiAwarenessContext"))) {
    // Make sure we're 1:1 for HMD pixels
    SetThreadDpiAwarenessContextFn(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
  }
#endif
  RECT rect{0, 0, (LONG)size.width, (LONG)size.height};
  AdjustWindowRect(&rect, WS_OVERLAPPEDWINDOW, false);
  hWnd = CreateWindowW(wc.lpszClassName, L"hello_xr (Vulkan)",
                       WS_OVERLAPPEDWINDOW | WS_VISIBLE, CW_USEDEFAULT,
                       CW_USEDEFAULT, rect.right - rect.left,
                       rect.bottom - rect.top, 0, 0, hInst, 0);
  assert(hWnd != NULL);

  SetWindowLongPtr(hWnd, 0, LONG_PTR(this));

  VkWin32SurfaceCreateInfoKHR surfCreateInfo{
      VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR};
  surfCreateInfo.flags = 0;
  surfCreateInfo.hinstance = hInst;
  surfCreateInfo.hwnd = hWnd;
  if (vkCreateWin32SurfaceKHR(m_vkInstance, &surfCreateInfo, nullptr,
                              &surface) != VK_SUCCESS) {
    throw std::runtime("vkCreateWin32SurfaceKHR");
  }
#else
#error CreateSurface not supported on this OS
#endif // defined(VK_USE_PLATFORM_WIN32_KHR)

  VkSurfaceCapabilitiesKHR surfCaps;
  CHECK_VKCMD(vkGetPhysicalDeviceSurfaceCapabilitiesKHR(m_vkPhysicalDevice,
                                                        surface, &surfCaps));
  CHECK(surfCaps.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT);

  uint32_t surfFmtCount = 0;
  CHECK_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(m_vkPhysicalDevice, surface,
                                                   &surfFmtCount, nullptr));
  std::vector<VkSurfaceFormatKHR> surfFmts(surfFmtCount);
  CHECK_VKCMD(vkGetPhysicalDeviceSurfaceFormatsKHR(
      m_vkPhysicalDevice, surface, &surfFmtCount, &surfFmts[0]));
  uint32_t foundFmt;
  for (foundFmt = 0; foundFmt < surfFmtCount; ++foundFmt) {
    if (surfFmts[foundFmt].format == format)
      break;
  }

  CHECK(foundFmt < surfFmtCount);

  uint32_t presentModeCount = 0;
  CHECK_VKCMD(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_vkPhysicalDevice, surface, &presentModeCount, nullptr));
  std::vector<VkPresentModeKHR> presentModes(presentModeCount);
  CHECK_VKCMD(vkGetPhysicalDeviceSurfacePresentModesKHR(
      m_vkPhysicalDevice, surface, &presentModeCount, &presentModes[0]));

  // Do not use VSYNC for the mirror window, but Nvidia doesn't support
  // IMMEDIATE so fall back to MAILBOX
  VkPresentModeKHR presentMode = VK_PRESENT_MODE_IMMEDIATE_KHR;
  for (uint32_t i = 0; i < presentModeCount; ++i) {
    if ((presentModes[i] == VK_PRESENT_MODE_IMMEDIATE_KHR) ||
        (presentModes[i] == VK_PRESENT_MODE_MAILBOX_KHR)) {
      presentMode = presentModes[i];
      break;
    }
  }

  VkBool32 presentable = false;
  CHECK_VKCMD(vkGetPhysicalDeviceSurfaceSupportKHR(
      m_vkPhysicalDevice, m_queueFamilyIndex, surface, &presentable));
  CHECK(presentable);

  VkSwapchainCreateInfoKHR swapchainInfo{
      VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
  swapchainInfo.flags = 0;
  swapchainInfo.surface = surface;
  swapchainInfo.minImageCount = surfCaps.minImageCount;
  swapchainInfo.imageFormat = format;
  swapchainInfo.imageColorSpace = surfFmts[foundFmt].colorSpace;
  swapchainInfo.imageExtent = size;
  swapchainInfo.imageArrayLayers = 1;
  swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
  swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
  swapchainInfo.queueFamilyIndexCount = 0;
  swapchainInfo.pQueueFamilyIndices = nullptr;
  swapchainInfo.preTransform = VK_SURFACE_TRANSFORM_IDENTITY_BIT_KHR;
  swapchainInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
  swapchainInfo.presentMode = presentMode;
  swapchainInfo.clipped = true;
  swapchainInfo.oldSwapchain = VK_NULL_HANDLE;
  CHECK_VKCMD(
      vkCreateSwapchainKHR(m_vkDevice, &swapchainInfo, nullptr, &swapchain));

  // Fence to throttle host on Acquire
  VkFenceCreateInfo fenceInfo{VK_STRUCTURE_TYPE_FENCE_CREATE_INFO};
  CHECK_VKCMD(vkCreateFence(m_vkDevice, &fenceInfo, nullptr, &readyFence));

  swapchainCount = 0;
  CHECK_VKCMD(
      vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, nullptr));
  assert(swapchainCount < maxImages);
  CHECK_VKCMD(
      vkGetSwapchainImagesKHR(m_vkDevice, swapchain, &swapchainCount, image));
  if (swapchainCount > maxImages) {
    Log::Write(Log::Level::Info, "Reducing swapchain length from " +
                                     std::to_string(swapchainCount) + " to " +
                                     std::to_string(maxImages));
    swapchainCount = maxImages;
  }

  Log::Write(Log::Level::Info,
             "Swapchain length " + std::to_string(swapchainCount));
}

void Swapchain::Prepare(VkCommandBuffer buf) {
  // Convert swapchain images to VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL
  for (uint32_t i = 0; i < swapchainCount; ++i) {
    VkImageMemoryBarrier imgBarrier{VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER};
    imgBarrier.srcAccessMask = 0; // XXX was VK_ACCESS_TRANSFER_READ_BIT wrong?
    imgBarrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    imgBarrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    imgBarrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    imgBarrier.image = image[i];
    imgBarrier.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdPipelineBarrier(buf, VK_PIPELINE_STAGE_TRANSFER_BIT,
                         VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0,
                         nullptr, 1, &imgBarrier);
  }
}

void Swapchain::Wait() {
  if (presentFence) {
    // Wait for the fence...
    CHECK_VKCMD(
        vkWaitForFences(m_vkDevice, 1, &presentFence, VK_TRUE, UINT64_MAX));
    // ...then reset the fence for future Acquire calls
    CHECK_VKCMD(vkResetFences(m_vkDevice, 1, &presentFence));
    presentFence = VK_NULL_HANDLE;
  }
}

void Swapchain::Acquire(VkSemaphore readySemaphore) {
  // If we're not using a semaphore to rate-limit the GPU, rate limit the host
  // with a fence instead
  if (readySemaphore == VK_NULL_HANDLE) {
    Wait();
    presentFence = readyFence;
  }

  CHECK_VKCMD(vkAcquireNextImageKHR(m_vkDevice, swapchain, UINT64_MAX,
                                    readySemaphore, presentFence,
                                    &renderImageIdx));
}

void Swapchain::Present(VkQueue queue, VkSemaphore drawComplete) {
  VkPresentInfoKHR presentInfo{VK_STRUCTURE_TYPE_PRESENT_INFO_KHR};
  if (drawComplete) {
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &drawComplete;
  }
  presentInfo.swapchainCount = 1;
  presentInfo.pSwapchains = &swapchain;
  presentInfo.pImageIndices = &renderImageIdx;
  auto res = vkQueuePresentKHR(queue, &presentInfo);
  if (res == VK_ERROR_OUT_OF_DATE_KHR) {
    Recreate();
    return;
  }
  CHECK_VKRESULT(res, "vkQueuePresentKHR");
}
#endif // defined(USE_MIRROR_WINDOW)


