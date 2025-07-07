#include "camera.h"
#include <DirectXMath.h>
#include <algorithm>
#include <math.h>
#include <numbers>

namespace grapho {
namespace camera {

struct EuclideanTransform
{
  DirectX::XMFLOAT4 Rotation = { 0, 0, 0, 1 };
  DirectX::XMFLOAT3 Translation = { 0, 0, 0 };

  static EuclideanTransform Store(DirectX::XMVECTOR r, DirectX::XMVECTOR t)
  {
    EuclideanTransform transform;
    DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&transform.Rotation, r);
    DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&transform.Translation, t);
    return transform;
  }

  bool HasRotation() const
  {
    if (Rotation.x == 0 && Rotation.y == 0 && Rotation.z == 0 &&
        (Rotation.w == 1 || Rotation.w == -1)) {
      return false;
    }
    return true;
  }

  DirectX::XMMATRIX ScalingTranslationMatrix(float scaling) const
  {
    auto r = DirectX::XMMatrixRotationQuaternion(
      DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation));
    auto t = DirectX::XMMatrixTranslation(Translation.x * scaling,
                                          Translation.y * scaling,
                                          Translation.z * scaling);
    return r * t;
  }

  DirectX::XMMATRIX Matrix() const
  {
    auto r = DirectX::XMMatrixRotationQuaternion(
      DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation));
    auto t =
      DirectX::XMMatrixTranslation(Translation.x, Translation.y, Translation.z);
    return r * t;
  }

  EuclideanTransform& SetMatrix(DirectX::XMMATRIX m)
  {
    DirectX::XMVECTOR s;
    DirectX::XMVECTOR r;
    DirectX::XMVECTOR t;
    if (!DirectX::XMMatrixDecompose(&s, &r, &t, m)) {
      assert(false);
    }
    // DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&InitialScale, s);
    DirectX::XMStoreFloat4((DirectX::XMFLOAT4*)&Rotation, r);
    DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&Translation, t);
    return *this;
  }

  DirectX::XMMATRIX InversedMatrix() const
  {
    auto r = DirectX::XMMatrixRotationQuaternion(DirectX::XMQuaternionInverse(
      DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation)));
    auto t = DirectX::XMMatrixTranslation(
      -Translation.x, -Translation.y, -Translation.z);
    return t * r;
  }

  EuclideanTransform Invrsed() const
  {
    auto r = DirectX::XMQuaternionInverse(
      DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation));
    auto t = DirectX::XMVector3Rotate(
      DirectX::XMVectorSet(-Translation.x, -Translation.y, -Translation.z, 1),
      r);
    return Store(r, t);
  }

  EuclideanTransform Rotate(DirectX::XMVECTOR r)
  {
    return EuclideanTransform::Store(
      DirectX::XMQuaternionMultiply(
        DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation), r),
      DirectX::XMVector3Transform(
        DirectX::XMLoadFloat3((const DirectX::XMFLOAT3*)&Translation),
        DirectX::XMMatrixRotationQuaternion(r)));
  }
};

Projection::Projection()
{
  FovY = DirectX::XMConvertToRadians(30.0f);
}

void
Projection::Update(DirectX::XMFLOAT4X4* projection)
{
  auto aspectRatio = Viewport.AspectRatio();
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)projection,
    DirectX::XMMatrixPerspectiveFovRH(FovY, aspectRatio, NearZ, FarZ));
}

void
Camera::YawPitch(int dx, int dy)
{
  const EuclideanTransform Transform{ Rotation, Translation };
  auto inv = Transform.Invrsed();
  auto _m = DirectX::XMMatrixRotationQuaternion(
    DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Transform.Rotation));
  DirectX::XMFLOAT4X4 m;
  DirectX::XMStoreFloat4x4(&m, _m);

  auto x = m._31;
  auto y = m._32;
  auto z = m._33;

  auto yaw = atan2(x, z) - DirectX::XMConvertToRadians(static_cast<float>(dx));
  TmpYaw = yaw;
  auto qYaw =
    DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(0, 1, 0, 0), yaw);

  auto half_pi = static_cast<float>(std::numbers::pi / 2) - 0.01f;
  auto pitch = std::clamp(atan2(y, sqrt(x * x + z * z)) +
                            DirectX::XMConvertToRadians(static_cast<float>(dy)),
                          -half_pi,
                          half_pi);
  TmpPitch = pitch;
  auto qPitch =
    DirectX::XMQuaternionRotationAxis(DirectX::XMVectorSet(-1, 0, 0, 0), pitch);

  auto q =
    DirectX::XMQuaternionInverse(DirectX::XMQuaternionMultiply(qPitch, qYaw));
  auto et = EuclideanTransform::Store(
    q, DirectX::XMLoadFloat3((const DirectX::XMFLOAT3*)&inv.Translation));

  auto dst = et.Invrsed();
  Rotation = dst.Rotation;
  Translation = dst.Translation;
}

void
Camera::Shift(int dx, int dy)
{
  const EuclideanTransform Transform{ Rotation, Translation };
  auto factor = std::tan(Projection.FovY * 0.5f) * 2.0f * GazeDistance /
                Projection.Viewport.Height;

  auto _m = DirectX::XMMatrixRotationQuaternion(
    DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Transform.Rotation));
  DirectX::XMFLOAT4X4 m;
  DirectX::XMStoreFloat4x4(&m, _m);

  auto left_x = m._11;
  auto left_y = m._12;
  auto left_z = m._13;
  auto up_x = m._21;
  auto up_y = m._22;
  auto up_z = m._23;
  Translation.x += (-left_x * dx + up_x * dy) * factor;
  Translation.y += (-left_y * dx + up_y * dy) * factor;
  Translation.z += (-left_z * dx + up_z * dy) * factor;
}

void
Camera::Dolly(int d)
{
  if (d == 0) {
    return;
  }

  const EuclideanTransform Transform{ Rotation, Translation };
  auto _m = DirectX::XMMatrixRotationQuaternion(
    DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Transform.Rotation));
  DirectX::XMFLOAT4X4 m;
  DirectX::XMStoreFloat4x4(&m, _m);
  auto x = m._31;
  auto y = m._32;
  auto z = m._33;
  DirectX::XMFLOAT3 Gaze{
    Transform.Translation.x - x * GazeDistance,
    Transform.Translation.y - y * GazeDistance,
    Transform.Translation.z - z * GazeDistance,
  };
  if (d > 0) {
    GazeDistance *= 0.9f;
  } else {
    GazeDistance *= 1.1f;
  }
  Translation.x = Gaze.x + x * GazeDistance;
  Translation.y = Gaze.y + y * GazeDistance;
  Translation.z = Gaze.z + z * GazeDistance;
}

void
Camera::Update()
{
  const EuclideanTransform Transform{ Rotation, Translation };
  Projection.Update(&ProjectionMatrix);
  DirectX::XMStoreFloat4x4((DirectX::XMFLOAT4X4*)&ViewMatrix,
                           Transform.InversedMatrix());
}

DirectX::XMFLOAT4X4
Camera::ViewProjection() const
{
  DirectX::XMFLOAT4X4 m;
  DirectX::XMStoreFloat4x4(
    (DirectX::XMFLOAT4X4*)&m,
    DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&ViewMatrix) *
      DirectX::XMLoadFloat4x4((const DirectX::XMFLOAT4X4*)&ProjectionMatrix));
  return m;
}

void
Camera::Fit(const DirectX::XMFLOAT3& min, const DirectX::XMFLOAT3& max)
{
  // Yaw = {};
  // Pitch = {};
  Rotation = { 0, 0, 0, 1 };
  auto height = max.y - min.y;
  if (fabs(height) < 1e-4) {
    return;
  }
  auto distance = height * 0.5f / std::atan(Projection.FovY * 0.5f);
  Translation.x = (max.x + min.x) * 0.5f;
  Translation.y = (max.y + min.y) * 0.5f;
  Translation.z = distance * 1.2f;
  GazeDistance = Translation.z;
  auto r =
    DirectX::XMVectorGetX(DirectX::XMVector3Length(DirectX::XMVectorSubtract(
      DirectX::XMLoadFloat3((const DirectX::XMFLOAT3*)&min),
      DirectX::XMLoadFloat3((const DirectX::XMFLOAT3*)&max))));
  Projection.NearZ = r * 0.01f;
  Projection.FarZ = r * 100.0f;
}

// 0-> X
// |
// v
//
// Y
std::optional<Ray>
Camera::GetRay(float PixelFromLeft, float PixelFromTop) const
{
  Ray ret{
    Translation,
  };

  auto t = tan(Projection.FovY / 2);
  auto h = Projection.Viewport.Height / 2;
  auto y = t * (h - PixelFromTop) / h;
  auto w = Projection.Viewport.Width / 2;
  auto x = t * Projection.Viewport.AspectRatio() * (PixelFromLeft - w) / w;

  auto q = DirectX::XMLoadFloat4((const DirectX::XMFLOAT4*)&Rotation);
  DirectX::XMStoreFloat3((DirectX::XMFLOAT3*)&ret.Direction,
                         DirectX::XMVector3Normalize(DirectX::XMVector3Rotate(
                           DirectX::XMVectorSet(x, y, -1, 0), q)));

  if (!ret.IsValid()) {
    return std::nullopt;
  }
  return ret;
}

} // namespace
}
