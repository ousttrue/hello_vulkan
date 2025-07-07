#pragma once
#include <functional>
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif
#include <imgui.h>
#include <imgui_internal.h>
#include <optional>
#include <string>
#include <list>

namespace grapho {
namespace imgui {

struct CursorState
{
  bool IsActive;
  bool IsHovered;
};

inline CursorState
DraggableImage(ImTextureID texture, const ImVec2& size)
{
  // image button. capture mouse event
  ImGui::ImageButton(
    texture, size, { 0, 1 }, { 1, 0 }, 0, { 1, 1, 1, 1 }, { 1, 1, 1, 1 });
  ImGui::ButtonBehavior(ImGui::GetCurrentContext()->LastItemData.Rect,
                        ImGui::GetCurrentContext()->LastItemData.ID,
                        nullptr,
                        nullptr,
                        ImGuiButtonFlags_MouseButtonMiddle |
                          ImGuiButtonFlags_MouseButtonRight);

  return {
    ImGui::IsItemActive(),
    ImGui::IsItemHovered(),
  };
}

// https://github.com/ocornut/imgui/issues/319
//
// [usage]
// auto size = ImGui::GetContentRegionAvail();
// static float sz1 = 300;
// float sz2 = size.x - sz1 - 5;
// grapho::imgui::Splitter(true, 5.0f, &sz1, &sz2, 100, 100);
// ImGui::BeginChild("1", ImVec2(sz1, -1), true);
// ImGui::EndChild();
// ImGui::SameLine();
// ImGui::BeginChild("2", ImVec2(sz2, -1), true);
// ImGui::EndChild();
inline bool
Splitter(bool split_vertically,
         float thickness,
         float* size1,
         float* size2,
         float min_size1,
         float min_size2,
         float splitter_long_axis_size = -1.0f)
{
  using namespace ImGui;
  ImGuiContext& g = *GImGui;
  ImGuiWindow* window = g.CurrentWindow;
  ImGuiID id = window->GetID("##Splitter");
  ImRect bb;
  bb.Min = window->DC.CursorPos +
           (split_vertically ? ImVec2(*size1, 0.0f) : ImVec2(0.0f, *size1));
  bb.Max = bb.Min + CalcItemSize(split_vertically
                                   ? ImVec2(thickness, splitter_long_axis_size)
                                   : ImVec2(splitter_long_axis_size, thickness),
                                 0.0f,
                                 0.0f);
  return SplitterBehavior(bb,
                          id,
                          split_vertically ? ImGuiAxis_X : ImGuiAxis_Y,
                          size1,
                          size2,
                          min_size1,
                          min_size2,
                          0.0f);
}

struct SplitterObject
{
  std::optional<float> Size;

  // +-+
  // +-+
  // +-+
  std::tuple<float, float> SplitHorizontal(const ImVec2& size,
                                           float ratio = 0.5f)
  {
    if (!Size) {
      Size = size.y * ratio;
    }
    float sz1 = *Size;
    float sz2 = size.y - sz1 - 5;
    grapho::imgui::Splitter(false, 5.0f, &sz1, &sz2, 100, 100);
    Size = sz1;
    return { sz1, sz2 };
  }

  // +-+-+
  // +-+-+
  std::tuple<float, float> SplitVertical(const ImVec2& size, float ratio = 0.5f)
  {
    if (!Size) {
      Size = size.x * 0.5f;
    }
    float sz1 = *Size;
    float sz2 = size.x - sz1 - 5;
    grapho::imgui::Splitter(false, 5.0f, &sz1, &sz2, 100, 100);
    Size = sz1;
    return { sz1, sz2 };
  }
};

class TreeSplitter
{
public:
  struct UI
  {
    std::string Label;
    std::function<void()> Show;
    std::function<void()> Text;
    std::list<UI> Children;

    void ShowSelector(const UI* selected, UI** clicked, int level = 0)
    {
      static ImGuiTreeNodeFlags base_flags =
        ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_OpenOnDoubleClick |
        ImGuiTreeNodeFlags_SpanAvailWidth;

      ImGuiTreeNodeFlags node_flags = base_flags;
      if (this == selected) {
        node_flags |= ImGuiTreeNodeFlags_Selected;
      }
      bool is_leaf = Children.empty();
      if (is_leaf) {
        node_flags |= ImGuiTreeNodeFlags_Leaf;
      }

      bool node_open =
        ImGui::TreeNodeEx((void*)this, node_flags, "%s", Label.c_str());

      if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
        *clicked = this;
      }

      if (node_open) {
        for (auto& child : Children) {
          child.ShowSelector(selected, clicked, level + 1);
        }
        ImGui::TreePop();
      }
    }
  };

private:
  std::list<UI> m_list;
  UI* m_selected = nullptr;
  float m_splitter = 200.0f;

public:
  void Clear()
  {
    m_list.clear();
    m_selected = nullptr;
  }

  UI* Push(const char* label,
           UI* parent = nullptr,
           const std::function<void()>& callback = {},
           const std::function<void()>& text = {})
  {
    auto& list = parent ? parent->Children : m_list;
    list.push_back({
      .Label = label,
      .Show = callback,
      .Text = text,
    });
    return &list.back();
  }

  void ShowGui()
  {
    // auto size = ImGui::GetCurrentWindow()->Size;
    auto size = ImGui::GetContentRegionAvail();
    float s = size.x - m_splitter - 5;
    // ImGui::Text("%f, %f: %f; %f", size.x, size.y, f, s);
    grapho::imgui::Splitter(true, 5, &m_splitter, &s, 8, 8);

    ImGui::BeginChild("left pane", ImVec2(m_splitter, 0), true);
    ShowSelector();
    ImGui::EndChild();

    ImGui::SameLine();
    ImGui::BeginChild("item view", ImVec2(0, 0));
    ShowSelected();
    ImGui::EndChild();
  }

  void ShowSelector()
  {
    ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 5);
    UI* clicked = nullptr;
    for (auto& ui : m_list) {
      ui.ShowSelector(m_selected, &clicked);
    }
    if (clicked) {
      m_selected = clicked;
    }
    ImGui::PopStyleVar();
  }

  void ShowSelected()
  {
    if (m_selected && m_selected->Show) {
      m_selected->Show();
    }
  }

  void ShowText()
  {
    if (m_selected && m_selected->Text) {
      m_selected->Text();
    }
  }
};

template<typename T>
static bool
EnumCombo(const char* label,
          T* value,
          SpanModoki<const std::tuple<T, const char*>> span)
{
  using TUPLE = std::tuple<T, const char*>;
  int i = 0;
  for (; i < span.size(); ++i) {
    if (std::get<0>(span[i]) == *value) {
      break;
    }
  }

  auto callback = [](void* data, int n, const char** out_str) -> bool {
    auto span = (std::span<const TUPLE>*)data;
    if (n < (*span).size()) {
      *out_str = std::get<1>((*span)[n]);
      return true;
    }
    return false;
  };

  bool updated = ImGui::Combo(label, &i, callback, (void*)&span, span.size());
  if (updated) {
    *value = std::get<0>(span[i]);
  }
  return updated;
}

template<typename T>
static bool
GenericCombo(const char* label,
             T* value,
             std::span<const std::tuple<T, std::string>> span)
{
  using TUPLE = std::tuple<T, std::string>;
  using SPAN = std::span<const TUPLE>;
  int i = 0;
  for (; i < span.size(); ++i) {
    if (std::get<0>(span[i]) == *value) {
      break;
    }
  }

  auto callback = [](void* data, int n, const char** out_str) -> bool {
    auto span = (const SPAN*)data;
    if (n < span->size()) {
      auto& tuple = (*span)[n];
      *out_str = std::get<1>(tuple).c_str();
      return true;
    }
    return false;
  };
  if (ImGui::Combo(label, &i, callback, (void*)&span, span.size())) {
    *value = std::get<0>(span[i]);
    return true;
  }
  return false;
}

inline bool
BeginTableColumns(const char* title,
                  std::span<const char*> cols,
                  const ImVec2& size = { 0, 0 })
{
  static ImGuiTableFlags flags =
    ImGuiTableFlags_Resizable | ImGuiTableFlags_RowBg |
    ImGuiTableFlags_Borders | ImGuiTableFlags_NoBordersInBody |
    ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY |
    ImGuiTableFlags_SizingFixedFit;

  if (ImGui::BeginTable(title, cols.size(), flags, size)) {
    for (auto col : cols) {
      ImGui::TableSetupColumn(col);
    }
    ImGui::TableSetupScrollFreeze(0, 1);
    ImGui::TableHeadersRow();
    return true;
  }

  return false;
}

template<typename T>
std::optional<T>
SelectVector(const std::vector<T>& nodes,
             const T& current,
             const std::function<const char*(const T&)>& toLabel)
{
  std::optional<T> selected;
  ImGui::SetNextItemWidth(-1);
  if (ImGui::BeginCombo("##_SelectNode", toLabel(current))) {
    for (auto& node : nodes) {
      bool is_selected = (current == node);
      if (ImGui::Selectable(toLabel(node), is_selected)) {
        selected = node;
      }
      if (is_selected) {
        // Set the initial focus when opening the combo (scrolling + for
        // keyboard navigation support in the upcoming navigation branch)
        ImGui::SetItemDefaultFocus();
      }
    }
    ImGui::EndCombo();
  }
  return selected;
}

}
}
