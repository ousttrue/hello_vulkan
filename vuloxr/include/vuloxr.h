#pragma once
#include <memory>
#include <stdarg.h>
#include <stdexcept>
#include <string>
#include <vector>
#ifdef ANDROID
#include <android/log.h>
#else
#include <stdarg.h>
#endif

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
  template <typename... ARGS> static void Log(int prio, const char *fmt, ...) {
    va_list arg;
    va_start(arg, fmt);
    __android_log_vprint(prio, tag.c_str(), fmt, arg);
    va_end(arg);
  }
  template <typename... ARGS>
  static void Verbose(const char *fmt, ARGS... args) {
    Log(ANDROID_LOG_VERBOSE, fmt, args...);
  }
  template <typename... ARGS> static void Info(const char *fmt, ARGS... args) {
    Log(ANDROID_LOG_INFO, fmt, args...);
  }
  template <typename... ARGS> static void Warn(const char *fmt, ARGS... args) {
    Log(ANDROID_LOG_WARN, fmt, args...);
  }
  template <typename... ARGS> static void Error(const char *fmt, ARGS... args) {
    Log(ANDROID_LOG_ERROR, fmt, args...);
  }
#else

  template <typename... ARGS>
  static void Log(const std::string &begin, const char *_fmt, ...) {
    auto end = "\e[0m";
    va_list arg;
    va_start(arg, _fmt);
    auto fmt = (begin + _fmt + end);
    if (!fmt.ends_with('\n')) {
      fmt += '\n';
    }
    vfprintf(stderr, fmt.c_str(), arg);
    va_end(arg);
  }
  template <typename... ARGS>
  static void Verbose(const char *fmt, ARGS... args) {
    Log("\e[0m" // default
        "VERBOSE: ",
        fmt, args...);
  }
  template <typename... ARGS> static void Info(const char *fmt, ARGS... args) {
    Log("\e[0;32m" // green
        "INFO   : ",
        fmt, args...);
  }
  template <typename... ARGS> static void Warn(const char *fmt, ARGS... args) {
    Log("\e[0;33m" // yellow
        "WARN   : ",
        fmt, args...);
  }
  template <typename... ARGS> static void Error(const char *fmt, ARGS... args) {
    Log("\e[0;31m" // red
        "ERROR  : ",
        fmt, args...);
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
