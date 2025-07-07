#pragma once
#include <functional>
#include <optional>
#include <string>
#include <string_view>

namespace grapho {

void
SetErrorMessage(std::string_view msg);

std::string_view
GetErrorMessage();

inline std::string
GetErrorString()
{
  auto msg = GetErrorMessage();
  return { msg.begin(), msg.end() };
}

namespace gl3 {
std::optional<const char*>
TryGetError();

inline void
CheckAndPrintError(const std::function<void(const char*)>& print)
{
  while (auto error = TryGetError()) {
    print(*error);
  }
}

} // namespace
} // namespace
