#include <common/check.h>
// #include <openxr/openxr.h>
#include <openxr/openxr_reflection.h>

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

[[noreturn]] inline void ThrowXrResult(XrResult res,
                                       const char *originator = nullptr,
                                       const char *sourceLocation = nullptr) {
  Throw(Fmt("XrResult failure [%s]", to_string(res)), originator,
        sourceLocation);
}

#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);

inline XrResult CheckXrResult(XrResult res, const char *originator = nullptr,
                              const char *sourceLocation = nullptr) {
  if (XR_FAILED(res)) {
    ThrowXrResult(res, originator, sourceLocation);
  }

  return res;
}
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);
