#include <DirectXMath.h>

#include "ray.h"

namespace grapho {
namespace camera {

Ray Ray::Transform(const DirectX::XMMATRIX &m) const {
  Ray ray;
  DirectX::XMStoreFloat3(&ray.Origin, DirectX::XMVector3Transform(
                                          DirectX::XMLoadFloat3(&Origin), m));
  DirectX::XMStoreFloat3(
      &ray.Direction,
      DirectX::XMVector3Normalize(DirectX::XMVector3TransformNormal(
          DirectX::XMLoadFloat3(&Direction), m)));
  return ray;
}

} // namespace camera
} // namespace grapho
