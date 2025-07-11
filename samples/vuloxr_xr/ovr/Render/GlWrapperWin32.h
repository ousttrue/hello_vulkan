/*
 * Copyright (c) Meta Platforms, Inc. and affiliates.
 * All rights reserved.
 *
 * Licensed under the Oculus SDK License Agreement (the "License");
 * you may not use the Oculus SDK except in compliance with the License,
 * which is provided at the time of installation or download, or which
 * otherwise accompanies this software in either electronic or hard copy form.
 *
 * You may obtain a copy of the License at
 * https://developer.oculus.com/licenses/oculussdk/
 *
 * Unless required by applicable law or agreed to in writing, the Oculus SDK
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
/*
Copyright (c) 2016 Oculus VR, LLC.
*/

// This file is a cut down version of gfxwrapper_opengl.h from the OpenXR SDK.
// It is originally based on the Oculus multiplatform GL wrapper but all of the
// code for platforms other than WIN32 has been removed.

#pragma once

#if defined(WIN32)

#ifdef __cplusplus
extern "C" {
#endif

#define OPENGL_VERSION_MAJOR 4
#define OPENGL_VERSION_MINOR 3
#define GLSL_VERSION "430"
#define SPIRV_VERSION "99"

#define GL_APIENTRY APIENTRY
#define GL_APIENTRYP APIENTRYP

#include <windows.h>
#include <GL/gl.h>
#define GL_EXT_color_subtable
#include <GL/glext.h>
#include <GL/wglext.h>
#include "gl_format.h"

#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_NUM_VIEWS_OVR 0x9630
#define GL_FRAMEBUFFER_ATTACHMENT_TEXTURE_BASE_VIEW_INDEX_OVR 0x9632
#define GL_MAX_VIEWS_OVR 0x9631

extern PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers;
extern PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers;
extern PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer;
extern PFNGLBLITFRAMEBUFFERPROC glBlitFramebuffer;
extern PFNGLGENRENDERBUFFERSPROC glGenRenderbuffers;
extern PFNGLDELETERENDERBUFFERSPROC glDeleteRenderbuffers;
extern PFNGLBINDRENDERBUFFERPROC glBindRenderbuffer;
extern PFNGLISRENDERBUFFERPROC glIsRenderbuffer;
extern PFNGLRENDERBUFFERSTORAGEPROC glRenderbufferStorage;
extern PFNGLRENDERBUFFERSTORAGEMULTISAMPLEPROC glRenderbufferStorageMultisample;
extern PFNGLFRAMEBUFFERRENDERBUFFERPROC glFramebufferRenderbuffer;
extern PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D;
extern PFNGLFRAMEBUFFERTEXTURELAYERPROC glFramebufferTextureLayer;
extern PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus;
extern PFNGLCHECKNAMEDFRAMEBUFFERSTATUSPROC glCheckNamedFramebufferStatus;

extern PFNGLGENBUFFERSPROC glGenBuffers;
extern PFNGLDELETEBUFFERSPROC glDeleteBuffers;
extern PFNGLBINDBUFFERPROC glBindBuffer;
extern PFNGLBINDBUFFERBASEPROC glBindBufferBase;
extern PFNGLBUFFERDATAPROC glBufferData;
extern PFNGLBUFFERSUBDATAPROC glBufferSubData;
extern PFNGLBUFFERSTORAGEPROC glBufferStorage;
extern PFNGLMAPBUFFERPROC glMapBuffer;
extern PFNGLMAPBUFFERRANGEPROC glMapBufferRange;
extern PFNGLUNMAPBUFFERPROC glUnmapBuffer;

extern PFNGLGENVERTEXARRAYSPROC glGenVertexArrays;
extern PFNGLDELETEVERTEXARRAYSPROC glDeleteVertexArrays;
extern PFNGLBINDVERTEXARRAYPROC glBindVertexArray;
extern PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer;
extern PFNGLVERTEXATTRIBDIVISORPROC glVertexAttribDivisor;
extern PFNGLDISABLEVERTEXATTRIBARRAYPROC glDisableVertexAttribArray;
extern PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray;

extern PFNGLACTIVETEXTUREPROC glActiveTexture;
extern PFNGLTEXIMAGE3DPROC glTexImage3D;
extern PFNGLCOMPRESSEDTEXIMAGE2DPROC glCompressedTexImage2D;
extern PFNGLCOMPRESSEDTEXIMAGE3DPROC glCompressedTexImage3D;
extern PFNGLTEXSUBIMAGE3DPROC glTexSubImage3D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE2DPROC glCompressedTexSubImage2D;
extern PFNGLCOMPRESSEDTEXSUBIMAGE3DPROC glCompressedTexSubImage3D;

extern PFNGLTEXSTORAGE2DPROC glTexStorage2D;
extern PFNGLTEXSTORAGE3DPROC glTexStorage3D;
extern PFNGLTEXIMAGE2DMULTISAMPLEPROC glTexImage2DMultisample;
extern PFNGLTEXIMAGE3DMULTISAMPLEPROC glTexImage3DMultisample;
extern PFNGLTEXSTORAGE2DMULTISAMPLEPROC glTexStorage2DMultisample;
extern PFNGLTEXSTORAGE3DMULTISAMPLEPROC glTexStorage3DMultisample;
extern PFNGLGENERATEMIPMAPPROC glGenerateMipmap;
extern PFNGLBINDIMAGETEXTUREPROC glBindImageTexture;

extern PFNGLCREATEPROGRAMPROC glCreateProgram;
extern PFNGLDELETEPROGRAMPROC glDeleteProgram;
extern PFNGLCREATESHADERPROC glCreateShader;
extern PFNGLDELETESHADERPROC glDeleteShader;
extern PFNGLSHADERSOURCEPROC glShaderSource;
extern PFNGLCOMPILESHADERPROC glCompileShader;
extern PFNGLGETSHADERIVPROC glGetShaderiv;
extern PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog;
extern PFNGLUSEPROGRAMPROC glUseProgram;
extern PFNGLATTACHSHADERPROC glAttachShader;
extern PFNGLLINKPROGRAMPROC glLinkProgram;
extern PFNGLGETPROGRAMIVPROC glGetProgramiv;
extern PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog;
extern PFNGLGETATTRIBLOCATIONPROC glGetAttribLocation;
extern PFNGLBINDATTRIBLOCATIONPROC glBindAttribLocation;
extern PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation;
extern PFNGLGETUNIFORMBLOCKINDEXPROC glGetUniformBlockIndex;
extern PFNGLGETPROGRAMRESOURCEINDEXPROC glGetProgramResourceIndex;
extern PFNGLUNIFORMBLOCKBINDINGPROC glUniformBlockBinding;
extern PFNGLSHADERSTORAGEBLOCKBINDINGPROC glShaderStorageBlockBinding;
extern PFNGLPROGRAMUNIFORM1IPROC glProgramUniform1i;
extern PFNGLUNIFORM1IPROC glUniform1i;
extern PFNGLUNIFORM1IVPROC glUniform1iv;
extern PFNGLUNIFORM2IVPROC glUniform2iv;
extern PFNGLUNIFORM3IVPROC glUniform3iv;
extern PFNGLUNIFORM4IVPROC glUniform4iv;
extern PFNGLUNIFORM1FPROC glUniform1f;
extern PFNGLUNIFORM3FPROC glUniform3f;
extern PFNGLUNIFORM1FVPROC glUniform1fv;
extern PFNGLUNIFORM2FVPROC glUniform2fv;
extern PFNGLUNIFORM3FVPROC glUniform3fv;
extern PFNGLUNIFORM4FVPROC glUniform4fv;
extern PFNGLUNIFORMMATRIX2FVPROC glUniformMatrix2fv;
extern PFNGLUNIFORMMATRIX2X3FVPROC glUniformMatrix2x3fv;
extern PFNGLUNIFORMMATRIX2X4FVPROC glUniformMatrix2x4fv;
extern PFNGLUNIFORMMATRIX3X2FVPROC glUniformMatrix3x2fv;
extern PFNGLUNIFORMMATRIX3FVPROC glUniformMatrix3fv;
extern PFNGLUNIFORMMATRIX3X4FVPROC glUniformMatrix3x4fv;
extern PFNGLUNIFORMMATRIX4X2FVPROC glUniformMatrix4x2fv;
extern PFNGLUNIFORMMATRIX4X3FVPROC glUniformMatrix4x3fv;
extern PFNGLUNIFORMMATRIX4FVPROC glUniformMatrix4fv;

extern PFNGLDRAWELEMENTSINSTANCEDPROC glDrawElementsInstanced;
extern PFNGLDISPATCHCOMPUTEPROC glDispatchCompute;
extern PFNGLMEMORYBARRIERPROC glMemoryBarrier;

extern PFNGLGENQUERIESPROC glGenQueries;
extern PFNGLDELETEQUERIESPROC glDeleteQueries;
extern PFNGLISQUERYPROC glIsQuery;
extern PFNGLBEGINQUERYPROC glBeginQuery;
extern PFNGLENDQUERYPROC glEndQuery;
extern PFNGLQUERYCOUNTERPROC glQueryCounter;
extern PFNGLGETQUERYIVPROC glGetQueryiv;
extern PFNGLGETQUERYOBJECTIVPROC glGetQueryObjectiv;
extern PFNGLGETQUERYOBJECTUIVPROC glGetQueryObjectuiv;
extern PFNGLGETQUERYOBJECTI64VPROC glGetQueryObjecti64v;
extern PFNGLGETQUERYOBJECTUI64VPROC glGetQueryObjectui64v;

extern PFNGLFENCESYNCPROC glFenceSync;
extern PFNGLCLIENTWAITSYNCPROC glClientWaitSync;
extern PFNGLDELETESYNCPROC glDeleteSync;
extern PFNGLISSYNCPROC glIsSync;

extern PFNGLBLENDFUNCSEPARATEPROC glBlendFuncSeparate;
extern PFNGLBLENDEQUATIONSEPARATEPROC glBlendEquationSeparate;

extern PFNGLDEBUGMESSAGECONTROLPROC glDebugMessageControl;
extern PFNGLDEBUGMESSAGECALLBACKPROC glDebugMessageCallback;

extern PFNGLBLENDCOLORPROC glBlendColor;
extern PFNWGLCHOOSEPIXELFORMATARBPROC wglChoosePixelFormatARB;
extern PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB;
extern PFNWGLSWAPINTERVALEXTPROC wglSwapIntervalEXT;
extern PFNWGLDELAYBEFORESWAPNVPROC wglDelayBeforeSwapNV;

extern PFNGLDEPTHRANGEFPROC glDepthRangef;
extern PFNGLBLENDEQUATIONPROC glBlendEquation;
extern PFNGLINVALIDATEFRAMEBUFFERPROC glInvalidateFramebuffer;

void ovrGl_CreateContext_Windows(HDC* hDC, HGLRC* hGLRC);
void ovrGl_DestroyContext_Windows();
const char* ovrGl_ErrorString_Windows(const GLint error);

#ifdef __cplusplus
}
#endif

#endif // defined(WIN32)
