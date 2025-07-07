#include <DirectXMath.h>
#include <GL/glew.h>
#include <cuber/gl3/GlLineRenderer.h>
#include <cuber/mesh.h>
#include <grapho/gl3/error_check.h>
#include <grapho/gl3/shader.h>
#include <grapho/gl3/vao.h>

using namespace grapho::gl3;

namespace cuber::gl3 {

static auto vertex_shader_text = u8R"(
uniform mat4 VP;
in vec3 vPos;
in vec4 vColor;
out vec4 color;

void main()
{
    gl_Position = VP * vec4(vPos, 1.0);
    color = vColor;
}
)";

static auto fragment_shader_text = u8R"(
in vec4 color;
out vec4 FragColor;

void main()
{
    FragColor = color;
}
)";

GlLineRenderer::GlLineRenderer()
{

  // auto glsl_version = "#version 150";
  auto glsl_version = u8"#version 310 es\nprecision highp float;";

  std::u8string_view vs[] = {
    glsl_version,
    u8"\n",
    vertex_shader_text,
  };
  std::u8string_view fs[] = {
    glsl_version,
    u8"\n",
    fragment_shader_text,
  };
  if (auto shader = ShaderProgram::Create(vs, fs)) {
    shader_ = shader;
  } else {
    throw std::runtime_error(grapho::GetErrorString());
  }

  vbo_ = Vbo::Create(sizeof(LineVertex) * 65535, nullptr);
  if (!vbo_) {
    throw std::runtime_error("grapho::gl3::Vbo::Create");
  }

  std::shared_ptr<grapho::gl3::Vbo> slots[] = {
    vbo_, //
  };
  grapho::VertexLayout layouts[] = {
      {
          .Id =
              {
                  .AttributeLocation = 0,
                  .Slot = 0,
                  // .SemanticName = "vPos",
              },
          .Type = grapho::ValueType::Float,
          .Count = 3,
          .Offset = 0,
          .Stride = sizeof(LineVertex),
      },
      {
          .Id =
              {
                  .AttributeLocation = 1,
                  .Slot = 0,
                  // .SemanticName = "vColor",
              },
          .Type = grapho::ValueType::Float,
          .Count = 4,
          .Offset = offsetof(LineVertex, Color),
          .Stride = sizeof(LineVertex),
      },
  };

  vao_ = Vao::Create(grapho::make_span(layouts), grapho::make_span(slots));
  if (!vao_) {
    throw std::runtime_error("grapho::gl3::Vao::Create");
  }
}
GlLineRenderer::~GlLineRenderer() {}
void
GlLineRenderer::Render(const float projection[16],
                       const float view[16],
                       std::span<const LineVertex> lines)
{
  if (lines.empty()) {
    return;
  }
  glEnable(GL_DEPTH_TEST);
  glEnable(GL_CULL_FACE);

  auto v = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)view);
  auto p = DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)projection);
  DirectX::XMFLOAT4X4 vp;
  DirectX::XMStoreFloat4x4(&vp, v * p);

  shader_->Use();
  shader_->SetUniform("VP", vp);

  vbo_->Upload(sizeof(LineVertex) * lines.size(), lines.data());
  vao_->Draw(GL_LINES, lines.size(), 0);
}

} // namespace cuber::gl3
