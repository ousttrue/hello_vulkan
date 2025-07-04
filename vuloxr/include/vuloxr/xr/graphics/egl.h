#pragma once
#include "../../../vuloxr.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include "../../xr.h"

#include <assert.h>
#include <span>
#include <stdio.h>
#include <stdlib.h>

#define EGLASSERT() AssertEGLError(__FILE__, __LINE__)

namespace vuloxr {

namespace egl {

struct OpenGLES {
  //
};

} // namespace egl

namespace xr {

inline EGLConfig _egl_get_config() {
  auto dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  auto ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  int cfg_id;
  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &cfg_id);

  EGLint cfg_attribs[] = {EGL_CONFIG_ID, 0, EGL_NONE};
  cfg_attribs[1] = cfg_id;

  int ival;
  EGLConfig cfg;
  if (eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ival) != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return cfg;
}

inline EGLContext _egl_get_context() {
  auto dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  auto ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return ctx;
}

inline const char *GetEGLErrMsg(int nCode) {
  switch (nCode) {
  case EGL_SUCCESS:
    return "EGL_SUCCESS";
    break;
  case EGL_NOT_INITIALIZED:
    return "EGL_NOT_INITIALIZED";
    break;
  case EGL_BAD_ACCESS:
    return "EGL_BAD_ACCESS";
    break;
  case EGL_BAD_ALLOC:
    return "EGL_BAD_ALLOC";
    break;
  case EGL_BAD_ATTRIBUTE:
    return "EGL_BAD_ATTRIBUTE";
    break;
  case EGL_BAD_CONTEXT:
    return "EGL_BAD_CONTEXT";
    break;
  case EGL_BAD_CONFIG:
    return "EGL_BAD_CONFIG";
    break;
  case EGL_BAD_CURRENT_SURFACE:
    return "EGL_BAD_CURRENT_SURFACE";
    break;
  case EGL_BAD_DISPLAY:
    return "EGL_BAD_DISPLAY";
    break;
  case EGL_BAD_SURFACE:
    return "EGL_BAD_SURFACE";
    break;
  case EGL_BAD_PARAMETER:
    return "EGL_BAD_PARAMETER";
    break;
  case EGL_BAD_NATIVE_PIXMAP:
    return "EGL_BAD_NATIVE_PIXMAP";
    break;
  case EGL_BAD_NATIVE_WINDOW:
    return "EGL_BAD_NATIVE_WINDOW";
    break;
  case EGL_CONTEXT_LOST:
    return "EGL_CONTEXT_LOST";
    break;
  default:
    return "UNKNOWN EGL ERROR";
    break;
  }
}

inline void AssertEGLError(const char *lpFile, int nLine) {
  int error;

  while ((error = eglGetError()) != EGL_SUCCESS) {
    printf("[EGL ASSERT ERR] \"%s\"(%d):0x%04x(%s)\n", lpFile, nLine, error,
           GetEGLErrMsg(error));
  }
}

static EGLConfig find_egl_config(int r, int g, int b, int a, int d, int s,
                                 int ms, int sfc_type, int ver) {
  EGLint num_conf, i;
  EGLBoolean ret;
  EGLConfig conf = 0, *conf_array = NULL;

  EGLint config_attribs[] = {EGL_RED_SIZE,
                             8, /*  0 */
                             EGL_GREEN_SIZE,
                             8, /*  2 */
                             EGL_BLUE_SIZE,
                             8, /*  4 */
                             EGL_ALPHA_SIZE,
                             8, /*  6 */
                             EGL_DEPTH_SIZE,
                             EGL_DONT_CARE, /*  8 */
                             EGL_STENCIL_SIZE,
                             EGL_DONT_CARE, /* 10 */
                             EGL_SAMPLES,
                             EGL_DONT_CARE, /* 12 */
                             EGL_SURFACE_TYPE,
                             EGL_WINDOW_BIT, /* 14 */
                             EGL_RENDERABLE_TYPE,
                             EGL_OPENGL_ES2_BIT,
                             EGL_NONE};

  config_attribs[1] = r;
  config_attribs[3] = g;
  config_attribs[5] = b;
  config_attribs[7] = a;
  config_attribs[9] = d;
  config_attribs[11] = s;
  config_attribs[13] = ms;
  config_attribs[15] = sfc_type; /* EGL_WINDOW_BIT/EGL_STREAM_BIT_KHR */

  switch (ver) {
  case 1:
  case 2:
    config_attribs[17] = EGL_OPENGL_ES2_BIT;
    break;
  case 3:
    config_attribs[17] = EGL_OPENGL_ES3_BIT;
    break;
  default:
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto exit;
  }

  ret = eglChooseConfig(eglGetDisplay(EGL_DEFAULT_DISPLAY), config_attribs,
                        NULL, 0, &num_conf);
  if (ret != EGL_TRUE || num_conf == 0) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto exit;
  }

  conf_array = (EGLConfig *)calloc(num_conf, sizeof(EGLConfig));
  if (conf_array == NULL) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto exit;
  }

  ret = eglChooseConfig(eglGetDisplay(EGL_DEFAULT_DISPLAY), config_attribs,
                        conf_array, num_conf, &num_conf);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto exit;
  }

  for (i = 0; i < num_conf; i++) {
    EGLint id, rsize, gsize, bsize, asize;

    eglGetConfigAttrib(eglGetDisplay(EGL_DEFAULT_DISPLAY), conf_array[i],
                       EGL_CONFIG_ID, &id);
    eglGetConfigAttrib(eglGetDisplay(EGL_DEFAULT_DISPLAY), conf_array[i],
                       EGL_RED_SIZE, &rsize);
    eglGetConfigAttrib(eglGetDisplay(EGL_DEFAULT_DISPLAY), conf_array[i],
                       EGL_GREEN_SIZE, &gsize);
    eglGetConfigAttrib(eglGetDisplay(EGL_DEFAULT_DISPLAY), conf_array[i],
                       EGL_BLUE_SIZE, &bsize);
    eglGetConfigAttrib(eglGetDisplay(EGL_DEFAULT_DISPLAY), conf_array[i],
                       EGL_ALPHA_SIZE, &asize);

    // fprintf (stderr, "[%d] (%d, %d, %d, %d)\n", id, rsize, gsize, bsize,
    // asize);

    if (rsize == r && gsize == g && bsize == b && asize == a) {
      conf = conf_array[i];
      break;
    }
  }

  if (i == num_conf) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto exit;
  }

exit:
  if (conf_array)
    free(conf_array);

  return conf;
}

inline XrGraphicsBindingOpenGLESAndroidKHR
getGraphicsBindingOpenGLESAndroidKHR() {
  return XrGraphicsBindingOpenGLESAndroidKHR{
      .type = XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
      .display = eglGetCurrentDisplay(),
      .config = _egl_get_config(),
      .context = _egl_get_context(),
  };
}

inline void getGLESGraphicsRequirementsKHR(XrInstance instance,
                                           XrSystemId sysid) {
  PFN_xrGetOpenGLESGraphicsRequirementsKHR xrGetOpenGLESGraphicsRequirementsKHR;
  xrGetInstanceProcAddr(
      instance, "xrGetOpenGLESGraphicsRequirementsKHR",
      (PFN_xrVoidFunction *)&xrGetOpenGLESGraphicsRequirementsKHR);

  XrGraphicsRequirementsOpenGLESKHR gfxReq = {
      XR_TYPE_GRAPHICS_REQUIREMENTS_OPENGL_ES_KHR};
  xrGetOpenGLESGraphicsRequirementsKHR(instance, sysid, &gfxReq);

  GLint major, minor;
  glGetIntegerv(GL_MAJOR_VERSION, &major);
  glGetIntegerv(GL_MINOR_VERSION, &minor);
  XrVersion glver = XR_MAKE_VERSION(major, minor, 0);

  Logger::Info(
      "GLES version: %" PRIx64 ", supported: (%" PRIx64 " - %" PRIx64 ")\n",
      glver, gfxReq.minApiVersionSupported, gfxReq.maxApiVersionSupported);

  assert(glver >= gfxReq.minApiVersionSupported ||
         glver <= gfxReq.maxApiVersionSupported);
}

inline std::tuple<egl::OpenGLES, XrGraphicsBindingOpenGLESAndroidKHR>
createEgl(XrInstance xr_instance, XrSystemId xr_systemId) {
  EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
  EGLint sfc_attr[] = {EGL_WIDTH, 16, EGL_HEIGHT, 16, EGL_NONE};

  auto s_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
  assert(s_dpy != EGL_NO_DISPLAY);

  EGLint major, minor;
  auto ret = eglInitialize(s_dpy, &major, &minor);
  assert(ret == EGL_TRUE);

  auto config = find_egl_config(8, 8, 8, 8, 24, 0, 0, EGL_DONT_CARE, 3);
  assert(ret == EGL_TRUE);

  auto s_sfc = eglCreatePbufferSurface(s_dpy, config, sfc_attr);
  assert(s_sfc != EGL_NO_SURFACE);

  EGLASSERT();

  eglBindAPI(EGL_OPENGL_ES_API);

  switch (3) {
  case 1:
    context_attribs[1] = 1;
    break;
  case 2:
    context_attribs[1] = 2;
    break;
  case 3:
    context_attribs[1] = 3;
    break;
  default:
    assert(false);
    return {};
  }

  auto s_ctx = eglCreateContext(s_dpy, config, EGL_NO_CONTEXT, context_attribs);
  assert(s_ctx != EGL_NO_CONTEXT);
  EGLASSERT();

  ret = eglMakeCurrent(s_dpy, s_sfc, s_sfc, s_ctx);
  assert(ret == EGL_TRUE);
  EGLASSERT();

  getGLESGraphicsRequirementsKHR(xr_instance, xr_systemId);

  egl::OpenGLES graphics;

  auto binding = getGraphicsBindingOpenGLESAndroidKHR();

  return {graphics, binding};
}

inline int64_t
selectColorSwapchainFormat(std::span<const int64_t> runtimeFormats) {
  // List of supported color swapchain formats.
  std::vector<int64_t> supportedColorSwapchainFormats{GL_RGBA8, GL_RGBA8_SNORM};

  // In OpenGLES 3.0+, the R, G, and B values after blending are converted into
  // the non-linear sRGB automatically.
  // if (m_contextApiMajorVersion >= 3) {
  supportedColorSwapchainFormats.push_back(GL_SRGB8_ALPHA8);
  // }

  auto swapchainFormatIt =
      std::find_first_of(runtimeFormats.begin(), runtimeFormats.end(),
                         supportedColorSwapchainFormats.begin(),
                         supportedColorSwapchainFormats.end());
  if (swapchainFormatIt == runtimeFormats.end()) {
    throw std::runtime_error(
        "No runtime swapchain format supported for color swapchain");
  }

  return *swapchainFormatIt;
}

} // namespace xr
} // namespace vuloxr
