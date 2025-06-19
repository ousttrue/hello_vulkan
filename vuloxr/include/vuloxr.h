#pragma once
#include <memory>
#include <stdarg.h>
#include <stdexcept>
#include <string>
#include <vector>

namespace vuloxr {

inline bool find_name(const std::vector<const char *> &names,
                      const char *target) {
  for (auto name : names) {
    if (strcmp(name, target) == 0) {
      return true;
    }
  }
  return false;
}

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

struct Logger {
  static std::string tag;

#ifdef ANDROID
  static void Info(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_INFO, tag.c_str(), fmt, arg);
    va_end(arg);
  }
  static void Error(const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(ANDROID_LOG_ERROR, tag.c_str(), fmt, arg);
    va_end(arg);
  }
#else
  static void Info(const char *_fmt, ...) {
    auto begin = "\e[0;32m";
    auto end = "\e[0m";

    va_list arg;
    va_start(arg, _fmt);
    auto fmt = (begin + std::string("INFO: ") + _fmt + end);
    if (!fmt.ends_with('\n')) {
      fmt += '\n';
    }
    vfprintf(stderr, fmt.c_str(), arg);
    va_end(arg);
  }
  static void Error(const char *_fmt, ...) {
    auto begin = "\e[0;31m";
    auto end = "\e[0m";

    va_list arg;
    va_start(arg, _fmt);
    auto fmt = (begin + std::string("ERROR: ") + _fmt + end);
    if (!fmt.ends_with('\n')) {
      fmt += '\n';
    }
    vfprintf(stderr, fmt.c_str(), arg);
    va_end(arg);
  }
#endif
};
inline std::string Logger::tag = "vuloxr";

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
