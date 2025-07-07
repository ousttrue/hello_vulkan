#include <GL/glew.h>

#include "fbo.h"

namespace grapho::gl3 {

void
ClearViewport(const camera::Viewport& vp, const ClearParam& param)
{
  glViewport(0, 0, static_cast<int>(vp.Width), static_cast<int>(vp.Height));
  assert(!TryGetError());
  glScissor(0, 0, static_cast<int>(vp.Width), static_cast<int>(vp.Height));
  assert(!TryGetError());
  if (param.ApplyAlpha) {
    glClearColor(vp.Color[0] * vp.Color[3],
                 vp.Color[1] * vp.Color[3],
                 vp.Color[2] * vp.Color[3],
                 vp.Color[3]);
  } else {
    glClearColor(vp.Color[0], vp.Color[1], vp.Color[2], vp.Color[3]);
  }
  assert(!TryGetError());
  if (param.Depth) {
    glClearDepth(vp.Depth);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
  } else {
    glClear(GL_COLOR_BUFFER_BIT);
  }
  assert(!TryGetError());
}

Fbo::Fbo()
{
  glGenFramebuffers(1, &m_fbo);
}

Fbo::~Fbo()
{
  glDeleteFramebuffers(1, &m_fbo);
  if (m_rbo) {
    glDeleteRenderbuffers(1, &m_rbo);
  }
}

void
Fbo::Bind()
{
  glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
}

void
Fbo::Unbind()
{
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void
Fbo::AttachDepth(int width, int height)
{
  Bind();
  if (!m_rbo) {
    glGenRenderbuffers(1, &m_rbo);
  }
  glBindRenderbuffer(GL_RENDERBUFFER, m_rbo);
  glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);
  glFramebufferRenderbuffer(
    GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_rbo);
  glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void
Fbo::AttachTexture2D(uint32_t texture, int mipLevel)
{
  Bind();
  glFramebufferTexture2D(
    GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, mipLevel);
  // uint32_t buffers[] = { GL_COLOR_ATTACHMENT0 };
  // glDrawBuffers(1, buffers);
}

void
Fbo::AttachCubeMap(int i, uint32_t texture, int mipLevel)
{
  Bind();
  glFramebufferTexture2D(GL_FRAMEBUFFER,
                         GL_COLOR_ATTACHMENT0,
                         GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         texture,
                         mipLevel);
}

} // namespace
