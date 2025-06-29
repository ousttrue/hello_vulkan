#include "util_render_target.h"
#include "util_shader.h"
#include <GLES3/gl31.h>

static shader_obj_t s_sobj;

static float s_vtx[] = {
    -0.5f, 0.5f, -0.5f, -0.5f, 0.5f, 0.5f,
};

static float s_col[] = {
    1.0f, 0.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f,
};

static char s_strVS[] = "                         \
                                                  \
attribute    vec4    a_Vertex;                    \
attribute    vec4    a_Color;                     \
varying      vec4    v_color;                     \
                                                  \
void main (void)                                  \
{                                                 \
    gl_Position = a_Vertex;                       \
    v_color     = a_Color;                        \
}                                                ";

static char s_strFS[] = "                         \
                                                  \
precision mediump float;                          \
varying     vec4     v_color;                     \
                                                  \
void main (void)                                  \
{                                                 \
    gl_FragColor = v_color;                       \
}                                                 ";

int init_gles_scene() {
  generate_shader(&s_sobj, s_strVS, s_strFS);

  return 0;
}

int render_gles_scene(render_target_t &rtarget, int view_w, int view_h) {
  glBindFramebuffer(GL_FRAMEBUFFER, rtarget.fbo_id);

  glViewport(0, 0, view_w, view_h);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  /* ------------------------------------------- *
   *  Render
   * ------------------------------------------- */
  shader_obj_t *sobj = &s_sobj;
  glUseProgram(sobj->program);

  glEnableVertexAttribArray(sobj->loc_vtx);
  glEnableVertexAttribArray(sobj->loc_clr);
  glVertexAttribPointer(sobj->loc_vtx, 2, GL_FLOAT, GL_FALSE, 0, s_vtx);
  glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, s_col);

  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}
