#include <DirectXMath.h>
#include <GL/glew.h>
#include <cuber/gl3/GlCubeRenderer.h>
#include <cuber/mesh.h>
#include <grapho/gl3/error_check.h>
#include <grapho/gl3/shader.h>
#include <grapho/gl3/ubo.h>
#include <grapho/gl3/vao.h>

using namespace grapho::gl3;

namespace cuber::gl3 {

static auto vertex_m_shadertext = u8R"(
uniform mat4 VP;
in vec4 vPosFace;
in vec4 vUvBarycentric;
in vec4 iRow0;
in vec4 iRow1;
in vec4 iRow2;
in vec4 iRow3;
in vec4 iPositive_xyz_flag;
in vec4 iNegative_xyz_flag;
out vec4 oUvBarycentric;
flat out uvec3 o_Palette_Flag_Flag;

mat4 transform(vec4 r0, vec4 r1, vec4 r2, vec4 r3)
{
  return mat4(
    r0.x, r0.y, r0.z, r0.w,
    r1.x, r1.y, r1.z, r1.w,
    r2.x, r2.y, r2.z, r2.w,
    r3.x, r3.y, r3.z, r3.w
  );
}

void main()
{
    gl_Position = VP * transform(iRow0, iRow1, iRow2, iRow3) * vec4(vPosFace.xyz, 1);
    oUvBarycentric = vUvBarycentric;
    if(vPosFace.w==0.0)
    {
      o_Palette_Flag_Flag = uvec3(iPositive_xyz_flag.x, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else if(vPosFace.w==1.0)
    {
      o_Palette_Flag_Flag = uvec3(iPositive_xyz_flag.y, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else if(vPosFace.w==2.0)
    {
      o_Palette_Flag_Flag = uvec3(iPositive_xyz_flag.z, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else if(vPosFace.w==3.0)
    {
      o_Palette_Flag_Flag = uvec3(iNegative_xyz_flag.x, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else if(vPosFace.w==4.0)
    {
      o_Palette_Flag_Flag = uvec3(iNegative_xyz_flag.y, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else if(vPosFace.w==5.0)
    {
      o_Palette_Flag_Flag = uvec3(iNegative_xyz_flag.z, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
    else{
      o_Palette_Flag_Flag = uvec3(0, 
        iPositive_xyz_flag.w,
        iNegative_xyz_flag.w);
    }
}
)";

static auto fragment_m_shadertext = u8R"(
in vec4 oUvBarycentric;
flat in uvec3 o_Palette_Flag_Flag;
out vec4 FragColor;
layout (std140) uniform palette { 
  vec4 colors[32];
  vec4 textures[32];
} Palette;

uniform sampler2D sampler0;
uniform sampler2D sampler1;
uniform sampler2D sampler2;

// https://github.com/rreusser/glsl-solid-wireframe
float grid (vec2 vBC, float width) {
  vec3 bary = vec3(vBC.x, vBC.y, 1.0 - vBC.x - vBC.y);
  vec3 d = fwidth(bary);
  vec3 a3 = smoothstep(d * (width - 0.5), d * (width + 0.5), bary);
  return min(a3.x, a3.y);
}

void main()
{
    vec4 border = vec4(vec3(grid(oUvBarycentric.zw, 1.0)), 1);
    uint index = o_Palette_Flag_Flag.x;
    vec4 color = Palette.colors[index];
    vec4 texel;
    if(Palette.textures[index].x==0.0)
    {
      texel = texture(sampler0, oUvBarycentric.xy);
    }
    else if(Palette.textures[index].x==1.0)
    {
      texel = texture(sampler1, oUvBarycentric.xy);
    }
    else if(Palette.textures[index].x==2.0)
    {
      texel = texture(sampler2, oUvBarycentric.xy);
    }
    else{
      texel = vec4(1, 1, 1, 1);
    }
    FragColor = texel * color * border;
}
)";

GlCubeRenderer::GlCubeRenderer()
{

  // auto glsl_version = "#version 150";
  auto glsl_version = u8"#version 310 es\nprecision highp float;";

  std::u8string_view vs[] = {
    glsl_version,
    u8"\n",
    vertex_m_shadertext,
  };
  std::u8string_view fs[] = {
    glsl_version,
    u8"\n",
    fragment_m_shadertext,
  };
  if (auto shader = ShaderProgram::Create(vs, fs)) {
    m_shader = shader;
  } else {
    throw std::runtime_error(::grapho::GetErrorString());
  }

  auto [vertices, indices, layouts] = Cube(true, false);

  auto vbo = Vbo::Create(sizeof(Vertex) * vertices.size(), vertices.data());
  if (!vbo) {
    throw std::runtime_error("cuber::Vbo::Create");
  }

  m_instance_vbo = Vbo::Create(sizeof(float) * 16 * 65535, nullptr);
  if (!m_instance_vbo) {
    throw std::runtime_error("cuber::Vbo::Create: m_instance_vbo");
  }

  std::shared_ptr<grapho::gl3::Vbo> slots[] = {
    vbo,            //
    m_instance_vbo, //
  };

  auto ibo = Ibo::Create(
    sizeof(uint32_t) * indices.size(), indices.data(), GL_UNSIGNED_INT);
  if (!ibo) {
    throw std::runtime_error("cuber::Vbo::Create");
  }
  m_vao =
    Vao::Create(grapho::make_span(layouts), grapho::make_span(slots), ibo);
  if (!m_vao) {
    throw std::runtime_error("cuber::Vao::Create");
  }

  m_ubo = Ubo::Create(sizeof(Pallete), &Pallete);
}

GlCubeRenderer::~GlCubeRenderer() {}

void
GlCubeRenderer::UploadPallete()
{
  m_ubo->Upload(Pallete);
}

void
GlCubeRenderer::Render(const float projection[16],
                       const float view[16],
                       const Instance* data,
                       uint32_t instanceCount)
{
  if (instanceCount == 0) {
    return;
  }
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  auto v = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)view);
  auto p = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)projection);
  DirectX::XMFLOAT4X4 vp;
  DirectX::XMStoreFloat4x4(&vp, v * p);

  m_shader->Use();
  m_shader->SetUniform("VP", vp);

  auto block_index = m_shader->UboBlockIndex("palette");
  m_shader->UboBind(*block_index, 1);
  m_ubo->SetBindingPoint(1);

  m_instance_vbo->Upload(sizeof(Instance) * instanceCount, data);
  m_vao->DrawInstance(instanceCount, CUBE_INDEX_COUNT, 0);
}

} // namespace cuber::gl3
