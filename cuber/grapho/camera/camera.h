#pragma once
#include "ray.h"
#include "viewport.h"
#include <optional>

namespace grapho {
namespace camera {

struct Projection
{
  Viewport Viewport;
  float FovY;
  float NearZ = 0.01f;
  float FarZ = 1000.0f;

  Projection();

  void Update(DirectX::XMFLOAT4X4* projection);

  void SetViewport(const struct Viewport& viewport) { Viewport = viewport; }

  void SetRect(float x, float y, float w, float h)
  {
    SetViewport({ x, y, w, h });
  }

  void SetSize(float w, float h) { SetRect(0, 0, w, h); }
};

struct MouseState
{
  float X;
  float Y;
  float DeltaX = 0;
  float DeltaY = 0;
  bool LeftDown = false;
  bool MiddleDown = false;
  bool RightDown = false;
  float Wheel = 0;
};

struct Camera
{
  Projection Projection;
  DirectX::XMFLOAT4 Rotation = { 0, 0, 0, 1 };
  DirectX::XMFLOAT3 Translation = { 0, 0, 0 };

  DirectX::XMFLOAT4X4 ViewMatrix;
  DirectX::XMFLOAT4X4 ProjectionMatrix;

  float GazeDistance = 5;
  float TmpYaw;
  float TmpPitch;

  void YawPitch(int dx, int dy);

  void Shift(int dx, int dy);

  void Dolly(int d);

  void MouseInputTurntable(const MouseState& mouse)
  {
    if (mouse.RightDown) {
      YawPitch(static_cast<int>(mouse.DeltaX), static_cast<int>(mouse.DeltaY));
    }
    if (mouse.MiddleDown) {
      Shift(static_cast<int>(mouse.DeltaX), static_cast<int>(mouse.DeltaY));
    }
    Dolly(static_cast<int>(mouse.Wheel));
  }

  void Update();

  DirectX::XMFLOAT4X4 ViewProjection() const;

  void Fit(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max);

  // 0-> X
  // |
  // v
  //
  // Y
  std::optional<Ray> GetRay(float PixelFromLeft, float PixelFromTop) const;

  std::optional<Ray> GetRay(const MouseState& mouse) const
  {
    return GetRay(mouse.X, mouse.Y);
  }

  bool InViewport(const MouseState& mouse) const
  {
    if (mouse.X < 0) {
      return false;
    }
    if (mouse.X > Projection.Viewport.Width) {
      return false;
    }
    if (mouse.Y < 0) {
      return false;
    }
    if (mouse.Y > Projection.Viewport.Height) {
      return false;
    }
    return true;
  }
};

} // namespace
} // namespace
