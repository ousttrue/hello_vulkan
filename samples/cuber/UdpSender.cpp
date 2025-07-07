#include <DirectXMath.h>

#include "UdpSender.h"
// #include "../../subprojects/meshutils/Source/MeshUtils/muQuat32.h"
#include "muQuat32.h"
#include "Bvh.h"
#include "Payload.h"
#include <DirectXMath.h>
#include <iostream>

UdpSender::UdpSender(asio::io_context &io)
    : socket_(io, asio::ip::udp::endpoint(asio::ip::udp::v4(), 0)) {}

std::shared_ptr<Payload> UdpSender::GetOrCreatePayload() {
  std::lock_guard<std::mutex> lock(mutex_);
  if (payloads_.empty()) {
    return std::make_shared<Payload>();
  } else {
    auto payload = payloads_.front();
    payloads_.pop_front();
    return payload;
  }
}

void UdpSender::ReleasePayload(const std::shared_ptr<Payload> &payload) {
  std::lock_guard<std::mutex> lock(mutex_);
  payloads_.push_back(payload);
}

void UdpSender::SendSkeleton(asio::ip::udp::endpoint ep,
                             const std::shared_ptr<Bvh> &bvh) {
  auto payload = GetOrCreatePayload();
  joints_.clear();
  auto scaling = bvh->GuessScaling();
  for (auto joint : bvh->joints) {
    joints_.push_back({
        .parentBoneIndex = joint.parent,
        .boneType = (uint16_t)joint.bone_,
        .xFromParent = joint.localOffset.x * scaling,
        .yFromParent = joint.localOffset.y * scaling,
        .zFromParent = joint.localOffset.z * scaling,
    });
  }
  payload->SetSkeleton(joints_);

  socket_.async_send_to(asio::buffer(payload->buffer), ep,
                        [self = this, payload](asio::error_code ec,
                                               std::size_t bytes_transferred) {
                          self->ReleasePayload(payload);
                        });
}

static DirectX::XMFLOAT4X4 ToMat(const BvhMat3 &rot, const BvhOffset &pos,
                                 float scaling) {

  auto r = DirectX::XMLoadFloat3x3((const DirectX::XMFLOAT3X3 *)&rot);
  auto t = DirectX::XMMatrixTranslation(pos.x * scaling, pos.y * scaling,
                                        pos.z * scaling);
  auto m = r * t;
  DirectX::XMFLOAT4X4 mat;
  DirectX::XMStoreFloat4x4(&mat, m);
  return mat;
}

static DirectX::XMFLOAT4 ToQuat(const BvhMat3 &rot) {

  auto r = DirectX::XMLoadFloat3x3((const DirectX::XMFLOAT3X3 *)&rot);
  auto q = DirectX::XMQuaternionRotationMatrix(r);
  DirectX::XMFLOAT4 vec4;
  DirectX::XMStoreFloat4(&vec4, q);
  return vec4;
}

void UdpSender::SendFrame(asio::ip::udp::endpoint ep,
                          const std::shared_ptr<Bvh> &bvh,
                          const BvhFrame &frame, bool pack) {

  auto payload = GetOrCreatePayload();

  auto scaling = bvh->GuessScaling();
  for (auto &joint : bvh->joints) {
    auto [pos, rot] = frame.Resolve(joint.channels);
    DirectX::XMFLOAT4 rotation;
    DirectX::XMStoreFloat4(&rotation, DirectX::XMQuaternionRotationMatrix(rot));

    if (joint.index == 0) {
      payload->SetFrame(
          std::chrono::duration_cast<std::chrono::nanoseconds>(frame.time),
          pos.x * scaling, pos.y * scaling, pos.z * scaling, pack);
    }
    if (pack) {
      auto packed =
          quat_packer::Pack(rotation.x, rotation.y, rotation.z, rotation.w);
#if _DEBUG
      {
        mu::quatf debug_q{rotation.x, rotation.y, rotation.z, rotation.w};
        auto debug_packed = mu::quat32(debug_q);
        assert(*(uint32_t *)&debug_packed.value == packed);
      }
      payload->Push(packed);
#endif
    } else {
      payload->Push(rotation);
    }
  }

  socket_.async_send_to(asio::buffer(payload->buffer), ep,
                        [self = this, payload](asio::error_code ec,
                                               std::size_t bytes_transferred) {
                          self->ReleasePayload(payload);
                        });
}
