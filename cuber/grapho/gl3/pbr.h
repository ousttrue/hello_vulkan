#pragma once
#include "../mesh.h"
#include "cubemap.h"
#include "cuberenderer.h"
#include "error_check.h"
#include "fbo.h"
#include "shader.h"
#include "vao.h"
#include <assert.h>

namespace grapho {
namespace gl3 {

// pbr: generate a 2D LUT(look up table) from the BRDF equations used.
inline std::shared_ptr<grapho::gl3::Texture>
GenerateBrdfLUTTexture()
{
#include "shaders/brdf_fs.h"
#include "shaders/brdf_vs.h"

  auto brdfLUTTexture =
    grapho::gl3::Texture::Create({ .Width = 512,
                                   .Height = 512,
                                   .Format = grapho::PixelFormat::f16_RGB,
                                   .ColorSpace = grapho::ColorSpace::Linear },
                                 true);

  // then re-configure capture framebuffer object and render screen-space quad
  // with BRDF shader.
  grapho::gl3::Fbo fbo;
  fbo.AttachTexture2D(brdfLUTTexture->Handle());
  grapho::gl3::ClearViewport(grapho::camera::Viewport{ 512, 512 });
  auto brdfShader = grapho::gl3::ShaderProgram::Create(BRDF_VS, BRDF_FS);
  brdfShader->Use();

  // renderQuad() renders a 1x1 XY quad in NDC
  auto quad = grapho::mesh::Quad();
  auto quadVao = grapho::gl3::Vao::Create(quad);
  auto drawCount = quad->DrawCount();
  auto drawMode = *grapho::gl3::GLMode(quad->Mode);
  quadVao->Draw(drawMode, drawCount);

  glBindFramebuffer(GL_FRAMEBUFFER, 0);

  return brdfLUTTexture;
}

// pbr: convert HDR equirectangular environment map to cubemap equivalent
inline void
GenerateEnvCubeMap(const grapho::gl3::CubeRenderer& cubeRenderer,
                   uint32_t envCubemap)
{
#include "shaders/cubemap_vs.h"
#include "shaders/equirectangular_to_cubemap_fs.h"

  auto equirectangularToCubemapShader =
    grapho::gl3::ShaderProgram::Create(CUBEMAP_VS, EQUIRECTANGULAR_FS);
  equirectangularToCubemapShader->Use();
  equirectangularToCubemapShader->SetUniform("equirectangularMap", 0);

  cubeRenderer.Render(
    512,
    envCubemap,
    [equirectangularToCubemapShader](const auto& projection, const auto& view) {
      equirectangularToCubemapShader->SetUniform("projection", projection);
      equirectangularToCubemapShader->SetUniform("view", view);
    });
}

// pbr: solve diffuse integral by convolution to create an irradiance
// (cube)map.
inline void
GenerateIrradianceMap(const grapho::gl3::CubeRenderer& cubeRenderer,
                      uint32_t irradianceMap)
{
#include "shaders/cubemap_vs.h"
#include "shaders/irradiance_convolution_fs.h"
  auto irradianceShader =
    grapho::gl3::ShaderProgram::Create(CUBEMAP_VS, IRRADIANCE_CONVOLUTION_FS);
  irradianceShader->Use();
  irradianceShader->SetUniform("environmentMap", 0);

  cubeRenderer.Render(
    32,
    irradianceMap,
    [irradianceShader](const auto& projection, const auto& view) {
      irradianceShader->SetUniform("projection", projection);
      irradianceShader->SetUniform("view", view);
    });
}

// pbr: run a quasi monte-carlo simulation on the environment lighting to
// create a prefilter (cube)map.
inline void
GeneratePrefilterMap(const grapho::gl3::CubeRenderer& cubeRenderer,
                     uint32_t prefilterMap)
{
#include "shaders/cubemap_vs.h"
#include "shaders/prefilter_fs.h"
  auto prefilterShader =
    grapho::gl3::ShaderProgram::Create(CUBEMAP_VS, PREFILTER_FS);
  prefilterShader->Use();
  prefilterShader->SetUniform("environmentMap", 0);
  assert(!TryGetError());

  unsigned int maxMipLevels = 5;
  for (unsigned int mip = 0; mip < maxMipLevels; ++mip) {
    // reisze framebuffer according to mip-level size.
    auto mipSize = static_cast<int>(128 * std::pow(0.5, mip));
    float roughness = (float)mip / (float)(maxMipLevels - 1);
    prefilterShader->SetUniform("roughness", roughness);

    assert(!TryGetError());
    cubeRenderer.Render(
      mipSize,
      prefilterMap,
      [prefilterShader](const auto& projection, const auto& view) {
        prefilterShader->SetUniform("projection", projection);
        prefilterShader->SetUniform("view", view);
      },
      mip);
    assert(!TryGetError());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);
  }
}

struct PbrEnv
{
  std::shared_ptr<Cubemap> EnvCubemap;
  std::shared_ptr<Cubemap> IrradianceMap;
  std::shared_ptr<Cubemap> PrefilterMap;
  std::shared_ptr<Texture> BrdfLUTTexture;

  // Skybox skybox;
  std::shared_ptr<grapho::gl3::Vao> Cube;
  uint32_t CubeDrawCount = 0;
  std::shared_ptr<grapho::gl3::ShaderProgram> BackgroundShader;

  PbrEnv(const std::shared_ptr<Texture>& hdrTexture)
  {
    glDisable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);
    // set depth function to less than AND equal for skybox depth trick.
    glDepthFunc(GL_LEQUAL);
    // enable seamless cubemap sampling for lower mip levels in the pre-filter
    // map.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    EnvCubemap = grapho::gl3::Cubemap::Create(
      {
        512,
        512,
        grapho::PixelFormat::f16_RGB,
        grapho::ColorSpace::Linear,
      },
      true);
    EnvCubemap->SamplingLinear(true);
    assert(!TryGetError());

    // hdr to cuemap
    hdrTexture->Activate(0);
    grapho::gl3::CubeRenderer cubeRenderer;
    grapho::gl3::GenerateEnvCubeMap(cubeRenderer, EnvCubemap->Handle());
    EnvCubemap->GenerateMipmap();
    EnvCubemap->UnBind();
    assert(!TryGetError());

    // irradianceMap
    IrradianceMap = grapho::gl3::Cubemap::Create(
      {
        32,
        32,
        grapho::PixelFormat::f16_RGB,
        grapho::ColorSpace::Linear,
      },
      true);
    EnvCubemap->Activate(0);
    grapho::gl3::GenerateIrradianceMap(cubeRenderer, IrradianceMap->Handle());
    assert(!TryGetError());

    // prefilterMap
    PrefilterMap = grapho::gl3::Cubemap::Create(
      {
        128,
        128,
        grapho::PixelFormat::f16_RGB,
        grapho::ColorSpace::Linear,
      },
      true);
    PrefilterMap->SamplingLinear(true);
    PrefilterMap->GenerateMipmap();
    EnvCubemap->Activate(0);
    assert(!TryGetError());
    grapho::gl3::GeneratePrefilterMap(cubeRenderer, PrefilterMap->Handle());
    assert(!TryGetError());
    assert(!TryGetError());

    // brdefLUT
    BrdfLUTTexture = grapho::gl3::GenerateBrdfLUTTexture();
    assert(!TryGetError());

    // skybox
    auto cube = grapho::mesh::Cube();
    auto vbo =
      grapho::gl3::Vbo::Create(cube->Vertices.Size(), cube->Vertices.Data());
    std::shared_ptr<grapho::gl3::Vbo> slots[]{ vbo };
    Cube = grapho::gl3::Vao::Create(make_span(cube->Layouts), make_span(slots));
    CubeDrawCount = cube->Vertices.Count;
    assert(!TryGetError());

#include "shaders/background_fs.h"
#include "shaders/background_vs.h"
    BackgroundShader =
      grapho::gl3::ShaderProgram::Create(BACKGROUND_VS, BACKGROUND_FS);
    if (!BackgroundShader) {
      throw std::runtime_error(GetErrorString());
    }
    BackgroundShader->Use();
    BackgroundShader->SetUniform("environmentMap", 0);
    assert(!TryGetError());
  }

  void Activate()
  {
    IrradianceMap->Activate(0);
    PrefilterMap->Activate(1);
    BrdfLUTTexture->Activate(2);
  }

  void DrawSkybox(const XMFLOAT4X4& projection, const XMFLOAT4X4& view)
  {
    glEnable(GL_DEPTH_TEST);
    // set depth function to less than AND equal for skybox depth trick.
    glDepthFunc(GL_LEQUAL);
    // enable seamless cubemap sampling for lower mip levels in the pre-filter
    // map.
    glEnable(GL_TEXTURE_CUBE_MAP_SEAMLESS);

    auto isCull = glIsEnabled(GL_CULL_FACE);
    if (isCull) {
      glDisable(GL_CULL_FACE);
    }

    EnvCubemap->Activate(0);
    // skybox.Draw(projection, view);

    // render skybox (render as last to prevent overdraw)
    BackgroundShader->Use();
    BackgroundShader->SetUniform("projection", projection);
    BackgroundShader->SetUniform("view", view);
    // glBindTexture(GL_TEXTURE_CUBE_MAP, irradianceMap); // display irradiance
    // map glBindTexture(GL_TEXTURE_CUBE_MAP, prefilterMap); // display
    // prefilter map
    Cube->Draw(GL_TRIANGLES, CubeDrawCount);

    // render BRDF map to screen
    // brdfShader.Use();
    // renderQuad();
    if (isCull) {
      glEnable(GL_CULL_FACE);
    }
  }
};

inline std::shared_ptr<ShaderProgram>
CreatePbrShader(std::span<std::u8string_view> _vs = {},
                std::span<std::u8string_view> _fs = {})
{
#include "shaders/pbr_fs.h"
#include "shaders/pbr_vs.h"
  std::vector<std::u8string_view> vs = { _vs.begin(), _vs.end() };
  std::vector<std::u8string_view> fs = { _fs.begin(), _fs.end() };
  if (vs.empty()) {
    vs.push_back(PBR_VS);
  }
  if (fs.empty()) {
    fs.push_back(PBR_FS);
  }
  return grapho::gl3::ShaderProgram::Create(vs, fs);
}

}
}
