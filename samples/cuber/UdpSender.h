#pragma once
#include "Bvh.h"
#include "Payload.h"
#include "srht.h"
#include <asio.hpp>
#include <list>
#include <memory>
#include <mutex>
#include <span>

class UdpSender {
  asio::ip::udp::socket socket_;
  std::list<std::shared_ptr<Payload>> payloads_;
  std::mutex mutex_;
  std::vector<srht::JointDefinition> joints_;

public:
  UdpSender(asio::io_context &io);
  std::shared_ptr<Payload> GetOrCreatePayload();
  void ReleasePayload(const std::shared_ptr<Payload> &payload);
  void SendSkeleton(asio::ip::udp::endpoint ep,
                    const std::shared_ptr<Bvh> &bvh);
  void SendFrame(asio::ip::udp::endpoint ep, const std::shared_ptr<Bvh> &bvh,
                 const BvhFrame &frame, bool pack);
};
