#include <DirectXCollision.h>

#include <GL/glew.h>

#include "BvhPanel.h"
#include "GlfwPlatform.h"
#include "GuiApp.h"
#include <cuber/gl3/GlCubeRenderer.h>
#include <cuber/gl3/GlLineRenderer.h>
#include <grapho/gl3/texture.h>
#include <grapho/imgui/printfbuffer.h>
#include <imgui.h>

#include <Windows.h>

struct rgba
{
  uint8_t r;
  uint8_t g;
  uint8_t b;
  uint8_t a;
};

const auto TextureBind = 0;
const auto PalleteIndex = 9;

//   7+-+6
//   / /|
// 3+-+2 +5
// | |
// 0+-+1
DirectX::XMFLOAT3 p[8] = {
  { -0.5f, -0.5f, -0.5f }, //
  { +0.5f, -0.5f, -0.5f }, //
  { +0.5f, +0.5f, -0.5f }, //
  { -0.5f, +0.5f, -0.5f }, //
  { -0.5f, -0.5f, +0.5f }, //
  { +0.5f, -0.5f, +0.5f }, //
  { +0.5f, +0.5f, +0.5f }, //
  { -0.5f, +0.5f, +0.5f }, //
};

std::array<int, 4> triangles[] = {
  { 1, 5, 6, 2 }, // x+
  { 3, 2, 6, 7 }, // y+
  { 0, 1, 2, 3 }, // z+
  { 4, 7, 3, 0 }, // x-
  { 1, 0, 4, 5 }, // y-
  { 5, 6, 7, 4 }, // z-
};

static std::optional<float>
Intersect(DirectX::XMVECTOR origin,
          DirectX::XMVECTOR dir,
          DirectX::XMMATRIX m,
          int t)
{
  auto [i0, i1, i2, i3] = triangles[t];
  float dist;
  if (DirectX::TriangleTests::Intersects(
        origin,
        dir,
        DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i0]), m),
        DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i1]), m),
        DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i2]), m),
        dist)) {
    return dist;
  } else if (DirectX::TriangleTests::Intersects(
               origin,
               dir,
               DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i2]), m),
               DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i3]), m),
               DirectX::XMVector3Transform(DirectX::XMLoadFloat3(&p[i0]), m),
               dist)) {
    return dist;
  } else {
    return std::nullopt;
  }
}

int
main(int argc, char** argv)
{
  // imgui
  GuiApp app;
  app.Camera.Translation = { 0, 1, 4 };

  // window
  GlfwPlatform platform;
  if (!platform.Create()) {
    return 1;
  }

  // bvh scene
  BvhPanel bvhPanel;

  // load bvh
  if (argc > 1) {
    if (auto bvh = Bvh::ParseFile(argv[1])) {
      bvhPanel.SetBvh(bvh);
    }
  }

  // cuber
  cuber::gl3::GlCubeRenderer cubeRenderer;
  cuber::gl3::GlLineRenderer lineRenderer;

  std::vector<cuber::LineVertex> lines;
  cuber::PushGrid(lines);

  // texture
  static rgba pixels[4] = {
    { 255, 0, 0, 255 },
    { 0, 255, 0, 255 },
    { 0, 0, 255, 255 },
    { 255, 255, 255, 255 },
  };
  auto texture = grapho::gl3::Texture::Create({
    .Width = 2,
    .Height = 2,
    .Format = grapho::PixelFormat::u8_RGBA,
    .Pixels = &pixels[0].r,
  });
  texture->SamplingPoint();

  std::vector<cuber::Instance> instances;
  instances.push_back({});
  auto t = DirectX::XMMatrixTranslation(0, 1, -1);
  auto s = DirectX::XMMatrixScaling(1.6f, 0.9f, 0.1f);
  DirectX::XMStoreFloat4x4(&instances.back().Matrix, s * t);

  // Pallete
  instances.back().PositiveFaceFlag = {
    PalleteIndex, PalleteIndex, PalleteIndex, 0
  };
  instances.back().NegativeFaceFlag = {
    PalleteIndex, PalleteIndex, PalleteIndex, 0
  };
  cubeRenderer.Pallete.Colors[PalleteIndex] = { 1, 1, 1, 1 };
  cubeRenderer.Pallete.Textures[PalleteIndex] = {
    TextureBind, TextureBind, TextureBind, TextureBind
  };
  cubeRenderer.UploadPallete();

  grapho::imgui::PrintfBuffer buf;

  // main loop
  while (auto time = platform.NewFrame(app.clear_color)) {
    // imgui
    {
      app.UpdateGui();
      bvhPanel.UpdateGui();
    }

    // scene
    {
      auto cubes = bvhPanel.GetCubes();
      instances.resize(1 + cubes.size());
      std::copy(cubes.begin(), cubes.end(), instances.data() + 1);

      auto& io = ImGui::GetIO();

      if (auto ray = app.Camera.GetRay(io.MousePos.x, io.MousePos.y)) {

        if (ImGui::Begin("ray")) {
          ImGui::InputFloat2("mouse pos", &io.MousePos.x);
          ImGui::InputFloat3("origin", &ray->Origin.x);
          ImGui::InputFloat3("dir", &ray->Direction.x);
        }
        ImGui::End();

        for (int i = 1; i < instances.size(); ++i) {
          auto& cube = instances[i];
          auto inv = DirectX::XMMatrixInverse(
            nullptr, DirectX::XMLoadFloat4x4(&cube.Matrix));
          auto local_ray = ray->Transform(inv);

          if (ImGui::Begin("ray")) {
            ImGui::InputFloat3(buf.Printf("local.origin[%d]", i),
                               &local_ray.Origin.x);
            ImGui::InputFloat3(buf.Printf("local.dir[%d]", i),
                               &local_ray.Direction.x);

            auto origin = DirectX::XMLoadFloat3(&ray->Origin);
            auto dir = DirectX::XMLoadFloat3(&ray->Direction);

            auto m = DirectX::XMLoadFloat4x4(&cube.Matrix);

            {
              float hit[3] = { 0, 0, 0 };

              if (auto d = Intersect(origin, dir, m, 0)) {
                cube.PositiveFaceFlag.x = 7;
                hit[0] = *d;
              } else {
                cube.PositiveFaceFlag.x = 8;
              }

              if (auto d = Intersect(origin, dir, m, 1)) {
                cube.PositiveFaceFlag.y = 7;
                hit[1] = *d;
              } else {
                cube.PositiveFaceFlag.y = 8;
              }

              if (auto d = Intersect(origin, dir, m, 2)) {
                cube.PositiveFaceFlag.z = 7;
                hit[2] = *d;
              } else {
                cube.PositiveFaceFlag.z = 8;
              }
              ImGui::InputFloat3(buf.Printf("%d.hit.positive", i), hit);
            }

            {
              float hit[3] = { 0, 0, 0 };
              if (auto d = Intersect(origin, dir, m, 3)) {
                cube.NegativeFaceFlag.x = 7;
                hit[0] = *d;
              } else {
                cube.NegativeFaceFlag.x = 8;
              }

              if (auto d = Intersect(origin, dir, m, 4)) {
                cube.NegativeFaceFlag.y = 7;
                hit[1] = *d;
              } else {
                cube.NegativeFaceFlag.y = 8;
              }

              if (auto d = Intersect(origin, dir, m, 5)) {
                cube.NegativeFaceFlag.z = 7;
                hit[2] = *d;
              } else {
                cube.NegativeFaceFlag.z = 8;
              }
              ImGui::InputFloat3(buf.Printf("%d.hit.negative", i), hit);
            }
          }
          ImGui::End();
        }
      }

      texture->Activate(TextureBind);
      cubeRenderer.Render(&app.Camera.ProjectionMatrix._11,
                          &app.Camera.ViewMatrix._11,
                          instances.data(),
                          instances.size());
      lineRenderer.Render(
        &app.Camera.ProjectionMatrix._11, &app.Camera.ViewMatrix._11, lines);

      auto data = app.RenderGui();
      platform.EndFrame(data);
    }
  }

  return 0;
}
