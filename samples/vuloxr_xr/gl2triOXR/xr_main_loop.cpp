#include "../xr_main_loop.h"

#include "util_shader.h"

#include <vuloxr/xr/session.h>
#include <vuloxr/xr/swapchain.h>

#include <thread>

#ifdef XR_USE_GRAPHICS_API_OPENGL_ES
using GraphicsSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLESKHR>;
const XrSwapchainImageOpenGLESKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_ES_KHR,
};
#endif

#ifdef XR_USE_GRAPHICS_API_OPENGL
using GraphicsSwapchain = vuloxr::xr::Swapchain<XrSwapchainImageOpenGLKHR>;
const XrSwapchainImageOpenGLKHR SwapchainImage{
    .type = XR_TYPE_SWAPCHAIN_IMAGE_OPENGL_KHR,
};
#endif

static XrVector2f s_vtx[] = {
    {-0.5f, 0.5f},  //
    {-0.5f, -0.5f}, //
    {0.5f, 0.5f},   //
};

static XrVector4f s_col[] = {
    {1.0f, 0.0f, 0.0f, 1.0f}, //
    {0.0f, 1.0f, 0.0f, 1.0f}, //
    {0.0f, 0.0f, 1.0f, 1.0f}, //
};

const char s_strVS[]{
#embed "shader.vert"
    ,
    0,
};

const char s_strFS[]{
#embed "shader.frag"
    ,
    0,
};

static const shader_obj_t &getShader() {
  static bool s_initialized = false;
  static shader_obj_t s_sobj;
  if (!s_initialized) {
    generate_shader(&s_sobj, s_strVS, s_strFS);
    s_initialized = true;
    vuloxr::Logger::Info("generate_shader");
  }
  return s_sobj;
}

struct DepthTexture : vuloxr::NonCopyable {
  uint32_t id;
  DepthTexture(int width, int height) {
    glGenRenderbuffers(1, &this->id);
    bind();
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
    unbind();
  }
  void bind() { glBindRenderbuffer(GL_RENDERBUFFER, this->id); }
  void unbind() { glBindRenderbuffer(GL_RENDERBUFFER, 0); }
};

struct FramebufferObject : vuloxr::NonCopyable {
  uint32_t id;
  FramebufferObject() { glGenFramebuffers(1, &this->id); }
  ~FramebufferObject() {}
  void bind() { glBindFramebuffer(GL_FRAMEBUFFER, this->id); }
  void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
  void attach(uint32_t color_id, uint32_t depth_id) {
    bind();
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           color_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, depth_id);
    GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(stat == GL_FRAMEBUFFER_COMPLETE);
    unbind();
  }
};

struct RenderTarget : vuloxr::NonCopyable {
  DepthTexture depth;
  FramebufferObject fbo;

  RenderTarget(uint32_t image_id, int width, int height)
      : depth(width, height) {
    fbo.attach(image_id, this->depth.id);
    vuloxr::Logger::Info("SwapchainImage FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)",
                         this->fbo.id, image_id, this->depth.id, width, height);
  }

  void beginFrame(int width, int height, const XrColor4f &clearColor) {
    this->fbo.bind();
    glViewport(0, 0, width, height);
    glClearColor(clearColor.r, clearColor.g, clearColor.b, clearColor.a);
    glClear(GL_COLOR_BUFFER_BIT);
  }

  void endFrame() { this->fbo.unbind(); }
};

struct Vbo {
  uint32_t id;
  Vbo() { glGenBuffers(1, &this->id); }
  ~Vbo() {}

  void bind() { glBindBuffer(GL_ARRAY_BUFFER, this->id); }
  void unbind() { glBindBuffer(GL_ARRAY_BUFFER, 0); }
  void assign(const void *p, uint32_t size) {
    bind();
    glBufferData(GL_ARRAY_BUFFER, size, p, GL_STATIC_DRAW);
    unbind();
  }
  template <typename T> void assign(std::span<const T> data) {
    assign(data.data(), sizeof(T) * data.size());
  }
};

struct Vao {
  struct AttributeLayout {
    int32_t attribute;
    uint32_t vbo;
    uint32_t components;
    uint32_t stride;
    uint32_t offset;
  };
  uint32_t id;
  Vao() { glGenVertexArrays(1, &this->id); }
  ~Vao() {}
  void bind() { glBindVertexArray(this->id); }
  void unbind() { glBindVertexArray(0); }
  void assign(std::span<const AttributeLayout> layouts) {
    bind();
    for (auto &layout : layouts) {
      glEnableVertexAttribArray(layout.attribute);
      glBindBuffer(GL_ARRAY_BUFFER, layout.vbo);
      glVertexAttribPointer(layout.attribute, layout.components, GL_FLOAT,
                            GL_FALSE, layout.stride,
                            (const void *)(int64_t)layout.offset);
    }
    unbind();
  }
};

struct ViewRenderer {
  std::vector<std::shared_ptr<RenderTarget>> backbuffers;

  ViewRenderer(int width, int height, std::span<const uint32_t> images)
      : backbuffers(images.size()) {
    for (int i = 0; i < images.size(); ++i) {
      this->backbuffers[i] =
          std::make_shared<RenderTarget>(images[i], width, height);
    }
  }

  std::shared_ptr<RenderTarget> operator[](uint32_t index) const {
    return this->backbuffers[index];
  }
};

void xr_main_loop(const std::function<bool(bool)> &runLoop, XrInstance instance,
                  XrSystemId systemId, XrSession session, XrSpace appSpace,
                  std::span<const int64_t> formats,
                  //
                  const Graphics &graphics,
                  //
                  const XrColor4f &clearColor, XrEnvironmentBlendMode blendMode,
                  XrViewConfigurationType viewConfigurationType) {
  auto format = vuloxr::xr::selectColorSwapchainFormat(formats);

  // auto viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
  vuloxr::xr::SessionState state(instance, session, viewConfigurationType);
  vuloxr::xr::Stereoscope stereoscope(instance, systemId,
                                      viewConfigurationType);
  // swapchain
  std::vector<std::shared_ptr<GraphicsSwapchain>> swapchains;
  std::vector<std::shared_ptr<ViewRenderer>> renderers;

  for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
    auto swapchain = std::make_shared<GraphicsSwapchain>(
        session, i, stereoscope.viewConfigurations[i], format, SwapchainImage);
    swapchains.push_back(swapchain);

    auto &viewConf = stereoscope.viewConfigurations[i];
    std::vector<uint32_t> images;
    for (auto &image : swapchain->swapchainImages) {
      images.push_back(image.image);
    }
    auto renderer = std::make_shared<ViewRenderer>(
        swapchain->swapchainCreateInfo.width,
        swapchain->swapchainCreateInfo.height, images);
    renderers.push_back(renderer);
  }

  auto sobj = &getShader();

  Vbo positions;
  positions.assign<XrVector2f>(s_vtx);
  Vbo colors;
  colors.assign<XrVector4f>(s_col);
  Vao vao;
  vao.assign(std::span<const Vao::AttributeLayout>({
      {
          .attribute = sobj->loc_vtx,
          .vbo = positions.id,
          .components = 2,
          .stride = 8,
          .offset = 0,
      },
      {
          .attribute = sobj->loc_clr,
          .vbo = colors.id,
          .components = 4,
          .stride = 16,
          .offset = 0,
      },
  }));

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

    auto frameState = vuloxr::xr::beginFrame(session);
    vuloxr::xr::LayerComposition composition(appSpace);

    if (frameState.shouldRender == XR_TRUE) {
      if (stereoscope.Locate(session, appSpace, frameState.predictedDisplayTime,
                             viewConfigurationType)) {

        for (uint32_t i = 0; i < stereoscope.views.size(); i++) {
          auto swapchain = swapchains[i];
          auto [index, image, projectionLayer] =
              swapchain->AcquireSwapchain(stereoscope.views[i]);

          {
            //  Render
            auto backbuffer = (*renderers[i])[index];
            backbuffer->beginFrame(swapchain->swapchainCreateInfo.width,
                                   swapchain->swapchainCreateInfo.height,
                                   clearColor);

            glUseProgram(sobj->program);

            vao.bind();

            glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

            vao.unbind();
            backbuffer->endFrame();
          }

          composition.pushView(projectionLayer);

          swapchain->EndSwapchain();
        }
      }
    }

    auto &layers = composition.commitLayers();
    vuloxr::xr::endFrame(session, frameState.predictedDisplayTime, layers);

  } // while
}
