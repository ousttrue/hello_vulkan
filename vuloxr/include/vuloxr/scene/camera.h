#pragma once
#include <DirectXMath.h>
#include <numbers>

namespace vuloxr {
namespace camera {

struct PerspectiveProjection {
  float fieldOfVyewY = 45.0f / 180.0f * std::numbers::pi;
  float nearZ = 0.01f;
  float farZ = 100.0f;
  float aspectRatio = 1.0f;
  DirectX::XMFLOAT4X4 matrix;
  void setViewSize(int width, int height) {
    this->aspectRatio = static_cast<float>(width) / static_cast<float>(height);
  }
  DirectX::XMFLOAT4X4 &calc() {
    DirectX::XMStoreFloat4x4(
        &this->matrix,
        DirectX::XMMatrixPerspectiveFovRH(this->fieldOfVyewY, this->aspectRatio,
                                          this->nearZ, this->farZ));
    return this->matrix;
  }
};

inline DirectX::XMFLOAT4X4
vulkanViewProjectionClip(const DirectX::XMFLOAT4X4 &view,
                         const DirectX::XMFLOAT4X4 &projection) {
  // auto Model = DirectX::XMLoadFloat4x4(&model);
  // Vulkan clip space has inverted Y and half Z.
  auto clip = DirectX::XMMatrixSet(1.0f, 0.0f, 0.0f, 0.0f,  //
                                   0.0f, -1.0f, 0.0f, 0.0f, //
                                   0.0f, 0.0f, 0.5f, 0.0f,  //
                                   0.0f, 0.0f, 0.5f, 1.0f);
  auto m = DirectX::XMLoadFloat4x4(&view) *
           DirectX::XMLoadFloat4x4(&projection) * clip;
  DirectX::XMFLOAT4X4 out;
  DirectX::XMStoreFloat4x4(&out, m);
  return out;
}

} // namespace camera
} // namespace vuloxr
