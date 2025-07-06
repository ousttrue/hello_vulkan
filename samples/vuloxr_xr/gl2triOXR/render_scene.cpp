#include "render_scene.h"
#include "assertgl.h"
#include "util_shader.h"

static shader_obj_t s_sobj;

static GLfloat s_vtx[] = {
    -0.5f, 0.5f,  //
    -0.5f, -0.5f, //
    0.5f,  0.5f,  //
};

static GLfloat s_col[] = {
    1.0f, 0.0f, 0.0f, 1.0f, //
    0.0f, 1.0f, 0.0f, 1.0f, //
    0.0f, 0.0f, 1.0f, 1.0f, //
};

static const char *s_strVS = R"(
attribute    vec4    a_Vertex;
attribute    vec4    a_Color;
varying      vec4    v_color;

void main (void)
{
    gl_Position = a_Vertex;
    v_color     = a_Color;
}
)";

static const char *s_strFS = R"(
precision mediump float;
varying     vec4     v_color;

void main (void)
{
    gl_FragColor = v_color;
}
)";

int init_gles_scene() {
  generate_shader(&s_sobj, s_strVS, s_strFS);

  return 0;
}

int render_gles_scene(uint32_t fbo_id, XrRect2Di imgRect) {
  shader_obj_t *sobj = &s_sobj;

  static GLuint vao = 0;
  static GLuint vbo[] = {0, 0};
  if (vao == 0) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(2, vbo);

    glBindVertexArray(vao);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_vtx), s_vtx, GL_STATIC_DRAW);
    glVertexAttribPointer(sobj->loc_vtx, 2, GL_FLOAT, GL_FALSE, 0, 0);

    glEnableVertexAttribArray(sobj->loc_clr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_col), s_col, GL_STATIC_DRAW);
    glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, 0);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    GLASSERT();
  }

  int view_x = imgRect.offset.x;
  int view_y = imgRect.offset.y;
  int view_w = imgRect.extent.width;
  int view_h = imgRect.extent.height;

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

  glViewport(view_x, view_y, view_w, view_h);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT);

  /* ------------------------------------------- *
   *  Render
   * ------------------------------------------- */
  glUseProgram(sobj->program);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
  glBindVertexArray(0);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}
