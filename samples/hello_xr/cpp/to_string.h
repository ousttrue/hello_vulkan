#pragma once
#include <openxr/openxr_reflection.h>
#include <stdarg.h>
#include <string>
#include <stdexcept>
#include <memory>

// Macro to generate stringify functions for OpenXR enumerations based data
// provided in openxr_reflection.h
// clang-format off
#define ENUM_CASE_STR(name, val) case name: return #name;
#define MAKE_TO_STRING_FUNC(enumType)                  \
    inline const char* to_string(enumType e) {         \
        switch (e) {                                   \
            XR_LIST_ENUM_##enumType(ENUM_CASE_STR)     \
            default: return "Unknown " #enumType;      \
        }                                              \
    }
// clang-format on

MAKE_TO_STRING_FUNC(XrReferenceSpaceType);
MAKE_TO_STRING_FUNC(XrViewConfigurationType);
MAKE_TO_STRING_FUNC(XrEnvironmentBlendMode);
MAKE_TO_STRING_FUNC(XrSessionState);
MAKE_TO_STRING_FUNC(XrResult);
MAKE_TO_STRING_FUNC(XrFormFactor);

inline std::string Fmt(const char *fmt, ...) {
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
    failureMessage += Fmt("\n    Origin: %s", originator);
  }
  if (sourceLocation != nullptr) {
    failureMessage += Fmt("\n    Source: %s", sourceLocation);
  }

  throw std::logic_error(failureMessage);
}

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  Throw(Fmt("XrResult failure [%s]", to_string(res)), originator,
        sourceLocation);
}

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}
