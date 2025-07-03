#pragma once

#ifdef XR_USE_GRAPHICS_API_OPENGL
#include <Windows.h>

#include <GL/glew.h>
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>
#include <GLES3/gl31.h>
#include <GLES3/gl3ext.h>
#endif
