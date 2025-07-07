#include "BvhPanel.h"
#include "Animation.h"
#include "Bvh.h"
#include "BvhNode.h"
#include "BvhSolver.h"
#include "UdpSender.h"
#include <algorithm>
#include <asio.hpp>
#include <asio/ip/address.hpp>
#include <imgui.h>
#include <thread>

struct BvhPanelImpl
{
  asio::io_context io_;
  asio::executor_work_guard<asio::io_context::executor_type> m_work;
  Animation m_animation;
  UdpSender m_sender;
  std::thread m_thread;
  std::shared_ptr<Bvh> m_bvh;
  asio::ip::udp::endpoint m_ep;
  bool m_enablePackQuat = false;
  std::vector<int> m_parentMap;

  std::vector<cuber::Instance> m_instances;
  std::mutex m_mutex;
  BvhSolver m_bvhSolver;

  BvhPanelImpl()
    : m_work(asio::make_work_guard(io_))
    , m_animation(io_)
    , m_sender(io_)
    , m_ep(asio::ip::make_address("127.0.0.1"), 54345)
  {
    m_animation.OnFrame([self = this](const BvhFrame& frame) {
      self->m_sender.SendFrame(
        self->m_ep, self->m_bvh, frame, self->m_enablePackQuat);
    });
    m_thread = std::thread([self = this]() {
      try {
        self->io_.run();
        std::cout << "[asio] end" << std::endl;
      } catch (std::exception const& e) {
        std::cout << "[asio] catch" << e.what() << std::endl;
      }
    });

    // bind bvh animation to renderer
    m_animation.OnFrame(
      [self = this](const BvhFrame& frame) { self->SyncFrame(frame); });
  }

  ~BvhPanelImpl()
  {
    m_animation.Stop();
    m_work.reset();
    m_thread.join();
  }

  void SetBvh(const std::shared_ptr<Bvh>& bvh)
  {
    m_bvh = bvh;
    if (!m_bvh) {
      return;
    }
    m_animation.SetBvh(bvh);
    for (auto& joint : m_bvh->joints) {
      m_parentMap.push_back(joint.parent);
    }
    m_sender.SendSkeleton(m_ep, m_bvh);

    m_bvhSolver.Initialize(bvh);
    m_instances.resize(m_bvh->joints.size());
  }

  void SelectBone(const std::shared_ptr<BvhNode>& node)
  {
    char id[256];
    snprintf(id, sizeof(id), "##%p", node.get());
    if (ImGui::BeginCombo(id,
                          srht::HumanoidBoneNames[(int)node->joint_.bone_])) {
      for (int n = 0; n < (int)srht::HumanoidBones::RIGHT_LITTLE_DISTAL; n++) {
        // ImFont *font = io.Fonts->Fonts[n];
        ImGui::PushID((void*)(uint64_t)n);
        if (ImGui::Selectable(srht::HumanoidBoneNames[n],
                              n == (int)node->joint_.bone_)) {
          node->joint_.bone_ = (srht::HumanoidBones)n;
        }
        ImGui::PopID();
      }
      ImGui::EndCombo();
    }
  }

  void TreeGui(const std::shared_ptr<BvhNode>& node,
               std::span<cuber::Instance>::iterator& it)
  {
    ImGui::TableNextRow();
    ImGui::TableNextColumn();
    const bool is_folder = (node->children_.size() > 0);

    ImGui::SetNextItemOpen(true, ImGuiCond_Once);
    bool open = false;
    if (is_folder) {
      open = ImGui::TreeNodeEx(node->joint_.name.c_str(),
                               ImGuiTreeNodeFlags_SpanFullWidth);
    } else {
      ImGui::TreeNodeEx(node->joint_.name.c_str(),
                        ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_Bullet |
                          ImGuiTreeNodeFlags_NoTreePushOnOpen |
                          ImGuiTreeNodeFlags_SpanFullWidth);
    }

    ImGui::TableNextColumn();
    SelectBone(node);

    // color picker
    // ImGui::TableNextColumn();
    // ImGui::ColorEdit4("##color",
    //                   &it->Color.x,
    //                   ImGuiColorEditFlags_NoInputs |
    //                     ImGuiColorEditFlags_NoLabel);

    if (open) {
      for (auto& child : node->children_) {
        TreeGui(child, ++it);
      }
      ImGui::TreePop();
    }
  }

  void UpdateGui()
  {
    if (!m_bvh) {
      return;
    }
    // bvh panel
    ImGui::Begin("BVH");

    ImGui::LabelText("bvh", "%zu joints", m_bvh->joints.size());

    ImGui::Checkbox("use quaternion pack32", &m_enablePackQuat);

    if (ImGui::Button("send skeleton")) {
      m_sender.SendSkeleton(m_ep, m_bvh);
    }

    // TREE
    // NAME, BONETYPE, COLOR
    static ImGuiTableFlags flags =
      ImGuiTableFlags_RowBg | ImGuiTableFlags_Borders;
    if (ImGui::BeginTable("tree", 3, flags)) {
      ImGui::TableSetupColumn("Name");
      ImGui::TableSetupColumn("HumanBone");
      ImGui::TableSetupColumn("Color");
      // ImGui::TableSetupScrollFreeze(0, 1);
      ImGui::TableHeadersRow();
      std::span<cuber::Instance> span{ m_instances.begin(), m_instances.end() };
      auto it = span.begin();
      TreeGui(m_bvhSolver.root_, it);
      ImGui::EndTable();
    }

    ImGui::End();
  }

  void SyncFrame(const BvhFrame& frame)
  {
    auto instances = m_bvhSolver.ResolveFrame(frame);
    {
      std::lock_guard<std::mutex> lock(m_mutex);
      m_instances.resize(instances.size());
      for (size_t i = 0; i < m_instances.size(); ++i) {
        m_instances[i] = { .Matrix = instances[i] };
      }
    }
  }

  std::span<const cuber::Instance> GetCubes()
  {
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_instances;
  }

  void GetCubes(std::vector<cuber::Instance>& cubes)
  {
    cubes.assign(m_instances.begin(), m_instances.end());
  }
};

BvhPanel::BvhPanel()
  : m_impl(new BvhPanelImpl)
{
}
BvhPanel::~BvhPanel()
{
  delete m_impl;
}
void
BvhPanel::SetBvh(const std::shared_ptr<Bvh>& bvh)
{
  m_impl->SetBvh(bvh);
}
void
BvhPanel::UpdateGui()
{
  m_impl->UpdateGui();
}
std::span<const cuber::Instance>
BvhPanel::GetCubes()
{
  return m_impl->GetCubes();
}

void
BvhPanel::GetCubes(std::vector<cuber::Instance>& cubes)
{
  m_impl->GetCubes(cubes);
}
