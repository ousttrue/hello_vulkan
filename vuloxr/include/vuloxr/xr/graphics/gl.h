#pragma once

#include <windows.h>

#include <GL/glew.h>
#include <GL/wglew.h>
// #define GL_EXT_color_subtable
// #include <GL/glext.h>
// #include <GL/wglext.h>
// #include <GL/gl_format.h>

// #include "../../../vuloxr.h"

#include "../../xr.h"

#include <openxr/openxr_platform.h>
#include <openxr/openxr_reflection.h>

#include <algorithm>
#include <span>

namespace vuloxr {

namespace gl {

struct OpenGL {
  static int64_t
  selectColorSwapchainFormat(std::span<const int64_t> runtimeFormats) {
    // List of supported color swapchain formats.
    constexpr int64_t SupportedColorSwapchainFormats[] = {
        GL_RGB10_A2,
        GL_RGBA16F,
        // The two below should only be used as a fallback, as they are linear
        // color formats without enough bits for color depth, thus leading to
        // banding.
        GL_RGBA8,
        GL_RGBA8_SNORM,
    };

    auto swapchainFormatIt =
        std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(),
                           std::begin(SupportedColorSwapchainFormats),
                           std::end(SupportedColorSwapchainFormats));
    if (swapchainFormatIt == runtimeFormats.end()) {
      throw std::runtime_error(
          "No runtime swapchain format supported for color swapchain");
    }

    return *swapchainFormatIt;
  }
};

} // namespace gl

namespace xr {

inline LRESULT APIENTRY WndProc(HWND hWnd, UINT message, WPARAM wParam,
                                LPARAM lParam) {
  // ksGpuWindow *window = (ksGpuWindow *)GetWindowLongPtrA(hWnd,
  // GWLP_USERDATA);

  switch (message) {
  case WM_SIZE: {
    // if (window != NULL) {
    //     window->windowWidth = (int)LOWORD(lParam);
    //     window->windowHeight = (int)HIWORD(lParam);
    // }
    return 0;
  }
  case WM_ACTIVATE: {
    // if (window != NULL) {
    //     window->windowActiveState = !HIWORD(wParam);
    // }
    return 0;
  }
  case WM_ERASEBKGND: {
    return 0;
  }
  case WM_CLOSE: {
    PostQuitMessage(0);
    return 0;
  }
  case WM_KEYDOWN: {
    // if (window != NULL) {
    //     if ((int)wParam >= 0 && (int)wParam < 256) {
    //         if ((int)wParam != KEY_SHIFT_LEFT && (int)wParam != KEY_CTRL_LEFT
    //         && (int)wParam != KEY_ALT_LEFT &&
    //             (int)wParam != KEY_CURSOR_UP && (int)wParam !=
    //             KEY_CURSOR_DOWN && (int)wParam != KEY_CURSOR_LEFT &&
    //             (int)wParam != KEY_CURSOR_RIGHT) {
    //             window->input.keyInput[(int)wParam] = true;
    //         }
    //     }
    // }
    break;
  }
  case WM_LBUTTONDOWN: {
    // window->input.mouseInput[MOUSE_LEFT] = true;
    // window->input.mouseInputX[MOUSE_LEFT] = LOWORD(lParam);
    // window->input.mouseInputY[MOUSE_LEFT] = window->windowHeight -
    // HIWORD(lParam);
    break;
  }
  case WM_RBUTTONDOWN: {
    // window->input.mouseInput[MOUSE_RIGHT] = true;
    // window->input.mouseInputX[MOUSE_RIGHT] = LOWORD(lParam);
    // window->input.mouseInputY[MOUSE_RIGHT] = window->windowHeight -
    // HIWORD(lParam);
    break;
  }
  }
  return DefWindowProcA(hWnd, message, wParam, lParam);
}

typedef enum {
  KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5,
  KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5,
  KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8,
  KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8,
  KS_GPU_SURFACE_COLOR_FORMAT_MAX
} ksGpuSurfaceColorFormat;

typedef enum {
  KS_GPU_SURFACE_DEPTH_FORMAT_NONE,
  KS_GPU_SURFACE_DEPTH_FORMAT_D16,
  KS_GPU_SURFACE_DEPTH_FORMAT_D24,
  KS_GPU_SURFACE_DEPTH_FORMAT_MAX
} ksGpuSurfaceDepthFormat;

typedef enum {
  KS_GPU_SAMPLE_COUNT_1 = 1,
  KS_GPU_SAMPLE_COUNT_2 = 2,
  KS_GPU_SAMPLE_COUNT_4 = 4,
  KS_GPU_SAMPLE_COUNT_8 = 8,
  KS_GPU_SAMPLE_COUNT_16 = 16,
  KS_GPU_SAMPLE_COUNT_32 = 32,
  KS_GPU_SAMPLE_COUNT_64 = 64,
} ksGpuSampleCount;

typedef struct {
  unsigned char redBits;
  unsigned char greenBits;
  unsigned char blueBits;
  unsigned char alphaBits;
  unsigned char colorBits;
  unsigned char depthBits;
} ksGpuSurfaceBits;

inline ksGpuSurfaceBits
ksGpuContext_BitsForSurfaceFormat(const ksGpuSurfaceColorFormat colorFormat,
                                  const ksGpuSurfaceDepthFormat depthFormat) {
  ksGpuSurfaceBits bits;
  bits.redBits =
      ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
           ? 8
           : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                  ? 8
                  : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                         ? 5
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5)
                                ? 5
                                : 8))));
  bits.greenBits =
      ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
           ? 8
           : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                  ? 8
                  : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                         ? 6
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5)
                                ? 6
                                : 8))));
  bits.blueBits =
      ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
           ? 8
           : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                  ? 8
                  : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                         ? 5
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5)
                                ? 5
                                : 8))));
  bits.alphaBits =
      ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R8G8B8A8)
           ? 8
           : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8)
                  ? 8
                  : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_R5G6B5)
                         ? 0
                         : ((colorFormat == KS_GPU_SURFACE_COLOR_FORMAT_B5G6R5)
                                ? 0
                                : 8))));
  bits.colorBits =
      bits.redBits + bits.greenBits + bits.blueBits + bits.alphaBits;
  bits.depthBits =
      ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D16)
           ? 16
           : ((depthFormat == KS_GPU_SURFACE_DEPTH_FORMAT_D24) ? 24 : 0));
  return bits;
}

// struct pfn {
// PFNGLBLENDCOLORPROC glBlendColor;
// PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
// PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
// PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
// PFNWGLDELAYBEFORESWAPNVPROC wglDelayBeforeSwapNV;

inline PROC GetExtension(const char *functionName) {
  return wglGetProcAddress(functionName);
}

inline void GlBootstrapExtensions() {
  // #if defined(OS_WINDOWS)
  wglChoosePixelFormatARB =
      (PFNWGLCHOOSEPIXELFORMATARBPROC)GetExtension("wglChoosePixelFormatARB");
  wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)GetExtension(
      "wglCreateContextAttribsARB");
  wglSwapIntervalEXT =
      (PFNWGLSWAPINTERVALEXTPROC)GetExtension("wglSwapIntervalEXT");
  wglDelayBeforeSwapNV =
      (PFNWGLDELAYBEFORESWAPNVPROC)GetExtension("wglDelayBeforeSwapNV");
  // #elif defined(OS_LINUX) && !defined(OS_LINUX_WAYLAND)
  //   glXCreateContextAttribsARB =
  //   (PFNGLXCREATECONTEXTATTRIBSARBPROC)GetExtension(
  //       "glXCreateContextAttribsARB");
  //   glXSwapIntervalEXT =
  //       (PFNGLXSWAPINTERVALEXTPROC)GetExtension("glXSwapIntervalEXT");
  //   glXDelayBeforeSwapNV =
  //       (PFNGLXDELAYBEFORESWAPNVPROC)GetExtension("glXDelayBeforeSwapNV");
  // #endif
}
// };

inline HGLRC ksGpuContext_CreateForSurface(
    const char *APPLICATION_NAME, HINSTANCE hInstance, HDC hDC,
    const ksGpuSurfaceColorFormat colorFormat =
        KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8,
    const ksGpuSurfaceDepthFormat depthFormat = KS_GPU_SURFACE_DEPTH_FORMAT_D24,
    const ksGpuSampleCount sampleCount = KS_GPU_SAMPLE_COUNT_1) {
  // UNUSED_PARM(queueIndex);

  // context->device = device;

  const ksGpuSurfaceBits bits =
      ksGpuContext_BitsForSurfaceFormat(colorFormat, depthFormat);

  PIXELFORMATDESCRIPTOR pfd = {
      sizeof(PIXELFORMATDESCRIPTOR),
      1,                       // version
      PFD_DRAW_TO_WINDOW |     // must support windowed
          PFD_SUPPORT_OPENGL | // must support OpenGL
          PFD_DOUBLEBUFFER,    // must support double buffering
      PFD_TYPE_RGBA,           // iPixelType
      bits.colorBits,          // cColorBits
      0,
      0, // cRedBits, cRedShift
      0,
      0, // cGreenBits, cGreenShift
      0,
      0, // cBlueBits, cBlueShift
      0,
      0,              // cAlphaBits, cAlphaShift
      0,              // cAccumBits
      0,              // cAccumRedBits
      0,              // cAccumGreenBits
      0,              // cAccumBlueBits
      0,              // cAccumAlphaBits
      bits.depthBits, // cDepthBits
      0,              // cStencilBits
      0,              // cAuxBuffers
      PFD_MAIN_PLANE, // iLayerType
      0,              // bReserved
      0,              // dwLayerMask
      0,              // dwVisibleMask
      0               // dwDamageMask
  };

  HWND localWnd = NULL;
  HDC localDC = hDC;

  if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
    // A valid OpenGL context is needed to get OpenGL extensions including
    // wglChoosePixelFormatARB and wglCreateContextAttribsARB. A device context
    // with a valid pixel format is needed to create an OpenGL context. However,
    // once a pixel format is set on a device context it is final. Therefore a
    // pixel format is set on the device context of a temporary window to create
    // a context to get the extensions for multi-sampling.
    localWnd = CreateWindowA(APPLICATION_NAME, "temp", 0, 0, 0, 0, 0, NULL,
                             NULL, hInstance, NULL);
    localDC = GetDC(localWnd);
  }

  int pixelFormat = ChoosePixelFormat(localDC, &pfd);
  if (pixelFormat == 0) {
    throw std::runtime_error("Failed to find a suitable pixel format.");
  }

  if (!SetPixelFormat(localDC, pixelFormat, &pfd)) {
    throw std::runtime_error("Failed to set the pixel format.");
  }

  // Now that the pixel format is set, create a temporary context to get the
  // extensions.
  {
    HGLRC hGLRC = wglCreateContext(localDC);
    wglMakeCurrent(localDC, hGLRC);

    GlBootstrapExtensions();

    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(hGLRC);
  }

  if (sampleCount > KS_GPU_SAMPLE_COUNT_1) {
    // Release the device context and destroy the window that were created to
    // get extensions.
    ReleaseDC(localWnd, localDC);
    DestroyWindow(localWnd);

    int pixelFormatAttribs[] = {WGL_DRAW_TO_WINDOW_ARB,
                                GL_TRUE,
                                WGL_SUPPORT_OPENGL_ARB,
                                GL_TRUE,
                                WGL_DOUBLE_BUFFER_ARB,
                                GL_TRUE,
                                WGL_PIXEL_TYPE_ARB,
                                WGL_TYPE_RGBA_ARB,
                                WGL_COLOR_BITS_ARB,
                                bits.colorBits,
                                WGL_DEPTH_BITS_ARB,
                                bits.depthBits,
                                WGL_SAMPLE_BUFFERS_ARB,
                                1,
                                WGL_SAMPLES_ARB,
                                sampleCount,
                                0};

    unsigned int numPixelFormats = 0;

    if (!wglChoosePixelFormatARB(hDC, pixelFormatAttribs, NULL, 1, &pixelFormat,
                                 &numPixelFormats) ||
        numPixelFormats == 0) {
      throw std::runtime_error("Failed to find MSAA pixel format.");
    }

    memset(&pfd, 0, sizeof(pfd));

    if (!DescribePixelFormat(hDC, pixelFormat, sizeof(PIXELFORMATDESCRIPTOR),
                             &pfd)) {
      throw std::runtime_error("Failed to describe the pixel format.");
    }

    if (!SetPixelFormat(hDC, pixelFormat, &pfd)) {
      throw std::runtime_error("Failed to set the pixel format.");
    }
  }

  auto OPENGL_VERSION_MAJOR = 4;
  auto OPENGL_VERSION_MINOR = 3;

  int contextAttribs[] = {WGL_CONTEXT_MAJOR_VERSION_ARB,
                          OPENGL_VERSION_MAJOR,
                          WGL_CONTEXT_MINOR_VERSION_ARB,
                          OPENGL_VERSION_MINOR,
                          WGL_CONTEXT_PROFILE_MASK_ARB,
                          WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
                          WGL_CONTEXT_FLAGS_ARB,
                          WGL_CONTEXT_FORWARD_COMPATIBLE_BIT_ARB |
                              WGL_CONTEXT_DEBUG_BIT_ARB,
                          0};

  // context->hDC = hDC;
  auto hGLRC = wglCreateContextAttribsARB(hDC, NULL, contextAttribs);
  if (!hGLRC) {
    throw std::runtime_error("Failed to create GL context.");
  }

  wglMakeCurrent(hDC, hGLRC);

  // GlInitExtensions();

  return hGLRC;
}

inline std::tuple<gl::OpenGL, XrGraphicsBindingOpenGLWin32KHR>
createGl(XrInstance instance, XrSystemId systemId) {
  // Extension function must be loaded by name
  PFN_xrGetOpenGLGraphicsRequirementsKHR pfnGetOpenGLGraphicsRequirementsKHR =
      nullptr;
  CheckXrResult(
      xrGetInstanceProcAddr(instance, "xrGetOpenGLGraphicsRequirementsKHR",
                            reinterpret_cast<PFN_xrVoidFunction *>(
                                &pfnGetOpenGLGraphicsRequirementsKHR)));

  XrGraphicsRequirementsOpenGLKHR graphicsRequirements{
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_KHR};
  CheckXrResult(pfnGetOpenGLGraphicsRequirementsKHR(instance, systemId,
                                                    &graphicsRequirements));

  // // Initialize the gl extensions. Note we have to open a window.
  // ksDriverInstance driverInstance{};
  // ksGpuQueueInfo queueInfo{};
  // ksGpuSurfaceColorFormat colorFormat{KS_GPU_SURFACE_COLOR_FORMAT_B8G8R8A8};
  // ksGpuSurfaceDepthFormat depthFormat{KS_GPU_SURFACE_DEPTH_FORMAT_D24};
  // ksGpuSampleCount sampleCount{KS_GPU_SAMPLE_COUNT_1};
  // if (!ksGpuWindow_Create(&window, &driverInstance, &queueInfo, 0,
  // colorFormat,
  //                         depthFormat, sampleCount, 640, 480, false)) {
  //   throw std::runtime_error("Unable to create GL context");
  // }

  auto hInstance = GetModuleHandleA(NULL);

  auto APPLICATION_NAME = "vuloxr_klass";
  auto WINDOW_TITLE = "vuloxr";

  WNDCLASSA wc;
  wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
  wc.lpfnWndProc = (WNDPROC)WndProc;
  wc.cbClsExtra = 0;
  wc.cbWndExtra = 0;
  wc.hInstance = hInstance;
  wc.hIcon = LoadIcon(NULL, IDI_WINLOGO);
  wc.hCursor = LoadCursor(NULL, IDC_ARROW);
  wc.hbrBackground = NULL;
  wc.lpszMenuName = NULL;
  wc.lpszClassName = APPLICATION_NAME;
  if (!RegisterClassA(&wc)) {
    throw std::runtime_error("Failed to register window class.");
  }

  DWORD dwExStyle = 0;
  DWORD dwStyle = 0;
  {
    // Fixed size window.
    dwExStyle = WS_EX_APPWINDOW | WS_EX_WINDOWEDGE;
    dwStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
  }

  RECT windowRect;
  windowRect.left = (long)0;
  windowRect.right = (long)640;
  windowRect.top = (long)0;
  windowRect.bottom = (long)480;
  AdjustWindowRectEx(&windowRect, dwStyle, FALSE, dwExStyle);
  auto hWnd =
      CreateWindowExA(dwExStyle,            // Extended style for the window
                      APPLICATION_NAME,     // Class name
                      WINDOW_TITLE,         // Window title
                      dwStyle |             // Defined window style
                          WS_CLIPSIBLINGS | // Required window style
                          WS_CLIPCHILDREN,  // Required window style
                      windowRect.left,      // Window X position
                      windowRect.top,       // Window Y position
                      windowRect.right - windowRect.left, // Window width
                      windowRect.bottom - windowRect.top, // Window height
                      NULL,                               // No parent window
                      NULL,                               // No menu
                      hInstance,                          // Instance
                      NULL); // No WM_CREATE parameter
  if (!hWnd) {
    // ksGpuWindow_Destroy(window);
    throw std::runtime_error("Failed to create window.");
  }

  auto hDC = GetDC(hWnd);
  if (!hDC) {
    throw std::runtime_error("Failed to acquire device context.");
  }

  // ksGpuDevice_Create(&window->device, instance, queueInfo);
  auto hGLRC = ksGpuContext_CreateForSurface(APPLICATION_NAME, hInstance, hDC);
  // ksGpuContext_SetCurrent(&window->context);
  wglMakeCurrent(hDC, hGLRC);

  ShowWindow(hWnd, SW_SHOW);
  SetForegroundWindow(hWnd);
  SetFocus(hWnd);

  glewInit();

  GLint major = 0;
  GLint minor = 0;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  const XrVersion desiredApiVersion = XR_MAKE_VERSION(major, minor, 0);
  if (graphicsRequirements.minApiVersionSupported > desiredApiVersion) {
    throw std::runtime_error(
        "Runtime does not support desired Graphics API and/or version");
  }

  XrGraphicsBindingOpenGLWin32KHR binding{
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_WIN32_KHR,
      .hDC = hDC,
      .hGLRC = hGLRC,
  };

  return {
      {},
      binding,
  };
}

} // namespace xr
} // namespace vuloxr
