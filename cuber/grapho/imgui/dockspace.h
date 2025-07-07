#pragma once
#include <functional>
#include <imgui_internal.h>
#include <optional>
#include <span>
#include <string>
#include <variant>
#include <vector>

namespace grapho {
namespace imgui {

struct NextWindowFlag
{
  ImVec2 Value;
  int Flag;
};
struct GuiStyleVar
{
  ImGuiStyleVar Flag;
  ImVec2 Value;
};

struct Dock
{
  std::string Name;
  std::function<void()> OnShow;
  ImGuiWindowFlags Flags = 0;
  std::function<bool(const char*, bool*, ImGuiWindowFlags)> Begin =
    [](auto title, auto popen, auto flags) {
      return ImGui::Begin(title, popen, flags);
    };
  std::function<void()> End = []() { ImGui::End(); };
  bool IsOpen = true;
  std::vector<GuiStyleVar> StyleVars;
  std::optional<NextWindowFlag> NextWindowSize;

  void NoPadding()
  {
    StyleVars.push_back({ ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f) });
  }

  Dock& NoScrollBar()
  {
    Flags |=
      (ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    return *this;
  }

  Dock& DefaultSize(int w, int h)
  {
    NextWindowSize = NextWindowFlag{
      ImVec2{ (float)300, (float)200 },
      ImGuiCond_FirstUseEver,
    };
    return *this;
  }

  void Show()
  {
    if (IsOpen) {
      // begin
      for (auto& style : StyleVars) {
        ImGui::PushStyleVar(style.Flag, style.Value);
      }
      auto isOpen = false;
      if (Begin) {
        if (NextWindowSize) {
          ImGui::SetNextWindowSize(NextWindowSize->Value, NextWindowSize->Flag);
        } else {
          ImGui::SetNextWindowSize({ 200.0f, 200.0f }, ImGuiCond_FirstUseEver);
        }
        isOpen = Begin(Name.c_str(), &IsOpen, Flags);
      }
      ImGui::PopStyleVar(StyleVars.size());
      if (isOpen && OnShow) {
        OnShow();
      }
      if (End) {
        End();
      }
    }
  }
};

inline void
DockSpaceLayout(const char* dock_space,
                const std::function<void()>& layout = {})
{
  ImGuiIO& io = ImGui::GetIO();
  assert(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
  ImGuiID dockspace_id = ImGui::GetID(dock_space);

  // remove root
  ImGui::DockBuilderRemoveNode(dockspace_id);
  // add root
  ImGui::DockBuilderAddNode(dockspace_id);

  // Build dockspace...
  if (layout) {
    layout();
  }

  ImGui::DockBuilderFinish(dockspace_id);
}

inline void
BeginDockSpace(const char* dock_space)
{
  // If you strip some features of, this demo is pretty much equivalent to
  // calling DockSpaceOverViewport()! In most cases you should be able to just
  // call DockSpaceOverViewport() and ignore all the code below! In this
  // specific demo, we are not using DockSpaceOverViewport() because:
  // - we allow the host window to be floating/moveable instead of filling the
  // viewport (when opt_fullscreen == false)
  // - we allow the host window to have padding (when opt_padding == true)
  // - we have a local  bar in the host window (vs. you could use
  // BeginMainMenuBar() + DockSpaceOverViewport() in your code!) TL;DR; this
  // demo is more complicated than what you would normally use. If we removed
  // all the options we are showcasing, this demo would become:
  //     void ShowExampleAppDockSpace()
  //     {
  //         ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
  //     }

  static bool opt_fullscreen = true;
  static bool opt_padding = false;
  static ImGuiDockNodeFlags dockspace_flags =
    ImGuiDockNodeFlags_PassthruCentralNode;

  // We are using the ImGuiWindowFlags_NoDocking flag to make the parent window
  // not dockable into, because it would be confusing to have two docking
  // targets within each others.
  ImGuiWindowFlags window_flags =
    ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
  if (opt_fullscreen) {
    const ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::SetNextWindowViewport(viewport->ID);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
    ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
    window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse |
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    window_flags |=
      ImGuiWindowFlags_NoBringToFrontOnFocus | ImGuiWindowFlags_NoNavFocus;
  } else {
    dockspace_flags &= ~ImGuiDockNodeFlags_PassthruCentralNode;
  }

  // When using ImGuiDockNodeFlags_PassthruCentralNode, DockSpace() will render
  // our background and handle the pass-thru hole, so we ask Begin() to not
  // render a background.
  if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
    window_flags |= ImGuiWindowFlags_NoBackground;

  // Important: note that we proceed even if Begin() returns false (aka window
  // is collapsed). This is because we want to keep our DockSpace() active. If a
  // DockSpace() is inactive, all active windows docked into it will lose their
  // parent and become undocked. We cannot preserve the docking relationship
  // between an active window and an inactive docking, otherwise any change of
  // dockspace/settings would lead to windows being stuck in limbo and never
  // being visible.
  if (!opt_padding)
    ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
  ImGui::Begin(dock_space, nullptr, window_flags);
  if (!opt_padding)
    ImGui::PopStyleVar();

  if (opt_fullscreen)
    ImGui::PopStyleVar(2);

  // Submit the DockSpace
  ImGuiIO& io = ImGui::GetIO();
  assert(io.ConfigFlags & ImGuiConfigFlags_DockingEnable);
  ImGuiID dockspace_id = ImGui::GetID(dock_space);

  ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
}

} // namespace
} // namespace
