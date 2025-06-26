#include "ViewRenderer.h"

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GLES3/gl31.h>

#include <openxr/openxr.h>

#include "util_shader.h"
#include <assert.h>
#include <vuloxr.h>

static float s_vtx[] = {
    -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f,
};

static float s_col[] = {
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
};

auto s_strVS = R"(
attribute    vec4    a_Vertex;
attribute    vec4    a_Color;
varying      vec4    v_color;

void main (void)
{
    gl_Position = a_Vertex;
    v_color     = a_Color;
}
)";

auto s_strFS = R"(
precision mediump float;
varying     vec4     v_color;

void main (void)
{
    gl_FragColor = v_color;
}
)";

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

ViewRenderer::ViewRenderer() {}
ViewRenderer::~ViewRenderer() {}
void ViewRenderer::render(uint32_t texc_id, uint32_t width, uint32_t height) {

  //  framebuffer
  auto found = m_renderTargetMap.find(texc_id);
  if (found == m_renderTargetMap.end()) {
    // Depth Buffer
    render_target rt;
    glGenRenderbuffers(1, &rt.depth_texture);
    glBindRenderbuffer(GL_RENDERBUFFER, rt.depth_texture);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    // FBO
    glGenFramebuffers(1, &rt.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, rt.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                           texc_id, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT,
                              GL_RENDERBUFFER, rt.depth_texture);

    GLenum stat = glCheckFramebufferStatus(GL_FRAMEBUFFER);
    assert(stat == GL_FRAMEBUFFER_COMPLETE);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    vuloxr::Logger::Info("SwapchainImage FBO:%d, TEXC:%d, TEXZ:%d, WH(%d, %d)",
                         rt.fbo, texc_id, rt.depth_texture, width, height);

    found = m_renderTargetMap.insert({texc_id, rt}).first;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, found->second.fbo);

  glViewport(0, 0, width, height);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  //  Render
  auto sobj = &getShader();
  glUseProgram(sobj->program);

  glEnableVertexAttribArray(sobj->loc_vtx);
  glEnableVertexAttribArray(sobj->loc_clr);
  glVertexAttribPointer(sobj->loc_vtx, 2, GL_FLOAT, GL_FALSE, 0, s_vtx);
  glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, s_col);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}
