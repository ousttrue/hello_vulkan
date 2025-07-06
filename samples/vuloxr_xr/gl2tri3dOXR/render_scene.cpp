#include "../xr_linear.h"
#include "util_debugstr.h"
#include "util_matrix.h"
#include "util_shader.h"
#include <stdio.h>

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
attribute vec4  a_Vertex;
attribute vec4  a_Color;
varying   vec4  v_color;
uniform   mat4  u_PMVMatrix;

void main(void)
{
    gl_Position = u_PMVMatrix * a_Vertex;
    v_color     = a_Color;
}
)";

static const char *s_strFS = R"(
precision mediump float;
varying   vec4  v_color;

void main(void)
{
    gl_FragColor = v_color;
}
)";

int init_gles_scene() {
  generate_shader(&s_sobj, s_strVS, s_strFS);
  init_dbgstr(0, 0);

  return 0;
}

static int draw_line(const float mtxPV[16], const float p0[3],
                     const float p1[3], const float color[4]) {
  shader_obj_t *sobj = &s_sobj;

  GLfloat floor_vtx[6];

  static GLuint vao = 0;
  static GLuint vbo[2] = {0, 0};
  if (vao == 0) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(2, vbo);

    glBindVertexArray(vao);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(floor_vtx), nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(sobj->loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray(sobj->loc_clr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, 16, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // GLASSERT();
  }

  // update vbo
  for (int i = 0; i < 3; i++) {
    floor_vtx[0 + i] = p0[i];
    floor_vtx[3 + i] = p1[i];
  }
  glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
  glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(floor_vtx), floor_vtx);
  glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
  glBufferSubData(GL_ARRAY_BUFFER, 0, 16, color);

  // render
  glUseProgram(sobj->program);
  glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, mtxPV);
  glEnable(GL_DEPTH_TEST);
  glBindVertexArray(vao);
  glDrawArrays(GL_LINES, 0, 2);

  glBindVertexArray(0);

  return 0;
}

static int draw_stage(const float matStage[16]) {
  float col_r[4] = {1.0f, 0.0f, 0.0f, 1.0f};
  float col_g[4] = {0.0f, 1.0f, 0.0f, 1.0f};
  float col_b[4] = {0.0f, 0.0f, 1.0f, 1.0f};
  float col_gray[] = {0.5f, 0.5f, 0.5f, 1.0f};
  float p0[3] = {0.0f, 0.0f, 0.0f};
  float py[3] = {0.0f, 1.0f, 0.0f};

  for (int x = -10; x <= 10; x++) {
    float *col = (x == 0) ? col_b : col_gray;
    float p0[3] = {1.0f * x, 0.0f, -10.0f};
    float p1[3] = {1.0f * x, 0.0f, 10.0f};
    draw_line(matStage, p0, p1, col);
  }
  for (int z = -10; z <= 10; z++) {
    float *col = (z == 0) ? col_r : col_gray;
    float p0[3] = {-10.0f, 0.0f, 1.0f * z};
    float p1[3] = {10.0f, 0.0f, 1.0f * z};
    draw_line(matStage, p0, p1, col);
  }

  draw_line(matStage, p0, py, col_g);

  return 0;
}

static int draw_triangle(const float matStage[16]) {
  shader_obj_t *sobj = &s_sobj;

  static GLuint vao = 0;
  static GLuint vbo[2] = {0, 0};
  if (vao == 0) {
    glGenVertexArrays(1, &vao);
    glGenBuffers(2, vbo);

    glBindVertexArray(vao);

    glEnableVertexAttribArray(sobj->loc_vtx);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[0]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_vtx), s_vtx, GL_STATIC_DRAW);
    glVertexAttribPointer(sobj->loc_vtx, 2, GL_FLOAT, GL_FALSE, 0, nullptr);

    glEnableVertexAttribArray(sobj->loc_clr);
    glBindBuffer(GL_ARRAY_BUFFER, vbo[1]);
    glBufferData(GL_ARRAY_BUFFER, sizeof(s_col), s_col, GL_STATIC_DRAW);
    glVertexAttribPointer(sobj->loc_clr, 4, GL_FLOAT, GL_FALSE, 0, nullptr);

    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    // GLASSERT();
  }

  float matM[16], matPVM[16];

  matrix_identity(matM);
  matrix_translate(matM, 0.0f, 1.0f, -1.0f);
  matrix_mult(matPVM, matStage, matM);

  // render
  glUseProgram(sobj->program);
  glUniformMatrix4fv(sobj->loc_mtx, 1, GL_FALSE, matPVM);
  glBindVertexArray(vao);
  glDrawArrays(GL_TRIANGLE_STRIP, 0, 3);
  glBindVertexArray(0);

  return 0;
}

int render_gles_scene(const XrCompositionLayerProjectionView &layerView,
                      uint32_t fbo_id, XrPosef &stagePose, uint32_t viewID) {
  int view_x = layerView.subImage.imageRect.offset.x;
  int view_y = layerView.subImage.imageRect.offset.y;
  int view_w = layerView.subImage.imageRect.extent.width;
  int view_h = layerView.subImage.imageRect.extent.height;

  glBindFramebuffer(GL_FRAMEBUFFER, fbo_id);

  glViewport(view_x, view_y, view_w, view_h);

  glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
  glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

  /* ------------------------------------------- *
   *  Matrix Setup
   *    (matPV)  = (proj) x (view)
   *    (matPVM) = (proj) x (view) x (model)
   * ------------------------------------------- */
  XrMatrix4x4f matP, matV, matC, matM, matPV, matPVM;

  /* Projection Matrix */
  XrMatrix4x4f_CreateProjectionFov(&matP, GRAPHICS_OPENGL_ES, layerView.fov,
                                   0.05f, 100.0f);

  /* View Matrix (inverse of Camera matrix) */
  XrVector3f scale = {1.0f, 1.0f, 1.0f};
  const auto &vewPose = layerView.pose;
  XrMatrix4x4f_CreateTranslationRotationScale(&matC, &vewPose.position,
                                              &vewPose.orientation, &scale);
  XrMatrix4x4f_InvertRigidBody(&matV, &matC);

  /* Stage Space Matrix */
  XrMatrix4x4f_CreateTranslationRotationScale(&matM, &stagePose.position,
                                              &stagePose.orientation, &scale);

  XrMatrix4x4f_Multiply(&matPV, &matP, &matV);
  XrMatrix4x4f_Multiply(&matPVM, &matPV, &matM);

  /* ------------------------------------------- *
   *  Render
   * ------------------------------------------- */
  float *matStage = reinterpret_cast<float *>(&matPVM);

  draw_stage(matStage);
  draw_triangle(matStage);

  {
    auto &pos = layerView.pose.position;
    auto &rot = layerView.pose.orientation;
    auto &fov = layerView.fov;
    int x = 800;
    int y = 400;
    char strbuf[128];

    update_dbgstr_winsize(view_w, view_h);

    sprintf(strbuf, "VIEWPOS(%6.4f, %6.4f, %6.4f)", pos.x, pos.y, pos.z);
    draw_dbgstr(strbuf, x, y);
    y += 22;

    sprintf(strbuf, "VIEWROT(%6.4f, %6.4f, %6.4f, %6.4f)", rot.x, rot.y, rot.z,
            rot.w);
    draw_dbgstr(strbuf, x, y);
    y += 22;

    sprintf(strbuf, "VIEWFOV(%6.4f, %6.4f, %6.4f, %6.4f)", fov.angleLeft,
            fov.angleRight, fov.angleUp, fov.angleDown);
    draw_dbgstr(strbuf, x, y);
    y += 22;
  }

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return 0;
}
