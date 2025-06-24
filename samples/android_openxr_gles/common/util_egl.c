/* ------------------------------------------------ *
 * The MIT License (MIT)
 * Copyright (c) 2019 terryky1220@gmail.com
 * ------------------------------------------------ */
#include "util_egl.h"
#include "winsys/winsys.h"
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#if 1
#define EGLASSERT() AssertEGLError(__FILE__, __LINE__)
#else
#define EGLASSERT() ((void)0)
#endif

static char *GetEGLErrMsg(int nCode) {
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

void AssertEGLError(char *lpFile, int nLine) {
  int error;

  while ((error = eglGetError()) != EGL_SUCCESS) {
    printf("[EGL ASSERT ERR] \"%s\"(%d):0x%04x(%s)\n", lpFile, nLine, error,
           GetEGLErrMsg(error));
  }
}

// #define USE_EGL_DEBUG 1

static EGLDisplay s_dpy;
static EGLSurface s_sfc;
static EGLContext s_ctx;

static int init_ext_functions() { return 0; }

// int
// egl_init_with_window_surface (int gles_version, void *window, int depth_size,
// int stencil_size, int sample_num)
// {
//     EGLint      major, minor;
//     EGLConfig   config;
//     EGLBoolean  ret;
//
//     EGLint context_attribs[] =
//     {
//         EGL_CONTEXT_CLIENT_VERSION, 2,
//         EGL_NONE
//     };
//
//     EGLint sfc_attr[] = { EGL_NONE };
//
//     s_dpy = eglGetDisplay (EGL_DEFAULT_DISPLAY);
//     if (s_dpy == EGL_NO_DISPLAY)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return -1;
//     }
//
//     ret = eglInitialize (s_dpy, &major, &minor);
//     if (ret != EGL_TRUE)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return -1;
//     }
//
//     config = find_egl_config (8, 8, 8, 8, depth_size, stencil_size,
//     sample_num, EGL_WINDOW_BIT, gles_version); if (ret != EGL_TRUE)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return -1;
//     }
//
//     s_sfc = eglCreateWindowSurface (s_dpy, config, (NativeWindowType)window,
//     sfc_attr); if (s_sfc == EGL_NO_SURFACE)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return( -1 );
//     }
//     EGLASSERT();
//
//     eglBindAPI (EGL_OPENGL_ES_API);
//
//     switch (gles_version)
//     {
//     case 1: context_attribs[1] = 1; break;
//     case 2: context_attribs[1] = 2; break;
//     case 3: context_attribs[1] = 3; break;
//     default:
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return -1;
//     }
//
//     s_ctx = eglCreateContext (s_dpy, config, EGL_NO_CONTEXT,
//     context_attribs); if (s_ctx== EGL_NO_CONTEXT)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return( -1 );
//     }
//     EGLASSERT();
//
//     ret = eglMakeCurrent (s_dpy, s_sfc, s_sfc, s_ctx);
//     if (ret != EGL_TRUE)
//     {
//         fprintf (stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//         return -1;
//     }
//     EGLASSERT();
//
//     return 0;
// }

// int egl_init_with_platform_window_surface(int gles_version, int depth_size,
//                                           int stencil_size, int sample_num,
//                                           int win_w, int win_h) {
//   void *native_dpy, *native_win;
//   EGLint major, minor;
//   EGLConfig config;
//   EGLBoolean ret;
//   EGLint context_attribs[] = {EGL_CONTEXT_CLIENT_VERSION, 2, EGL_NONE};
//   EGLint sfc_attr[] = {EGL_NONE};
//
//   init_ext_functions();
//
//   native_dpy = winsys_init_native_display();
//   if ((native_dpy != EGL_DEFAULT_DISPLAY) && (native_dpy == NULL)) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   s_dpy = eglGetDisplay(native_dpy);
//   if (s_dpy == EGL_NO_DISPLAY) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   ret = eglInitialize(s_dpy, &major, &minor);
//   if (ret != EGL_TRUE) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   eglBindAPI(EGL_OPENGL_ES_API);
//
//   config = find_egl_config(8, 8, 8, 8, depth_size, stencil_size, sample_num,
//                            EGL_WINDOW_BIT, gles_version);
//   if (config == NULL) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   native_win = winsys_init_native_window(s_dpy, win_w, win_h);
//   if (native_win == NULL) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   // s_sfc = eglCreatePlatformWindowSurfaceEXT (s_dpy, config, native_win,
//   // sfc_attr);
//   s_sfc = eglCreateWindowSurface(s_dpy, config, (NativeWindowType)native_win,
//                                  sfc_attr);
//   if (s_sfc == EGL_NO_SURFACE) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return (-1);
//   }
//
//   switch (gles_version) {
//   case 1:
//     context_attribs[1] = 1;
//     break;
//   case 2:
//     context_attribs[1] = 2;
//     break;
//   case 3:
//     context_attribs[1] = 3;
//     break;
//   default:
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//
//   s_ctx = eglCreateContext(s_dpy, config, EGL_NO_CONTEXT, context_attribs);
//   if (s_ctx == EGL_NO_CONTEXT) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return (-1);
//   }
//   EGLASSERT();
//
//   ret = eglMakeCurrent(s_dpy, s_sfc, s_sfc, s_ctx);
//   if (ret != EGL_TRUE) {
//     fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
//     return -1;
//   }
//   EGLASSERT();
//
// #if defined(USE_EGL_DEBUG)
//   glext_enable_debug();
// #endif
//
//   return 0;
// }

int egl_terminate() {
  EGLBoolean ret;

  ret = eglMakeCurrent(s_dpy, NULL, NULL, NULL);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }
  EGLASSERT();

  ret = eglDestroySurface(s_dpy, s_sfc);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }
  EGLASSERT();

  ret = eglDestroyContext(s_dpy, s_ctx);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }
  EGLASSERT();

  ret = eglTerminate(s_dpy);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  return 0;
}

int egl_swap() {
  EGLBoolean ret;

  ret = eglSwapBuffers(s_dpy, s_sfc);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  winsys_swap();

  return 0;
}

int egl_set_swap_interval(int interval) {
  EGLBoolean ret;

  ret = eglSwapInterval(s_dpy, interval);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  return 0;
}

int egl_get_current_surface_dimension(int *width, int *height) {
  EGLDisplay dpy;
  EGLSurface sfc;
  EGLBoolean ret;

  if (width == NULL || height == NULL) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  *width = *height = 0;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  sfc = eglGetCurrentSurface(EGL_DRAW);
  if (sfc == EGL_NO_SURFACE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  ret = eglQuerySurface(dpy, sfc, EGL_WIDTH, width);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  ret = eglQuerySurface(dpy, sfc, EGL_HEIGHT, height);
  if (ret != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  return 0;
}

EGLDisplay egl_get_display() {
  EGLDisplay dpy;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
  }

  return dpy;
}

EGLContext egl_get_context() {
  EGLDisplay dpy;
  EGLContext ctx;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return ctx;
}

EGLSurface egl_get_surface() {
  EGLDisplay dpy;
  EGLSurface sfc;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_SURFACE;
  }

  sfc = eglGetCurrentSurface(EGL_DRAW);
  if (sfc == EGL_NO_SURFACE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_SURFACE;
  }

  return sfc;
}

EGLConfig egl_get_config() {
  EGLDisplay dpy;
  EGLContext ctx;
  EGLConfig cfg;
  int cfg_id, ival;
  EGLint cfg_attribs[] = {EGL_CONFIG_ID, 0, EGL_NONE};

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  ctx = eglGetCurrentContext();
  if (ctx == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &cfg_id);
  cfg_attribs[1] = cfg_id;
  if (eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ival) != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_CONTEXT;
  }

  return cfg;
}

int egl_show_current_context_attrib(FILE *fp) {
  EGLDisplay dpy;
  EGLContext ctx;
  EGLint ival;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  ctx = eglGetCurrentContext();
  if (dpy == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  fprintf(fp, "-------------------------------------------------\n");
  fprintf(fp, "  CONTEXT ATTRIBUTE                              \n");
  fprintf(fp, "-------------------------------------------------\n");

  fprintf(fp, " %-32s: ", "EGL_CONFIG_ID");
  ival = -1;
  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_CONTEXT_CLIENT_TYPE");
  ival = -1;
  eglQueryContext(dpy, ctx, EGL_CONTEXT_CLIENT_TYPE, &ival);
  switch (ival) {
  case EGL_OPENGL_API:
    fprintf(fp, "EGL_OPENGL_API\n");
    break;
  case EGL_OPENGL_ES_API:
    fprintf(fp, "EGL_OPENGL_ES_API\n");
    break;
  case EGL_OPENVG_API:
    fprintf(fp, "EGL_OPENVG_API\n");
    break;
  default:
    fprintf(fp, "UNKNOWN\n");
    break;
  }

  fprintf(fp, " %-32s: ", "EGL_CONTEXT_CLIENT_VERSION");
  ival = -1;
  eglQueryContext(dpy, ctx, EGL_CONTEXT_CLIENT_VERSION, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_RENDER_BUFFER");
  ival = -1;
  eglQueryContext(dpy, ctx, EGL_RENDER_BUFFER, &ival);
  switch (ival) {
  case EGL_SINGLE_BUFFER:
    fprintf(fp, "EGL_SINGLE_BUFFER\n");
    break;
  case EGL_BACK_BUFFER:
    fprintf(fp, "EGL_BACK_BUFFER\n");
    break;
  default:
    fprintf(fp, "UNKNOWN\n");
    break;
  }
  fprintf(fp, "-------------------------------------------------\n");
  return 0;
}

int egl_show_current_config_attrib(FILE *fp) {
  EGLDisplay dpy;
  EGLContext ctx;
  EGLConfig cfg;
  EGLint ival;
  int cfg_id;
  EGLint cfg_attribs[] = {EGL_CONFIG_ID, 0, EGL_NONE};

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  ctx = eglGetCurrentContext();
  if (dpy == EGL_NO_CONTEXT) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  eglQueryContext(dpy, ctx, EGL_CONFIG_ID, &cfg_id);

  cfg_attribs[1] = cfg_id;
  if (eglChooseConfig(dpy, cfg_attribs, &cfg, 1, &ival) != EGL_TRUE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  fprintf(fp, "-------------------------------------------------\n");
  fprintf(fp, "  CONFIG ATTRIBUTE                               \n");
  fprintf(fp, "-------------------------------------------------\n");

  fprintf(fp, " %-32s: ", "EGL_CONFIG_ID");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_CONFIG_ID, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_BUFFER_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_BUFFER_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_RED_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_RED_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_GREEN_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_GREEN_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_BLUE_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_BLUE_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_LUMINANCE_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_LUMINANCE_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_ALPHA_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_ALPHA_MASK_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_ALPHA_MASK_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_BIND_TO_TEXTURE_RGB");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_BIND_TO_TEXTURE_RGB, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_BIND_TO_TEXTURE_RGBA");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_BIND_TO_TEXTURE_RGBA, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_COLOR_BUFFER_TYPE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_COLOR_BUFFER_TYPE, &ival);
  switch (ival) {
  case EGL_RGB_BUFFER:
    fprintf(fp, "EGL_RGB_BUFFER\n");
    break;
  case EGL_LUMINANCE_BUFFER:
    fprintf(fp, "EGL_LUMINANCE_BUFFER\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_CONFIG_CAVEAT");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_CONFIG_CAVEAT, &ival);
  switch (ival) {
  case EGL_NONE:
    fprintf(fp, "EGL_NONE\n");
    break;
  case EGL_SLOW_CONFIG:
    fprintf(fp, "EGL_SLOW_CONFIG\n");
    break;
  case EGL_NON_CONFORMANT_CONFIG:
    fprintf(fp, "EGL_NON_CONFORMANT_CONFIG\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_CONFORMANT");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_CONFORMANT, &ival);
  fprintf(fp, "%d\n", ival);
  if (ival & EGL_OPENGL_ES_BIT)
    fprintf(fp, " %-32s  + EGL_OPENGL_ES_BIT\n", "");
  if (ival & EGL_OPENGL_ES2_BIT)
    fprintf(fp, " %-32s  + EGL_OPENGL_ES2_BIT\n", "");

  fprintf(fp, " %-32s: ", "EGL_DEPTH_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_DEPTH_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_LEVEL");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_LEVEL, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MATCH_NATIVE_PIXMAP");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_MATCH_NATIVE_PIXMAP, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MAX_SWAP_INTERVAL");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_MAX_SWAP_INTERVAL, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MIN_SWAP_INTERVAL");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_MIN_SWAP_INTERVAL, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_NATIVE_RENDERABLE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_RENDERABLE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_NATIVE_VISUAL_TYPE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_NATIVE_VISUAL_TYPE, &ival);
  switch (ival) {
  case EGL_NONE:
    fprintf(fp, "EGL_NONE\n");
    break;
  default:
    fprintf(fp, "UNKNOWN(%d)\n", ival);
  }

  fprintf(fp, " %-32s: ", "EGL_RENDERABLE_TYPE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_RENDERABLE_TYPE, &ival);
  fprintf(fp, "%d\n", ival);
  if (ival & EGL_OPENGL_ES_BIT)
    fprintf(fp, " %-32s  + EGL_OPENGL_ES_BIT\n", "");
  if (ival & EGL_OPENGL_ES2_BIT)
    fprintf(fp, " %-32s  + EGL_OPENGL_ES2_BIT\n", "");

  fprintf(fp, " %-32s: ", "EGL_SAMPLE_BUFFERS");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_SAMPLE_BUFFERS, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_SAMPLES");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_SAMPLES, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_STENCIL_SIZE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_STENCIL_SIZE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_SURFACE_TYPE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_SURFACE_TYPE, &ival);
  fprintf(fp, "%d\n", ival);
  if (ival & EGL_PBUFFER_BIT)
    fprintf(fp, " %-32s  + EGL_PBUFFER_BIT\n", "");
  if (ival & EGL_PIXMAP_BIT)
    fprintf(fp, " %-32s  + EGL_PIXMAP_BIT\n", "");
  if (ival & EGL_WINDOW_BIT)
    fprintf(fp, " %-32s  + EGL_WINDOW_BIT\n", "");
  if (ival & EGL_MULTISAMPLE_RESOLVE_BOX_BIT)
    fprintf(fp, " %-32s  + EGL_MULTISAMPLE_RESOLVE_BOX_BIT\n", "");
  if (ival & EGL_SWAP_BEHAVIOR_PRESERVED_BIT)
    fprintf(fp, " %-32s  + EGL_SWAP_BEHAVIOR_PRESERVED_BIT\n", "");

  fprintf(fp, " %-32s: ", "EGL_TRANSPARENT_TYPE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_TRANSPARENT_TYPE, &ival);
  switch (ival) {
  case EGL_NONE:
    fprintf(fp, "EGL_NONE\n");
    break;
  case EGL_TRANSPARENT_RGB:
    fprintf(fp, "EGL_TRANSPARENT_RGB\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_TRANSPARENT_RED_VALUE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_TRANSPARENT_RED_VALUE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_TRANSPARENT_GREEN_VALUE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_TRANSPARENT_GREEN_VALUE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_TRANSPARENT_BLUE_VALUE");
  ival = -1;
  eglGetConfigAttrib(dpy, cfg, EGL_TRANSPARENT_BLUE_VALUE, &ival);
  fprintf(fp, "%d\n", ival);
  fprintf(fp, "-------------------------------------------------\n");

  return 0;
}

int egl_show_current_surface_attrib(FILE *fp) {
  EGLDisplay dpy;
  EGLSurface sfc;
  EGLint ival;

  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  sfc = eglGetCurrentSurface(EGL_DRAW);
  if (sfc == EGL_NO_SURFACE) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  fprintf(fp, "-------------------------------------------------\n");
  fprintf(fp, "  SURFACE ATTRIBUTE                              \n");
  fprintf(fp, "-------------------------------------------------\n");

  fprintf(fp, " %-32s: ", "EGL_CONFIG_ID");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_CONFIG_ID, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_WIDTH");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_WIDTH, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_HEIGHT");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_HEIGHT, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_LARGEST_PBUFFER");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_LARGEST_PBUFFER, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MIPMAP_TEXTURE");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_MIPMAP_TEXTURE, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MIPMAP_LEVEL");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_MIPMAP_LEVEL, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_TEXTURE_FORMAT");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_TEXTURE_FORMAT, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_TEXTURE_TARGET");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_TEXTURE_TARGET, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_MULTISMAPLE_RESOLVE");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_MULTISAMPLE_RESOLVE, &ival);
  switch (ival) {
  case EGL_MULTISAMPLE_RESOLVE_DEFAULT:
    fprintf(fp, "EGL_MULTISAMPLE_RESOLVE_DEFAULT\n");
    break;
  case EGL_MULTISAMPLE_RESOLVE_BOX:
    fprintf(fp, "EGL_MULTISAMPLE_RESOLVE_BOX\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_RENDER_BUFFER");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_RENDER_BUFFER, &ival);
  switch (ival) {
  case EGL_BACK_BUFFER:
    fprintf(fp, "EGL_BACK_BUFFER\n");
    break;
  case EGL_SINGLE_BUFFER:
    fprintf(fp, "EGL_SINGLE_BUFFER\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_SWAP_BEHAVIOR");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_SWAP_BEHAVIOR, &ival);
  switch (ival) {
  case EGL_BUFFER_PRESERVED:
    fprintf(fp, "EGL_BUFFER_PRESERVED\n");
    break;
  case EGL_BUFFER_DESTROYED:
    fprintf(fp, "EGL_BUFFER_DESTROYED\n");
    break;
  default:
    fprintf(fp, "ERR\n");
  }

  fprintf(fp, " %-32s: ", "EGL_HORIZONTAL_RESOLUTION");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_HORIZONTAL_RESOLUTION, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_VERTICAL_RESOLUTION");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_VERTICAL_RESOLUTION, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, " %-32s: ", "EGL_PIXEL_ASPECT_RATIO");
  ival = -1;
  eglQuerySurface(dpy, sfc, EGL_PIXEL_ASPECT_RATIO, &ival);
  fprintf(fp, "%d\n", ival);

  fprintf(fp, "-------------------------------------------------\n");

  return 0;
}

int egl_show_egl_info(FILE *fp) {
  const char *str;

  EGLDisplay dpy;
  dpy = eglGetCurrentDisplay();
  if (dpy == EGL_NO_DISPLAY) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  fprintf(fp, "============================================\n");
  fprintf(fp, "    EGL INFO\n");
  fprintf(fp, "============================================\n");

  str = eglQueryString(dpy, EGL_CLIENT_APIS);
  fprintf(fp, "EGL_CLIENT_APIS : %s\n", str);

  str = eglQueryString(dpy, EGL_VENDOR);
  fprintf(fp, "EGL_VENDOR      : %s\n", str);

  str = eglQueryString(dpy, EGL_VERSION);
  fprintf(fp, "EGL_VERSION     : %s\n", str);

  str = eglQueryString(dpy, EGL_EXTENSIONS);
  fprintf(fp, "EGL_EXTENSIONS  :\n");
  if (str) {
    char *cpy, *p;

    /* on some platform, prohibited to overwrite the buffer. */
    cpy = (char *)malloc(strlen(str) + 1);
    strcpy(cpy, str);
    p = strtok((char *)cpy, " ");
    while (p) {
      fprintf(fp, "                  %s\n", p);
      p = strtok(NULL, " ");
    }
    free(cpy);
  }

  fprintf(fp, "\n");

  return 0;
}

int egl_show_gl_info(FILE *fp) {
  const unsigned char *str;

  fprintf(fp, "============================================\n");
  fprintf(fp, "    GL INFO\n");
  fprintf(fp, "============================================\n");

  fprintf(fp, "GL_VERSION      : %s\n", glGetString(GL_VERSION));
  fprintf(fp, "GL_SL_VERSION   : %s\n",
          glGetString(GL_SHADING_LANGUAGE_VERSION));
  fprintf(fp, "GL_VENDOR       : %s\n", glGetString(GL_VENDOR));
  fprintf(fp, "GL_RENDERER     : %s\n", glGetString(GL_RENDERER));

  str = glGetString(GL_EXTENSIONS);
  fprintf(fp, "GL_EXTENSIONS   :\n");
  {
    char *p = strtok((char *)str, " ");
    while (p) {
      fprintf(fp, "                  %s\n", p);
      p = strtok(NULL, " ");
    }
  }
  return 0;
}

int egl_capture_to_img(char *fname) {
  int ret;
  int width, height;
  char dst_fname[128];
  FILE *fp = NULL;
  void *buf = NULL;

  ret = egl_get_current_surface_dimension(&width, &height);
  if (ret < 0) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  sprintf(dst_fname, "%s_RGBA8888_SIZE%dx%d.img", fname, width, height);

  fp = fopen(dst_fname, "wb");
  if (fp == NULL) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return -1;
  }

  buf = malloc(width * height * 4);
  if (buf == NULL) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    goto err_exit;
  }

  glPixelStorei(GL_PACK_ALIGNMENT, 1);
  glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, buf);

  fwrite(buf, 4, width * height, fp);

  fclose(fp);
  free(buf);

  return 0;

err_exit:
  if (fp)
    fclose(fp);

  if (buf)
    free(buf);

  return -1;
}

EGLImageKHR egl_create_eglimage(int width, int height) {
#if defined(USE_EGL_DRM)
  void *pixmap;
  EGLImageKHR egl_img;
  EGLBoolean ret;
  EGLint attrs[13];
  int bo_fd;

  pixmap = winsys_create_native_pixmap(width, height);
  if (pixmap == NULL) {
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_IMAGE_KHR;
  }

  bo_fd = (intptr_t)pixmap;

  attrs[0] = EGL_DMA_BUF_PLANE0_FD_EXT;
  attrs[1] = bo_fd;
  attrs[2] = EGL_DMA_BUF_PLANE0_PITCH_EXT;
  attrs[3] = width * 4; // gbm_bo_get_stride (bo);
  attrs[4] = EGL_DMA_BUF_PLANE0_OFFSET_EXT;
  attrs[5] = 0;
  attrs[6] = EGL_WIDTH;
  attrs[7] = width; // gbm_bo_get_width (bo);
  attrs[8] = EGL_HEIGHT;
  attrs[9] = height; // gbm_bo_get_height(bo);
  attrs[10] = EGL_LINUX_DRM_FOURCC_EXT;
  attrs[11] = GBM_FORMAT_ARGB8888;
  attrs[12] = EGL_NONE;

  egl_img = eglCreateImageKHR(s_dpy, EGL_NO_CONTEXT, EGL_LINUX_DMA_BUF_EXT,
                              NULL, attrs);
  if (egl_img == EGL_NO_IMAGE_KHR) {
    EGLASSERT();
    fprintf(stderr, "ERR: %s(%d)\n", __FILE__, __LINE__);
    return EGL_NO_IMAGE_KHR;
  }

  return egl_img;
#else
  return EGL_NO_IMAGE_KHR;
#endif
}
