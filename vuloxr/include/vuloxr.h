#pragma once
#include <memory>
#include <stdarg.h>
#include <stdexcept>
#include <string>

namespace vuloxr {

inline std::string fmt(const char *fmt, ...) {
  va_list vl;
  va_start(vl, fmt);
  int size = std::vsnprintf(nullptr, 0, fmt, vl);
  va_end(vl);

  if (size != -1) {
    std::unique_ptr<char[]> buffer(new char[size + 1]);

    va_start(vl, fmt);
    size = std::vsnprintf(buffer.get(), size + 1, fmt, vl);
    va_end(vl);
    if (size != -1) {
      return std::string(buffer.get(), size);
    }
  }

  throw std::runtime_error("Unexpected vsnprintf failure");
}

[[noreturn]] inline void Throw(std::string failureMessage,
                               const char *originator = nullptr,
                               const char *sourceLocation = nullptr) {
  if (originator != nullptr) {
    failureMessage += fmt("\n    Origin: %s", originator);
  }
  if (sourceLocation != nullptr) {
    failureMessage += fmt("\n    Source: %s", sourceLocation);
  }

  throw std::logic_error(failureMessage);
}

struct NonCopyable {
  NonCopyable() = default;
  ~NonCopyable() = default;
  NonCopyable(const NonCopyable &) = delete;
  NonCopyable &operator=(const NonCopyable &) = delete;
};

} // namespace vuloxr
