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
#pragma once

#include <array>
#include <vector>

#if defined(ANDROID)
#include <GLES3/gl3.h>
#include <EGL/egl.h>
#include <jni.h>

#define XR_USE_GRAPHICS_API_OPENGL_ES 1
#define XR_USE_PLATFORM_ANDROID 1
#elif defined(WIN32)
#include "Render/GlWrapperWin32.h"

#include <unknwn.h>
#define XR_USE_GRAPHICS_API_OPENGL 1
#define XR_USE_PLATFORM_WIN32 1
#endif

#include <openxr/openxr.h>
// #include <meta_openxr_preview/openxr_oculus_helpers.h>
#include <openxr/openxr_platform.h>


#include "OVR_Math.h"

#define NUM_EYES 2

struct ovrGeometry {
    void Clear();
    void CreateAxes();
    void CreateStage();
    void CreatePlane(const std::vector<XrVector3f>& vertices, const XrColor4f& color);
    void CreateVolume(const std::array<XrVector3f, 8>& vertices, const XrColor4f& color);
    void CreateMesh(const XrSpaceTriangleMeshMETA& mesh);
    void Destroy();
    void CreateVAO();
    void DestroyVAO();

    int IndexCount() const {
        return IndexCount_;
    }

    void BindVAO() const;

    bool IsRenderable() const {
        return IsRenderable_;
    }

   private:
    static constexpr int MAX_VERTEX_ATTRIB_POINTERS = 3;

    struct VertexAttribPointer {
        GLint Index;
        GLint Size;
        GLenum Type;
        GLboolean Normalized;
        GLsizei Stride;
        const GLvoid* Pointer;
    };

    void CreateIndexBuffer(const std::vector<unsigned short>& indices);

    int IndexCount_;

    VertexAttribPointer VertexAttribs_[MAX_VERTEX_ATTRIB_POINTERS];
    GLuint VertexBuffer_ = 0;
    GLuint IndexBuffer_ = 0;
    GLuint VertexArrayObject_ = 0;

    bool IsRenderable_ = false;
};

struct ovrProgram {
    static constexpr int MAX_PROGRAM_UNIFORMS = 8;
    static constexpr int MAX_PROGRAM_TEXTURES = 8;

    void Clear();
    bool Create(const char* vertexSource, const char* fragmentSource);
    void Destroy();
    GLuint Program;
    GLuint VertexShader;
    GLuint FragmentShader;
    // These will be -1 if not used by the program.
    GLint UniformLocation[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint UniformBinding[MAX_PROGRAM_UNIFORMS]; // ProgramUniforms[].name
    GLint Textures[MAX_PROGRAM_TEXTURES]; // Texture%i
};

struct ovrFramebuffer {
    void Clear();
    bool Create(
        const GLenum colorFormat,
        const int width,
        const int height,
        const int multisamples,
        const int swapChainLength,
        GLuint* colorTextures);
    void Destroy();
    void Bind(int element);
    void Unbind();
    void Resolve();
    int Width;
    int Height;
    int Multisamples;
    int SwapChainLength;
    struct Element {
        GLuint ColorTexture;
        GLuint DepthTexture;
        GLuint FrameBufferObject;
    };
    Element* Elements;
};

struct ovrPlane {
    explicit ovrPlane(const XrSpace space);

    void Update(const XrRect2Df& boundingBox2D, const XrColor4f& color);

    void Update(const XrBoundary2DFB& boundary2D, const XrColor4f& color);

    void SetPose(const XrPosef& T_World_Plane);

    void SetZOffset(const float zOffset);

    void SetVisible(const bool isVisible) {
        IsVisible_ = isVisible;
    }

    bool IsRenderable() const {
        return IsVisible_ && IsPoseSet_ && Geometry.IsRenderable();
    }

    XrSpace Space;
    OVR::Posef T_World_Plane;
    ovrGeometry Geometry;
    float ZOffset = 0.0f; // Z offset in the plane frame

   private:
    bool IsVisible_ = true;
    bool IsPoseSet_ = false;
};

struct ovrVolume {
    explicit ovrVolume(const XrSpace space);

    void Update(const XrRect3DfFB& boundingBox3D, const XrColor4f& color);

    void SetPose(const XrPosef& T_World_Volume);

    void SetVisible(const bool isVisible) {
        IsVisible_ = isVisible;
    }

    bool IsRenderable() const {
        return IsVisible_ && IsPoseSet_ && Geometry.IsRenderable();
    }

    XrSpace Space;
    OVR::Posef T_World_Volume;
    ovrGeometry Geometry;

   private:
    bool IsVisible_ = true;
    bool IsPoseSet_ = false;
};

struct ovrMesh {
    explicit ovrMesh(const XrSpace space);

    void Update(const XrSpaceTriangleMeshMETA& mesh);

    void SetPose(const XrPosef& T_World_Mesh);

    void SetVisible(const bool isVisible) {
        IsVisible_ = isVisible;
    }

    bool IsRenderable() const {
        return IsVisible_ && IsPoseSet_ && Geometry.IsRenderable();
    }

    XrSpace Space;
    OVR::Posef T_World_Mesh;
    ovrGeometry Geometry;

   private:
    bool IsVisible_ = true;
    bool IsPoseSet_ = false;
};


struct ovrScene {
   public:
    void Clear();
    void Create();
    void Destroy();
    bool IsCreated();
    void SetClearColor(const float* c);
    void CreateVAOs();
    void DestroyVAOs();

    void DestroyPlaneGeometries();
    void ClearPlaneGeometries();
    void DestroyVolumeGeometries();
    void ClearVolumeGeometries();

    void AddPlaneToRender();

    void UpdatePlaneToRender(
        const int planeIndex,
        const std::array<XrVector3f, 4>& vertices,
        const XrColor4f& color);

    void AddVolumeToRender();

    void UpdateVolumeToRender(
        const int volumeIndex,
        const std::array<XrVector3f, 8>& vertices,
        const XrColor4f& color);

    bool CreatedScene;
    bool CreatedVAOs;
    GLuint SceneMatrices;
    ovrProgram StageProgram;
    ovrGeometry Stage;
    ovrProgram AxesProgram;
    ovrGeometry Axes;
    ovrProgram PlaneProgram;
    ovrProgram VolumeProgram;
    ovrProgram MeshProgram;
    float ClearColor[4];

    std::vector<ovrPlane> Planes;
    std::vector<ovrVolume> Volumes;
    std::vector<ovrMesh> Meshes;
};

struct ovrAppRenderer {
    void Clear();
    void Create(
        GLenum format,
        int width,
        int height,
        int numMultiSamples,
        int swapChainLength,
        GLuint* colorTextures);
    void Destroy();

    struct FrameIn {
        int SwapChainIndex;
        OVR::Matrix4f View[NUM_EYES];
        OVR::Matrix4f Proj[NUM_EYES];
        bool HasStage;
        OVR::Posef StagePose;
        OVR::Vector3f StageScale;
        bool HasStationary;
        OVR::Posef StationaryPose;
        std::array<bool, 2> RenderController;
        std::array<OVR::Posef, 2> ControllerPoses;
    };

    void RenderFrame(const FrameIn& frameIn);

    ovrFramebuffer Framebuffer;
    ovrScene Scene;
};
