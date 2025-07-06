/* ------------------------------------------------ *
 * The MIT License (MIT)
 * Copyright (c) 2019 terryky1220@gmail.com
 * ------------------------------------------------ */
#include "teapot.h"
#include "assertgl.h"
#include "teapot_data.h"
#include "util_matrix.h"
#include "util_shader.h"

static shader_obj_t s_sobj;
static GLuint s_vao_id, s_vtx_id, s_nrm_id, g_idx_id;
static GLint s_loc_mtx_mv;
static GLint s_loc_mtx_pmv;
static GLint s_loc_mtx_nrm;
static GLint s_loc_color;

static char s_strVS[] = "                                   \n\
                                                            \n\
attribute vec4  a_Vertex;                                   \n\
attribute vec3  a_Normal;                                   \n\
uniform   mat4  u_PMVMatrix;                                \n\
uniform   mat4  u_MVMatrix;                                 \n\
uniform   mat3  u_ModelViewIT;                              \n\
varying   vec3  v_diffuse;                                  \n\
varying   vec3  v_specular;                                 \n\
const     float shiness = 16.0;                             \n\
const     vec3  LightPos = vec3(4.0, 4.0, 4.0);             \n\
const     vec3  LightCol = vec3(1.0, 1.0, 1.0);             \n\
                                                            \n\
void DirectionalLight (vec3 normal, vec3 eyePos)            \n\
{                                                           \n\
    vec3  lightDir = normalize (LightPos);                  \n\
    vec3  halfV    = normalize (LightPos - eyePos);         \n\
    float dVP      = max(dot(normal, lightDir), 0.0);       \n\
    float dHV      = max(dot(normal, halfV   ), 0.0);       \n\
                                                            \n\
    float pf = 0.0;                                         \n\
    if(dVP > 0.0)                                           \n\
        pf = pow(dHV, shiness);                             \n\
                                                            \n\
    v_diffuse += dVP * LightCol;                            \n\
    v_specular+= pf  * LightCol;                            \n\
}                                                           \n\
                                                            \n\
void main(void)                                             \n\
{                                                           \n\
    gl_Position = u_PMVMatrix * a_Vertex;                   \n\
    vec3 normal = normalize(u_ModelViewIT * a_Normal);      \n\
    vec3 eyePos = vec3(u_MVMatrix * a_Vertex);              \n\
                                                            \n\
    v_diffuse  = vec3(0.0);                                 \n\
    v_specular = vec3(0.0);                                 \n\
    DirectionalLight(normal, eyePos);                       \n\
}                                                           ";

static char s_strFS[] = "                                   \n\
precision mediump float;                                    \n\
                                                            \n\
uniform vec3    u_color;                                    \n\
varying vec3    v_diffuse;                                  \n\
varying vec3    v_specular;                                 \n\
void main(void)                                             \n\
{                                                           \n\
    vec3 color = u_color * 0.1;                             \n\
    color += (u_color * v_diffuse);                         \n\
    color += v_specular;                                    \n\
    gl_FragColor = vec4(color, 1.0);                        \n\
}                                                           ";

int init_teapot() {
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  generate_shader(&s_sobj, s_strVS, s_strFS);
  s_loc_mtx_mv = glGetUniformLocation(s_sobj.program, "u_MVMatrix");
  s_loc_mtx_pmv = glGetUniformLocation(s_sobj.program, "u_PMVMatrix");
  s_loc_mtx_nrm = glGetUniformLocation(s_sobj.program, "u_ModelViewIT");
  s_loc_color = glGetUniformLocation(s_sobj.program, "u_color");

  glGenVertexArrays(1, &s_vao_id);
  glBindVertexArray(s_vao_id);

  auto teapotVertices = teapot::positions();
  glGenBuffers(1, &s_vtx_id);
  glEnableVertexAttribArray(s_sobj.loc_vtx);
  glBindBuffer(GL_ARRAY_BUFFER, s_vtx_id);
  glBufferData(GL_ARRAY_BUFFER,
               sizeof(teapotVertices[0]) * teapotVertices.size(),
               teapotVertices.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(s_sobj.loc_vtx, 3, GL_FLOAT, GL_FALSE, 0, 0);

  auto teapotNormals = teapot::normals();
  glGenBuffers(1, &s_nrm_id);
  glEnableVertexAttribArray(s_sobj.loc_nrm);
  glBindBuffer(GL_ARRAY_BUFFER, s_nrm_id);
  glBufferData(GL_ARRAY_BUFFER, sizeof(teapotNormals[0]) * teapotNormals.size(),
               teapotNormals.data(), GL_STATIC_DRAW);
  glVertexAttribPointer(s_sobj.loc_nrm, 3, GL_FLOAT, GL_FALSE, 0, 0);

  auto teapotIndices = teapot::indices();
  glGenBuffers(1, &g_idx_id);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, g_idx_id);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER,
               sizeof(teapotIndices[0]) * teapotIndices.size(),
               teapotIndices.data(), GL_STATIC_DRAW);

  GLASSERT();

  glBindVertexArray(0);
  return 0;
}

int draw_teapot(int count, float col[3], float *matP, float *matV) {
  float matM[16], matVM[16], matPVM[16], matVMI4x4[16], matVMI3x3[9];

  glUseProgram(s_sobj.program);

  matrix_identity(matM);
  matrix_translate(matM, 0.0f, 0.0f, -3.0f);
  matrix_rotate(matM, count * 0.1f, 0.0f, 1.0f, 0.0f);
  matrix_scale(matM, 0.3f, 0.3f, 0.3f);
  matrix_translate(matM, 0.0f, -4.0f, 0.0f);

  matrix_mult(matVM, matV, matM);

  matrix_copy(matVMI4x4, matVM);
  matrix_invert(matVMI4x4);
  matrix_transpose(matVMI4x4);
  matVMI3x3[0] = matVMI4x4[0];
  matVMI3x3[1] = matVMI4x4[1];
  matVMI3x3[2] = matVMI4x4[2];
  matVMI3x3[3] = matVMI4x4[4];
  matVMI3x3[4] = matVMI4x4[5];
  matVMI3x3[5] = matVMI4x4[6];
  matVMI3x3[6] = matVMI4x4[8];
  matVMI3x3[7] = matVMI4x4[9];
  matVMI3x3[8] = matVMI4x4[10];

  matrix_mult(matPVM, matP, matVM);
  glUniformMatrix4fv(s_loc_mtx_mv, 1, GL_FALSE, matVM);
  glUniformMatrix4fv(s_loc_mtx_pmv, 1, GL_FALSE, matPVM);
  glUniformMatrix3fv(s_loc_mtx_nrm, 1, GL_FALSE, matVMI3x3);

  glUniform3f(s_loc_color, col[0], col[1], col[2]);

  glEnable(GL_DEPTH_TEST);

  // draw_teapot_vbo(col);
  glBindVertexArray(s_vao_id);
  glDrawElements(GL_TRIANGLES, teapot::indices().size(), GL_UNSIGNED_SHORT,
                 0);
  GLASSERT();

  glBindVertexArray(0);

  return 0;
}

int delete_teapot() {
  glDeleteBuffers(1, &s_vtx_id);
  glDeleteBuffers(1, &s_nrm_id);
  glDeleteBuffers(1, &g_idx_id);

  GLASSERT();
  return 0;
}
