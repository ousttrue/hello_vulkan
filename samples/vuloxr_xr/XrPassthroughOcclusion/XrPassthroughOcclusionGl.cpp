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
/************************************************************************************

Filename	:	XrPassthroughOcclusionGl.cpp
Content		:	This sample is derived from XrPassthrough.
                Used to demonstrate occlusions in passthrough

Copyright : Copyright (c) Meta Platforms, Inc. and affiliates. All rights reserved.

*************************************************************************************/

#include "XrPassthroughOcclusionGl.h"

#if defined(ANDROID)
#include <unistd.h>
#include <pthread.h>
#include <sys/prctl.h> // for prctl( PR_SET_NAME )
#include <android/log.h>
#include <android/native_window_jni.h> // for native window JNI
#include <android/input.h>
#endif // defined(ANDROID)

#include <atomic>
#include <thread>
#include <cmath>

#if defined(ANDROID)
#include <sys/system_properties.h>

#include <EGL/egl.h>
#include <GLES3/gl3.h>
#include <GLES3/gl3ext.h>
#endif // defined(ANDROID)

using namespace OVR;

// EXT_texture_border_clamp
#ifndef GL_CLAMP_TO_BORDER
#define GL_CLAMP_TO_BORDER 0x812D
#endif

#ifndef GL_TEXTURE_BORDER_COLOR
#define GL_TEXTURE_BORDER_COLOR 0x1004
#endif

#ifndef GL_FRAMEBUFFER_SRGB_EXT
#define GL_FRAMEBUFFER_SRGB_EXT 0x8DB9
#endif

#if !defined(GL_OVR_multiview)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#if !defined(GL_OVR_multiview_multisampled_render_to_texture)
typedef void(GL_APIENTRY* PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)(
    GLenum target,
    GLenum attachment,
    GLuint texture,
    GLint level,
    GLsizei samples,
    GLint baseViewIndex,
    GLsizei numViews);
#endif

#define DEBUG 1
#define OVR_LOG_TAG "XrPassthroughOcclusionGl"

#if defined(ANDROID)
#define ALOGE(...) __android_log_print(ANDROID_LOG_ERROR, OVR_LOG_TAG, __VA_ARGS__)
#if DEBUG
#define ALOGV(...) __android_log_print(ANDROID_LOG_VERBOSE, OVR_LOG_TAG, __VA_ARGS__)
#else
#define ALOGV(...)
#endif
#else
#define ALOGE(...)       \
    printf("ERROR: ");   \
    printf(__VA_ARGS__); \
    printf("\n")
#define ALOGV(...)       \
    printf("VERBOSE: "); \
    printf(__VA_ARGS__); \
    printf("\n")
#endif

/*
================================================================================

OpenGL-ES Utility Functions

================================================================================
*/

namespace {
struct OpenGLExtensions_t {
    bool multi_view; // GL_OVR_multiview, GL_OVR_multiview2
    bool EXT_texture_border_clamp; // GL_EXT_texture_border_clamp, GL_OES_texture_border_clamp
    bool EXT_sRGB_write_control;
};

OpenGLExtensions_t glExtensions;

void EglInitExtensions() {
    glExtensions = {};
    const char* allExtensions = (const char*)glGetString(GL_EXTENSIONS);
    if (allExtensions != nullptr) {
        glExtensions.multi_view = strstr(allExtensions, "GL_OVR_multiview2") &&
            strstr(allExtensions, "GL_OVR_multiview_multisampled_render_to_texture");

        glExtensions.EXT_texture_border_clamp =
            strstr(allExtensions, "GL_EXT_texture_border_clamp") ||
            strstr(allExtensions, "GL_OES_texture_border_clamp");
        glExtensions.EXT_sRGB_write_control = strstr(allExtensions, "GL_EXT_sRGB_write_control");
    }
}

const char* GlFrameBufferStatusString(GLenum status) {
    switch (status) {
        case GL_FRAMEBUFFER_UNDEFINED:
            return "GL_FRAMEBUFFER_UNDEFINED";
        case GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_ATTACHMENT";
        case GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT:
            return "GL_FRAMEBUFFER_INCOMPLETE_MISSING_ATTACHMENT";
        case GL_FRAMEBUFFER_UNSUPPORTED:
            return "GL_FRAMEBUFFER_UNSUPPORTED";
        case GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE:
            return "GL_FRAMEBUFFER_INCOMPLETE_MULTISAMPLE";
        default:
            return "unknown";
    }
}

#define CHECK_GL_ERRORS
#ifdef CHECK_GL_ERRORS

const char* GlErrorString(GLenum error) {
    switch (error) {
        case GL_NO_ERROR:
            return "GL_NO_ERROR";
        case GL_INVALID_ENUM:
            return "GL_INVALID_ENUM";
        case GL_INVALID_VALUE:
            return "GL_INVALID_VALUE";
        case GL_INVALID_OPERATION:
            return "GL_INVALID_OPERATION";
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            return "GL_INVALID_FRAMEBUFFER_OPERATION";
        case GL_OUT_OF_MEMORY:
            return "GL_OUT_OF_MEMORY";
        default:
            return "unknown";
    }
}

void GLCheckErrors(int line) {
    for (int i = 0; i < 10; i++) {
        const GLenum error = glGetError();
        if (error == GL_NO_ERROR) {
            break;
        }
        ALOGE("GL error on line %d: %s", line, GlErrorString(error));
    }
}

#define GL(func) \
    func;        \
    GLCheckErrors(__LINE__);

#else // CHECK_GL_ERRORS

#define GL(func) func;

#endif // CHECK_GL_ERRORS

} // namespace

/*
================================================================================

Geometry

================================================================================
*/

enum VertexAttributeLocation {
    VERTEX_ATTRIBUTE_LOCATION_POSITION,
    VERTEX_ATTRIBUTE_LOCATION_COLOR,
    VERTEX_ATTRIBUTE_LOCATION_UV,
    VERTEX_ATTRIBUTE_LOCATION_TRANSFORM
};

struct VertexAttribute {
    enum VertexAttributeLocation location;
    const char* name;
};

static VertexAttribute ProgramVertexAttributes[] = {
    {VERTEX_ATTRIBUTE_LOCATION_POSITION, "vertexPosition"},
    {VERTEX_ATTRIBUTE_LOCATION_COLOR, "vertexColor"},
    {VERTEX_ATTRIBUTE_LOCATION_UV, "vertexUv"},
    {VERTEX_ATTRIBUTE_LOCATION_TRANSFORM, "vertexTransform"}};

void Geometry::CreateBox() {
    struct CubeVertices {
        float positions[8][4];
        unsigned char colors[8][4];
    };

    const CubeVertices cubeVertices = {
        // positions
        {{-1.0f, -1.0f, -1.0f, 1.0f},
         {1.0f, -1.0f, -1.0f, 1.0f},
         {-1.0f, 1.0f, -1.0f, 1.0f},
         {1.0f, 1.0f, -1.0f, 1.0f},

         {-1.0f, -1.0f, 1.0f, 1.0f},
         {1.0f, -1.0f, 1.0f, 1.0f},
         {-1.0f, 1.0f, 1.0f, 1.0f},
         {1.0f, 1.0f, 1.0f, 1.0f}},
        // colors
        {
            {255, 0, 0, 255},
            {250, 255, 0, 255},
            {250, 0, 255, 255},
            {255, 255, 0, 255},
            {255, 0, 0, 255},
            {250, 255, 0, 255},
            {250, 0, 255, 255},
            {255, 255, 0, 255},
        },
    };

    //     6------7
    //    /|     /|
    //   2-+----3 |
    //   | |    | |
    //   | 4----+-5
    //   |/     |/
    //   0------1

    const unsigned short cubeIndices[36] = {0, 1, 3, 0, 3, 2,

                                            5, 4, 6, 5, 6, 7,

                                            4, 0, 2, 4, 2, 6,

                                            1, 5, 7, 1, 7, 3,

                                            4, 5, 1, 4, 1, 0,

                                            2, 3, 7, 2, 7, 6};

    VertexCount = 8;
    IndexCount = 36;

    VertexAttribs.resize(2);

    VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    VertexAttribs[0].Size = 4;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(cubeVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid*)offsetof(CubeVertices, positions);

    VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(cubeVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid*)offsetof(CubeVertices, colors);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(cubeVertices), &cubeVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(cubeIndices), cubeIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void Geometry::CreateAxes() {
    struct AxesVertices {
        float positions[6][3];
        unsigned char colors[6][4];
    };

    static const AxesVertices axesVertices = {
        // positions
        {{0, 0, 0}, {1, 0, 0}, {0, 0, 0}, {0, 1, 0}, {0, 0, 0}, {0, 0, 1}},
        // colors
        {{255, 0, 0, 255},
         {255, 0, 0, 255},
         {0, 255, 0, 255},
         {0, 255, 0, 255},
         {0, 0, 255, 255},
         {0, 0, 255, 255}},
    };

    static const unsigned short axesIndices[6] = {
        0,
        1, // x axis - red
        2,
        3, // y axis - green
        4,
        5 // z axis - blue
    };

    VertexCount = 6;
    IndexCount = 6;

    VertexAttribs.resize(2);

    VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    VertexAttribs[0].Size = 3;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = sizeof(axesVertices.positions[0]);
    VertexAttribs[0].Pointer = (const GLvoid*)offsetof(AxesVertices, positions);

    VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_COLOR;
    VertexAttribs[1].Size = 4;
    VertexAttribs[1].Type = GL_UNSIGNED_BYTE;
    VertexAttribs[1].Normalized = true;
    VertexAttribs[1].Stride = sizeof(axesVertices.colors[0]);
    VertexAttribs[1].Pointer = (const GLvoid*)offsetof(AxesVertices, colors);

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(axesVertices), &axesVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(axesIndices), axesIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void Geometry::CreatePlane() {
    struct MappedVertices {
        float Positions[8];
        float Uvs[8];
    };
    const MappedVertices mappedVertices = {
        {-1.0f, -1.0f, 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, 1.0f},
        {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f}};
    static const unsigned short kPlaneIndices[6] = {0, 1, 2, 2, 1, 3};

    VertexCount = 4;
    IndexCount = 6;

    VertexAttribs.resize(2);

    VertexAttribs[0].Index = VERTEX_ATTRIBUTE_LOCATION_POSITION;
    VertexAttribs[0].Size = 2;
    VertexAttribs[0].Type = GL_FLOAT;
    VertexAttribs[0].Normalized = false;
    VertexAttribs[0].Stride = 2 * sizeof(float);
    VertexAttribs[0].Pointer = reinterpret_cast<const GLvoid*>(offsetof(MappedVertices, Positions));

    VertexAttribs[1].Index = VERTEX_ATTRIBUTE_LOCATION_UV;
    VertexAttribs[1].Size = 2;
    VertexAttribs[1].Type = GL_FLOAT;
    VertexAttribs[1].Normalized = false;
    VertexAttribs[1].Stride = 2 * sizeof(float);
    VertexAttribs[1].Pointer = reinterpret_cast<const GLvoid*>(offsetof(MappedVertices, Uvs));

    GL(glGenBuffers(1, &VertexBuffer));
    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));
    GL(glBufferData(GL_ARRAY_BUFFER, sizeof(mappedVertices), &mappedVertices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ARRAY_BUFFER, 0));

    GL(glGenBuffers(1, &IndexBuffer));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(kPlaneIndices), kPlaneIndices, GL_STATIC_DRAW));
    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0));

    CreateVAO();
}

void Geometry::Destroy() {
    GL(glDeleteBuffers(1, &IndexBuffer));
    GL(glDeleteBuffers(1, &VertexBuffer));

    GL(glDeleteVertexArrays(1, &VertexArrayObject));

    VertexBuffer = 0;
    IndexBuffer = 0;
    VertexArrayObject = 0;
    VertexCount = 0;
    IndexCount = 0;

    VertexAttribs.clear();
}

void Geometry::CreateVAO() {
    GL(glGenVertexArrays(1, &VertexArrayObject));
    GL(glBindVertexArray(VertexArrayObject));

    GL(glBindBuffer(GL_ARRAY_BUFFER, VertexBuffer));

    for (const auto& vertexAttrib : VertexAttribs) {
        GL(glEnableVertexAttribArray(vertexAttrib.Index));
        GL(glVertexAttribPointer(
            vertexAttrib.Index,
            vertexAttrib.Size,
            vertexAttrib.Type,
            vertexAttrib.Normalized,
            vertexAttrib.Stride,
            vertexAttrib.Pointer));
    }

    GL(glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, IndexBuffer));
    GL(glBindVertexArray(0));
}

/*
================================================================================

Program

================================================================================
*/

struct Uniform {
    enum Index {
        MODEL_MATRIX,
        SCENE_MATRICES,
        DEPTH_VIEW_MATRICES,
        DEPTH_PROJECTION_MATRICES,
    };
    enum Type {
        UNIFORM,
        BUFFER,
    };

    Index index;
    Type type;
    const char* name;
};

static Uniform ProgramUniforms[] = {
    {Uniform::Index::MODEL_MATRIX, Uniform::Type::UNIFORM, "ModelMatrix"},
    {Uniform::Index::SCENE_MATRICES, Uniform::Type::BUFFER, "SceneMatrices"},
    {Uniform::Index::DEPTH_VIEW_MATRICES, Uniform::Type::UNIFORM, "DepthViewMatrix"},
    {Uniform::Index::DEPTH_PROJECTION_MATRICES, Uniform::Type::UNIFORM, "DepthProjectionMatrix"},
};

static const char* programVersion = "#version 300 es\n";

bool Program::Create(const char* vertexSource, const char* fragmentSource) {
    GLint r;

    GL(VertexShader = glCreateShader(GL_VERTEX_SHADER));

    const char* vertexSources[3] = {programVersion, "", vertexSource};
    GL(glShaderSource(VertexShader, 3, vertexSources, 0));
    GL(glCompileShader(VertexShader));
    GL(glGetShaderiv(VertexShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(VertexShader, sizeof(msg), 0, msg));
        ALOGE("%s\n", vertexSource);
        ALOGE("ERROR: %s\n", msg);
        return false;
    }

    const char* fragmentSources[2] = {programVersion, fragmentSource};
    GL(FragmentShader = glCreateShader(GL_FRAGMENT_SHADER));
    GL(glShaderSource(FragmentShader, 2, fragmentSources, 0));
    GL(glCompileShader(FragmentShader));
    GL(glGetShaderiv(FragmentShader, GL_COMPILE_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetShaderInfoLog(FragmentShader, sizeof(msg), 0, msg));
        ALOGE("%s\n", fragmentSource);
        ALOGE("ERROR: %s\n", msg);
        return false;
    }

    GL(Program_ = glCreateProgram());
    GL(glAttachShader(Program_, VertexShader));
    GL(glAttachShader(Program_, FragmentShader));

    // Bind the vertex attribute locations.
    for (size_t i = 0; i < sizeof(ProgramVertexAttributes) / sizeof(ProgramVertexAttributes[0]);
         i++) {
        GL(glBindAttribLocation(
            Program_, ProgramVertexAttributes[i].location, ProgramVertexAttributes[i].name));
    }

    GL(glLinkProgram(Program_));
    GL(glGetProgramiv(Program_, GL_LINK_STATUS, &r));
    if (r == GL_FALSE) {
        GLchar msg[4096];
        GL(glGetProgramInfoLog(Program_, sizeof(msg), 0, msg));
        ALOGE("Linking program failed: %s\n", msg);
        return false;
    }

    int numBufferBindings = 0;

    UniformLocation.clear();
    UniformBinding.clear();

    for (size_t i = 0; i < sizeof(ProgramUniforms) / sizeof(ProgramUniforms[0]); i++) {
        const int uniformIndex = ProgramUniforms[i].index;
        if (ProgramUniforms[i].type == Uniform::Type::BUFFER) {
            const GLint blockIndex = glGetUniformBlockIndex(Program_, ProgramUniforms[i].name);
            if (blockIndex >= 0) {
                UniformLocation[uniformIndex] = blockIndex;
                UniformBinding[uniformIndex] = numBufferBindings++;
                GL(glUniformBlockBinding(
                    Program_, UniformLocation[uniformIndex], UniformBinding[uniformIndex]));
            }
        } else {
            const GLint uniformLocation = glGetUniformLocation(Program_, ProgramUniforms[i].name);
            if (uniformLocation >= 0) {
                UniformLocation[uniformIndex] = uniformLocation;
                UniformBinding[uniformIndex] = uniformLocation;
            }
        }
    }

    GL(glUseProgram(Program_));

    // Get the texture locations.
    constexpr int kMaxTextures = 8;
    for (int i = 0; i < kMaxTextures; i++) {
        char name[32];
        sprintf(name, "Texture%i", i);
        const GLint location = glGetUniformLocation(Program_, name);
        if (location != -1) {
            Textures[i] = location;
            GL(glUniform1i(location, i));
        }
    }

    GL(glUseProgram(0));

    return true;
}

void Program::Destroy() {
    if (Program_ != 0) {
        GL(glDeleteProgram(Program_));
        Program_ = 0;
    }
    if (VertexShader != 0) {
        GL(glDeleteShader(VertexShader));
        VertexShader = 0;
    }
    if (FragmentShader != 0) {
        GL(glDeleteShader(FragmentShader));
        FragmentShader = 0;
    }

    UniformLocation.clear();
    UniformBinding.clear();
    Textures.clear();
}

int Program::GetUniformLocationOrDie(int uniformId) const {
    const auto it = UniformLocation.find(uniformId);
    if (it == UniformLocation.end()) {
        ALOGE("Could not find uniform location %d", uniformId);
        std::abort();
    }
    return it->second;
}

int Program::GetUniformBindingOrDie(int uniformId) const {
    const auto it = UniformBinding.find(uniformId);
    if (it == UniformBinding.end()) {
        ALOGE("Could not find uniform binding %d", uniformId);
        std::abort();
    }
    return it->second;
}

static const char SIX_DOF_VERTEX_SHADER[] = R"(
  #define NUM_VIEWS 2
  #define VIEW_ID gl_ViewID_OVR
  #extension GL_OVR_multiview2 : require
  layout(num_views=NUM_VIEWS) in;
  in vec3 vertexPosition;
  in vec4 vertexColor;
  uniform mat4 ModelMatrix;
  uniform SceneMatrices
  {
    uniform mat4 ViewMatrix[NUM_VIEWS];
    uniform mat4 ProjectionMatrix[NUM_VIEWS];
  } sm;
  out vec4 fragmentColor;
  out vec4 cubeWorldPosition;
  void main() {
    cubeWorldPosition = ModelMatrix * vec4(vertexPosition, 1.0f);
  	gl_Position = sm.ProjectionMatrix[VIEW_ID] * sm.ViewMatrix[VIEW_ID] * cubeWorldPosition;
  	fragmentColor = vertexColor;
  }
)";

static const char SIX_DOF_FRAGMENT_SHADER[] = R"(
  #define NUM_VIEWS 2
  #define VIEW_ID gl_ViewID_OVR
  #extension GL_OVR_multiview2 : require
  #extension GL_ARB_shading_language_420pack : enable
  in lowp vec4 fragmentColor;
  in lowp vec4 cubeWorldPosition;
  uniform highp mat4 DepthViewMatrix[NUM_VIEWS];
  uniform highp mat4 DepthProjectionMatrix[NUM_VIEWS];
  layout(binding = 0) uniform highp sampler2DArray EnvironmentDepthTexture;
  out lowp vec4 outColor;
  void main() {
    // Transform from world space to depth camera space using 6-DOF matrix
    highp vec4 cubeDepthCameraPosition = DepthProjectionMatrix[VIEW_ID] * DepthViewMatrix[VIEW_ID] * cubeWorldPosition;

    // 3D point --> Homogeneous Coordinates --> Normalized Coordinates in [0,1]
    highp vec2 cubeDepthCameraPositionHC = cubeDepthCameraPosition.xy / cubeDepthCameraPosition.w;
    cubeDepthCameraPositionHC = cubeDepthCameraPositionHC * 0.5f + 0.5f;

    // Sample from Environment Depth API texture
    highp vec3 depthViewCoord = vec3(cubeDepthCameraPositionHC, VIEW_ID);
    highp float depthViewEyeZ = texture(EnvironmentDepthTexture, depthViewCoord).r;

    // Get virtual object depth
    highp float cubeDepth = cubeDepthCameraPosition.z / cubeDepthCameraPosition.w;
    cubeDepth = cubeDepth * 0.5f + 0.5f;

    // Test virtual object depth with environment depth.
    // If the virtual object is further away (occluded) output a transparent color so real scene content from PT layer is displayed.
    outColor = fragmentColor;
    if (cubeDepth < depthViewEyeZ) {
      outColor.a = 1.0f; // fully opaque
    }
    else {
      outColor = vec4(0.0f, 0.0f, 0.0f, 0.0f); // invisible
    }

    gl_FragDepth = cubeDepth;
  }
)";

/*
================================================================================

Framebuffer

================================================================================
*/

static void* GlGetExtensionProc(const char* functionName) {
#if defined(ANDROID)
    return (void*)eglGetProcAddress(functionName);
#elif defined(WIN32)
    return (void*)wglGetProcAddress(functionName);
#else
    static_assert(false);
#endif
}

bool Framebuffer::Create(
    const GLenum colorFormat,
    const int width,
    const int height,
    const int multisamples,
    const int swapChainLength,
    GLuint* colorTextures) {
    PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC glFramebufferTextureMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTIVIEWOVRPROC)GlGetExtensionProc(
            "glFramebufferTextureMultiviewOVR");
    PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC glFramebufferTextureMultisampleMultiviewOVR =
        (PFNGLFRAMEBUFFERTEXTUREMULTISAMPLEMULTIVIEWOVRPROC)GlGetExtensionProc(
            "glFramebufferTextureMultisampleMultiviewOVR");

    Width = width;
    Height = height;
    Multisamples = multisamples;

    Elements.clear();
    Elements.resize(swapChainLength);

    for (int i = 0; i < swapChainLength; i++) {
        Element& el = Elements[i];
        // Create the color buffer texture.
        el.ColorTexture = colorTextures[i];
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, el.ColorTexture));
        GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER));
        GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER));
        GLfloat borderColor[] = {0.0f, 0.0f, 0.0f, 0.0f};
        GL(glTexParameterfv(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BORDER_COLOR, borderColor));
        GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR));
        GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR));
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

        // Create the depth buffer texture.
        GL(glGenTextures(1, &el.DepthTexture));
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, el.DepthTexture));
        GL(glTexStorage3D(GL_TEXTURE_2D_ARRAY, 1, GL_DEPTH_COMPONENT24, width, height, 2));
        GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));

        // Create the frame buffer.
        GL(glGenFramebuffers(1, &el.FrameBufferObject));
        GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, el.FrameBufferObject));
        if (multisamples > 1 && (glFramebufferTextureMultisampleMultiviewOVR != nullptr)) {
            GL(glFramebufferTextureMultisampleMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                el.DepthTexture,
                0 /* level */,
                multisamples /* samples */,
                0 /* baseViewIndex */,
                2 /* numViews */));
            GL(glFramebufferTextureMultisampleMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                el.ColorTexture,
                0 /* level */,
                multisamples /* samples */,
                0 /* baseViewIndex */,
                2 /* numViews */));
        } else if (glFramebufferTextureMultiviewOVR != nullptr) {
            GL(glFramebufferTextureMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_DEPTH_ATTACHMENT,
                el.DepthTexture,
                0 /* level */,
                0 /* baseViewIndex */,
                2 /* numViews */));
            GL(glFramebufferTextureMultiviewOVR(
                GL_DRAW_FRAMEBUFFER,
                GL_COLOR_ATTACHMENT0,
                el.ColorTexture,
                0 /* level */,
                0 /* baseViewIndex */,
                2 /* numViews */));
        }

        GL(GLenum renderFramebufferStatus = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER));
        GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
        if (renderFramebufferStatus != GL_FRAMEBUFFER_COMPLETE) {
            ALOGE(
                "Incomplete frame buffer object: %s",
                GlFrameBufferStatusString(renderFramebufferStatus));
            Elements.clear();
            return false;
        }
    }
    return true;
}

void Framebuffer::Destroy() {
    for (size_t i = 0; i < Elements.size(); i++) {
        Element& el = Elements[i];
        GL(glDeleteFramebuffers(1, &el.FrameBufferObject));
        GL(glDeleteTextures(1, &el.DepthTexture));
    }
    Elements.clear();
    Width = 0;
    Height = 0;
    Multisamples = 0;
}

void Framebuffer::Bind(int element) {
    // No overflow as soon Elements gets resized by swapChainLength parameter in Create().
    if (element < 0 || element >= static_cast<int>(Elements.size())) {
        ALOGE("Framebuffer index out of bounds.");
        std::abort();
    }
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, Elements[element].FrameBufferObject));
}

void Framebuffer::Unbind() {
    GL(glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0));
}

void Framebuffer::Resolve() {
    // Discard the depth buffer, so the tiler won't need to write it back out to memory.
    const GLenum depthAttachment[1] = {GL_DEPTH_ATTACHMENT};
    glInvalidateFramebuffer(GL_DRAW_FRAMEBUFFER, 1, depthAttachment);

    // We now let the resolve happen implicitly.
}

/*
================================================================================

Scene

================================================================================
*/

bool Scene::IsCreated() {
    return CreatedScene;
}

void Scene::Create() {
    // Setup the scene matrices.
    GL(glGenBuffers(1, &SceneMatrices));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, SceneMatrices));
    GL(glBufferData(
        GL_UNIFORM_BUFFER,
        2 * sizeof(Matrix4f) /* 2 view matrices */ +
            2 * sizeof(Matrix4f) /* 2 projection matrices */,
        nullptr,
        GL_STATIC_DRAW));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    if (!BoxDepthSpaceOcclusionProgram.Create(SIX_DOF_VERTEX_SHADER, SIX_DOF_FRAGMENT_SHADER)) {
        ALOGE("Failed to compile depth space occlusion box program");
    }
    Box.CreateBox();

    CreatedScene = true;
}

void Scene::Destroy() {
    GL(glDeleteBuffers(1, &SceneMatrices));
    BoxDepthSpaceOcclusionProgram.Destroy();
    Box.Destroy();
    CreatedScene = false;
}

/*
================================================================================

AppRenderer

================================================================================
*/

void AppRenderer::Create(
    GLenum format,
    int width,
    int height,
    int numMultiSamples,
    int swapChainLength,
    GLuint* colorTextures) {
    EglInitExtensions();
    if (!framebuffer.Create(
            format, width, height, numMultiSamples, swapChainLength, colorTextures)) {
        ALOGE("Failed to create framebuffer");
        std::abort();
    }
    if (glExtensions.EXT_sRGB_write_control) {
        // This app was originally written with the presumption that
        // its swapchains and compositor front buffer were RGB.
        // In order to have the colors the same now that its compositing
        // to an sRGB front buffer, we have to write to an sRGB swapchain
        // but with the linear->sRGB conversion disabled on write.
        GL(glDisable(GL_FRAMEBUFFER_SRGB_EXT));
    }
    IsCreated = true;
}

void AppRenderer::Destroy() {
    framebuffer.Destroy();
    scene.Destroy();
    IsCreated = false;
}

void AppRenderer::RenderFrame(const FrameIn& frameIn) {
    if (!IsCreated) {
        std::abort();
    }

    // Update the scene matrices.
    GL(glBindBuffer(GL_UNIFORM_BUFFER, scene.SceneMatrices));
    GL(Matrix4f* sceneMatrices = (Matrix4f*)glMapBufferRange(
           GL_UNIFORM_BUFFER,
           0,
           4 * sizeof(Matrix4f) /* 2 view + 2 proj matrices */,
           GL_MAP_WRITE_BIT | GL_MAP_INVALIDATE_BUFFER_BIT));

    if (sceneMatrices != nullptr) {
        std::memcpy(reinterpret_cast<char*>(sceneMatrices), &frameIn.View[0], 2 * sizeof(Matrix4f));
        std::memcpy(
            reinterpret_cast<char*>(sceneMatrices) + 2 * sizeof(Matrix4f),
            &frameIn.Proj[0],
            2 * sizeof(Matrix4f));
    }

    GL(glUnmapBuffer(GL_UNIFORM_BUFFER));
    GL(glBindBuffer(GL_UNIFORM_BUFFER, 0));

    // Render the eye images.
    framebuffer.Bind(frameIn.SwapChainIndex);

    GL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_SCISSOR_TEST));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_BLEND));

    GL(glViewport(0, 0, framebuffer.GetWidth(), framebuffer.GetHeight()));
    GL(glScissor(0, 0, framebuffer.GetWidth(), framebuffer.GetHeight()));

    GL(glClearColor(0.0, 0.0, 0.0, 0.0));
    GL(glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT));

    RenderScene(frameIn);

    framebuffer.Resolve();
    framebuffer.Unbind();
}

void AppRenderer::RenderScene(const FrameIn& frameIn) {
    GL(glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE));
    GL(glDepthMask(GL_TRUE));
    GL(glEnable(GL_DEPTH_TEST));
    GL(glDepthFunc(GL_LEQUAL));
    GL(glDisable(GL_CULL_FACE));
    GL(glDisable(GL_BLEND));
    GL(glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA));

    // Controllers
    GL(glUseProgram(scene.BoxDepthSpaceOcclusionProgram.GetProgramId()));
    GL(glBindVertexArray(scene.Box.GetVertexArrayObject()));

    constexpr size_t kDepthMatrixSize = 4 * 4 * sizeof(float);
    float viewDataBlock[4 * 4 * 2];
    float projectionDataBlock[4 * 4 * 2];
    std::memcpy(
        reinterpret_cast<char*>(viewDataBlock),
        &frameIn.DepthViewMatrices[0].M[0][0],
        kDepthMatrixSize);
    std::memcpy(
        reinterpret_cast<char*>(viewDataBlock) + kDepthMatrixSize,
        &frameIn.DepthViewMatrices[1].M[0][0],
        kDepthMatrixSize);
    std::memcpy(
        reinterpret_cast<char*>(projectionDataBlock),
        &frameIn.DepthProjectionMatrices[0].M[0][0],
        kDepthMatrixSize);
    std::memcpy(
        reinterpret_cast<char*>(projectionDataBlock) + kDepthMatrixSize,
        &frameIn.DepthProjectionMatrices[1].M[0][0],
        kDepthMatrixSize);

    GL(glUniformMatrix4fv(
        scene.BoxDepthSpaceOcclusionProgram.GetUniformLocationOrDie(
            Uniform::Index::DEPTH_VIEW_MATRICES),
        2,
        GL_FALSE,
        viewDataBlock));
    GL(glUniformMatrix4fv(
        scene.BoxDepthSpaceOcclusionProgram.GetUniformLocationOrDie(
            Uniform::Index::DEPTH_PROJECTION_MATRICES),
        2,
        GL_FALSE,
        projectionDataBlock));

    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, frameIn.DepthTexture));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST));
    GL(glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST));

    GL(glBindBufferBase(
        GL_UNIFORM_BUFFER,
        scene.BoxDepthSpaceOcclusionProgram.GetUniformBindingOrDie(Uniform::Index::SCENE_MATRICES),
        scene.SceneMatrices));
    for (const auto& trackedController : scene.TrackedControllers) {
        const Matrix4f pose(trackedController.Pose);
        const Matrix4f offset = Matrix4f::Translation(0, 0, -0.25);
        const Matrix4f scale = Matrix4f::Scaling(0.1, 0.1, 0.1);
        const Matrix4f model = pose * offset * scale;
        glUniformMatrix4fv(
            scene.BoxDepthSpaceOcclusionProgram.GetUniformLocationOrDie(
                Uniform::Index::MODEL_MATRIX),
            1,
            GL_TRUE,
            &model.M[0][0]);
        GL(glDrawElements(GL_TRIANGLES, scene.Box.GetIndexCount(), GL_UNSIGNED_SHORT, NULL));
    }

    GL(glBindVertexArray(0));
    GL(glBindTexture(GL_TEXTURE_2D_ARRAY, 0));
    GL(glUseProgram(0));
}
