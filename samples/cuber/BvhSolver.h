#pragma once
#include "Bvh.h"
#include <DirectXMath.h>
#include <list>
#include <memory>
#include <stack>
#include <vector>

struct BvhNode;
class BvhSolver {
  std::vector<std::shared_ptr<BvhNode>> nodes_;
  std::vector<DirectX::XMFLOAT4X4> instances_;

public:
  float scaling_ = 1.0f;
  std::shared_ptr<BvhNode> root_;
  void Initialize(const std::shared_ptr<Bvh> &bvh);
  std::span<DirectX::XMFLOAT4X4> ResolveFrame(const BvhFrame &frame);

private:
  void PushJoint(BvhJoint &joint);
  void CalcShape();
};
