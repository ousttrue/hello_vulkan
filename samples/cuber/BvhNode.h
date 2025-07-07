#pragma once
#include "Bvh.h"
#include <list>
#include <memory>
#include <vector>

struct BvhNode {
  BvhJoint &joint_;
  DirectX::XMFLOAT4X4 shape_;
  std::list<std::shared_ptr<BvhNode>> children_;
  BvhNode(BvhJoint &joint);
  void AddChild(const std::shared_ptr<BvhNode> &node) {
    children_.push_back(node);
  }
  void CalcShape(float scaling);
  void ResolveFrame(const BvhFrame &frame, DirectX::XMMATRIX m, float scaling,
                    std::span<DirectX::XMFLOAT4X4>::iterator &out);
};
