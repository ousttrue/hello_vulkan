#include "xr_vulkan_session.h"
#include "ViewRenderer.h"
#include <map>
#include <thread>

#include <vuloxr/vk/buffer.h>
#include <vuloxr/vk/command.h>
#include <vuloxr/vk/pipeline.h>
#include <vuloxr/vk/shaderc.h>
#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

char VertexShaderGlsl[] = {
#embed "shader.vert"
    , 0};

char FragmentShaderGlsl[] = {
#embed "shader.frag"
    , 0};

static_assert(sizeof(VertexShaderGlsl), "VertexShaderGlsl");
static_assert(sizeof(FragmentShaderGlsl), "FragmentShaderGlsl");

struct Vec3 {
  float x, y, z;
};

struct Mat4 {
  float m[16];
};

struct Vertex {
  Vec3 Position;
  Vec3 Color;
};

constexpr Vec3 Red{1, 0, 0};
constexpr Vec3 DarkRed{0.25f, 0, 0};
constexpr Vec3 Green{0, 1, 0};
constexpr Vec3 DarkGreen{0, 0.25f, 0};
constexpr Vec3 Blue{0, 0, 1};
constexpr Vec3 DarkBlue{0, 0, 0.25f};

// Vertices for a 1x1x1 meter cube. (Left/Right, Top/Bottom, Front/Back)
constexpr Vec3 LBB{-0.5f, -0.5f, -0.5f};
constexpr Vec3 LBF{-0.5f, -0.5f, 0.5f};
constexpr Vec3 LTB{-0.5f, 0.5f, -0.5f};
constexpr Vec3 LTF{-0.5f, 0.5f, 0.5f};
constexpr Vec3 RBB{0.5f, -0.5f, -0.5f};
constexpr Vec3 RBF{0.5f, -0.5f, 0.5f};
constexpr Vec3 RTB{0.5f, 0.5f, -0.5f};
constexpr Vec3 RTF{0.5f, 0.5f, 0.5f};

#define CUBE_SIDE(V1, V2, V3, V4, V5, V6, COLOR)                               \
  {V1, COLOR}, {V2, COLOR}, {V3, COLOR}, {V4, COLOR}, {V5, COLOR}, {V6, COLOR},

constexpr Vertex c_cubeVertices[] = {
    CUBE_SIDE(LTB, LBF, LBB, LTB, LTF, LBF, DarkRed)   // -X
    CUBE_SIDE(RTB, RBB, RBF, RTB, RBF, RTF, Red)       // +X
    CUBE_SIDE(LBB, LBF, RBF, LBB, RBF, RBB, DarkGreen) // -Y
    CUBE_SIDE(LTB, RTB, RTF, LTB, RTF, LTF, Green)     // +Y
    CUBE_SIDE(LBB, RBB, RTB, LBB, RTB, LTB, DarkBlue)  // -Z
    CUBE_SIDE(LBF, LTF, RTF, LBF, RTF, RBF, Blue)      // +Z
};

// Winding order is clockwise. Each side uses a different color.
constexpr unsigned short c_cubeIndices[] = {
    0,  1,  2,  3,  4,  5,  // -X
    6,  7,  8,  9,  10, 11, // +X
    12, 13, 14, 15, 16, 17, // -Y
    18, 19, 20, 21, 22, 23, // +Y
    24, 25, 26, 27, 28, 29, // -Z
    30, 31, 32, 33, 34, 35, // +Z
};

inline XrPosef RotateCCWAboutYAxis(float radians, XrVector3f translation) {
  XrPosef t{
      .orientation = {0, 0, 0, 1.0f},
      .position = {0, 0, 0},
  };
  t.orientation.x = 0.f;
  t.orientation.y = std::sin(radians * 0.5f);
  t.orientation.z = 0.f;
  t.orientation.w = std::cos(radians * 0.5f);
  t.position = translation;
  return t;
}

struct VisualizedSpaces : vuloxr::NonCopyable {
  std::vector<XrSpace> spaces;

  VisualizedSpaces(XrSession session) {
    // ViewFront
    addSpace(session, XR_REFERENCE_SPACE_TYPE_VIEW,
             {.orientation = {0, 0, 0, 1.0f}, .position = {0, 0, -2.0f}});
    // Local
    addSpace(session, XR_REFERENCE_SPACE_TYPE_LOCAL);
    // Stage
    addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE);
    // StageLeft
    addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
             RotateCCWAboutYAxis(0.f, {-2.f, 0.f, -2.f}));
    // StageRight
    addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
             RotateCCWAboutYAxis(0.f, {2.f, 0.f, -2.f}));
    // StageLeftRotated
    addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
             RotateCCWAboutYAxis(3.14f / 3.f, {-2.f, 0.5f, -2.f}));
    // StageRightRotated
    addSpace(session, XR_REFERENCE_SPACE_TYPE_STAGE,
             RotateCCWAboutYAxis(-3.14f / 3.f, {2.f, 0.5f, -2.f}));
  }

  ~VisualizedSpaces() {
    for (XrSpace visualizedSpace : this->spaces) {
      xrDestroySpace(visualizedSpace);
    }
  }

  void addSpace(XrSession session, XrReferenceSpaceType spaceType,
                const XrPosef &pose = {.orientation = {0, 0, 0, 1.0f},
                                       .position = {0, 0, 0}}) {
    XrReferenceSpaceCreateInfo referenceSpaceCreateInfo{
        .type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO,
        .referenceSpaceType = spaceType,
        .poseInReferenceSpace = pose,
    };
    XrSpace space;
    XrResult res =
        xrCreateReferenceSpace(session, &referenceSpaceCreateInfo, &space);
    if (XR_SUCCEEDED(res)) {
      this->spaces.push_back(space);
    } else {
      vuloxr::Logger::Error("Failed to create reference space");
    }
  }
};

void xr_vulkan_session(const std::function<bool(bool)> &runLoop,
                       XrInstance instance, XrSystemId systemId,
                       XrSession session, XrSpace appSpace,
                       //
                       VkFormat viewFormat, VkPhysicalDevice _physicalDevice,
                       uint32_t queueFamilyIndex, VkDevice device,
                       //
                       VkClearColorValue clearColor,
                       XrEnvironmentBlendMode blendMode,
                       XrViewConfigurationType viewConfigurationType) {
  vuloxr::vk::PhysicalDevice physicalDevice(_physicalDevice);

  static_assert(sizeof(Vertex) == 24, "Unexpected Vertex size");
  auto vertices = vuloxr::vk::VertexBuffer::create(
      device, sizeof(c_cubeVertices), std::size(c_cubeVertices),
      {
          {
              .binding = 0,
              .stride = static_cast<uint32_t>(sizeof(Vertex)),
              .inputRate = VK_VERTEX_INPUT_RATE_VERTEX,
          },
      },
      {
          {
              .location = 0,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, Position),
          },
          {
              .location = 1,
              .binding = 0,
              .format = VK_FORMAT_R32G32B32_SFLOAT,
              .offset = offsetof(Vertex, Color),
          },
      });
  vertices.memory = physicalDevice.allocForMap(device, vertices.buffer);
  vertices.memory.mapWrite(c_cubeVertices, sizeof(c_cubeVertices));

  auto indices = vuloxr::vk::IndexBuffer::create(device, sizeof(c_cubeIndices),
                                                 std::size(c_cubeIndices),
                                                 VK_INDEX_TYPE_UINT16);
  indices.memory = physicalDevice.allocForMap(device, indices.buffer);
  indices.memory.mapWrite(c_cubeIndices, sizeof(c_cubeIndices));

  // Create resources for each view.
  using VulkanSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageVulkan2KHR>;
  std::vector<std::shared_ptr<VulkanSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;

  auto depthFormat = VK_FORMAT_D32_SFLOAT;

  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  VisualizedSpaces spaces(session);
  vuloxr::xr::InputState input(instance, session);

  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);

  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    // XrSwapchain
    auto swapchain = std::make_shared<VulkanSwapchain>(
        session, i, stereoscope.viewConfigurations[i], viewFormat,
        XrSwapchainImageVulkan2KHR{XR_TYPE_SWAPCHAIN_IMAGE_VULKAN2_KHR});
    swapchains.push_back(swapchain);

    // pieline
    auto pipelineLayout = vuloxr::vk::createPipelineLayoutWithConstantSize(
        device, sizeof(float) * 16);
    auto renderPass =
        vuloxr::vk::createColorDepthRenderPass(device, viewFormat, depthFormat);

    auto vertexSPIRV = vuloxr::vk::glsl_vs_to_spv(VertexShaderGlsl);
    assert(vertexSPIRV.size());
    auto vs = vuloxr::vk::ShaderModule::createVertexShader(device, vertexSPIRV,
                                                           "main");

    auto fragmentSPIRV = vuloxr::vk::glsl_fs_to_spv(FragmentShaderGlsl);
    assert(fragmentSPIRV.size());
    auto fs = vuloxr::vk::ShaderModule::createFragmentShader(
        device, fragmentSPIRV, "main");

    std::vector<VkPipelineShaderStageCreateInfo> stages = {
        vs.pipelineShaderStageCreateInfo,
        fs.pipelineShaderStageCreateInfo,
    };

    VkExtent2D extent = {
        swapchain->swapchainCreateInfo.width,
        swapchain->swapchainCreateInfo.height,
    };
    VkRect2D scissor = {{0, 0}, extent};
    // Will invert y after projection
    VkViewport viewport = {
        .x = 0.0f,
        .y = 0.0f,
        .width = static_cast<float>(extent.width),
        .height = static_cast<float>(extent.height),
        .minDepth = 0.0f,
        .maxDepth = 1.0f,
    };

    vuloxr::vk::PipelineBuilder builder;
    auto pipeline = builder.create(device, renderPass, pipelineLayout, stages,
                                   vertices.bindings, vertices.attributes,
                                   {viewport}, {scissor}, {});

    auto ptr = std::make_shared<ViewRenderer>(
        physicalDevice, device, queueFamilyIndex, extent,
        (VkFormat)swapchain->swapchainCreateInfo.format, depthFormat,
        (VkSampleCountFlagBits)swapchain->swapchainCreateInfo.sampleCount,
        std::move(pipeline));
    renderers.push_back(ptr);
  }

  // mainloop
  while (runLoop(state.m_sessionRunning)) {
    state.PollEvents();
    if (state.m_exitRenderLoop) {
      break;
    }

    if (!state.m_sessionRunning) {
      // Throttle loop since xrWaitFrame won't be called.
      std::this_thread::sleep_for(std::chrono::milliseconds(250));
      continue;
    }

    input.PollActions();

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace, blendMode);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {
        // XrCompositionLayerProjection

        // update scene
        std::vector<Cube> cubes;

        // For each locatable space that we want to visualize, render a 25cm
        for (XrSpace visualizedSpace : spaces.spaces) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, visualizedSpace);
          if (vuloxr::xr::locationIsValid(res, location)) {
            cubes.push_back({location.pose, {0.25f, 0.25f, 0.25f}});
          }
        }
        for (auto &hand : input.hands) {
          auto [res, location] = vuloxr::xr::locate(
              appSpace, frameState.predictedDisplayTime, hand.space);
          auto s = hand.scale * 0.1f;
          if (vuloxr::xr::locationIsValid(res, location)) {
            cubes.push_back({location.pose, {s, s, s}});
          }
        }

        for (uint32_t i = 0; i < stereoscope.views.size(); ++i) {
          // XrCompositionLayerProjectionView(left / right)
          auto swapchain = swapchains[i];
          auto [_, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);
          composition.pushView(projectionLayer);

          VkExtent2D extent = {
              swapchain->swapchainCreateInfo.width,
              swapchain->swapchainCreateInfo.height,
          };
          renderers[i]->render(
              image.image, extent,
              (VkFormat)swapchain->swapchainCreateInfo.format, depthFormat,
              clearColor, vertices.buffer, indices.buffer, indices.drawCount,
              projectionLayer.pose, projectionLayer.fov, cubes);

          swapchain->EndSwapchain();
        }
      }
    }

    // std::vector<XrCompositionLayerBaseHeader *>
    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers,
                         blendMode);
  }

  vkDeviceWaitIdle(device);
}
