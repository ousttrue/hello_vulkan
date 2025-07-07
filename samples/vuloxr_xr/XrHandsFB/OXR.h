#pragma once
#include "OVR_Math.h"
#include <Misc/Log.h>
#include <openxr/openxr.h>
#include <string>
#include <xr_linear.h>

inline std::string OXR_ResultToString(XrInstance instance, XrResult result) {
  char errorBuffer[XR_MAX_RESULT_STRING_SIZE]{};
  xrResultToString(instance, result, errorBuffer);
  return std::string{errorBuffer};
}

inline void OXR_CheckErrors(XrInstance instance, XrResult result,
                            const char *function, bool failOnError) {
  if (XR_FAILED(result)) {
    const std::string error = OXR_ResultToString(instance, result);
    if (failOnError) {
      ALOGE("OpenXR error: %s: %s\n", function, error.c_str());
    } else {
      ALOGV("OpenXR error: %s: %s\n", function, error.c_str());
    }
  }
}

#if defined(DEBUG) || defined(_DEBUG)
#define OXR(func) OXR_CheckErrors(Instance, func, #func, true);
#else
#define OXR(func) OXR_CheckErrors(Instance, func, #func, false);
#endif

// inline XrVector2f ToXrVector2f(const OVR::Vector2f& s) {
//     XrVector2f r;
//     r.x = s.x;
//     r.y = s.y;
//     return r;
// }

inline OVR::Vector2f FromXrVector2f(const XrVector2f &s) {
  OVR::Vector2f r;
  r.x = s.x;
  r.y = s.y;
  return r;
}

// inline OVR::Vector2f FromXrExtent2Df(const XrExtent2Df& s) {
//     OVR::Vector2f r;
//     r.x = s.width;
//     r.y = s.height;
//     return r;
// }
//
// inline XrVector3f ToXrVector3f(const OVR::Vector3f& s) {
//     XrVector3f r;
//     r.x = s.x;
//     r.y = s.y;
//     r.z = s.z;
//     return r;
// }

inline OVR::Vector3f FromXrVector3f(const XrVector3f &s) {
  OVR::Vector3f r;
  r.x = s.x;
  r.y = s.y;
  r.z = s.z;
  return r;
}

// inline OVR::Vector4f FromXrVector4f(const XrVector4f& s) {
//     OVR::Vector4f r;
//     r.x = s.x;
//     r.y = s.y;
//     r.z = s.z;
//     r.w = s.w;
//     return r;
// }
//
// inline OVR::Vector4f FromXrColor4f(const XrColor4f& s) {
//     OVR::Vector4f r;
//     r.x = s.r;
//     r.y = s.g;
//     r.z = s.b;
//     r.w = s.a;
//     return r;
// }
//
// inline XrQuaternionf ToXrQuaternionf(const OVR::Quatf& s) {
//     XrQuaternionf r;
//     r.x = s.x;
//     r.y = s.y;
//     r.z = s.z;
//     r.w = s.w;
//     return r;
// }

inline OVR::Quatf FromXrQuaternionf(const XrQuaternionf &s) {
  OVR::Quatf r;
  r.x = s.x;
  r.y = s.y;
  r.z = s.z;
  r.w = s.w;
  return r;
}

// inline XrPosef ToXrPosef(const OVR::Posef& s) {
//     XrPosef r;
//     r.orientation = ToXrQuaternionf(s.Rotation);
//     r.position = ToXrVector3f(s.Translation);
//     return r;
// }

inline OVR::Posef FromXrPosef(const XrPosef &s) {
  OVR::Posef r;
  r.Rotation = FromXrQuaternionf(s.orientation);
  r.Translation = FromXrVector3f(s.position);
  return r;
}

inline XrTime ToXrTime(const double timeInSeconds) {
  return (timeInSeconds * 1e9);
}

inline double FromXrTime(const XrTime time) { return (time * 1e-9); }

inline OVR::Matrix4f FromXrMatrix4x4f(const XrMatrix4x4f &src) {
  // col major to row major ==> transpose
  return OVR::Matrix4f(src.m[0], src.m[4], src.m[8], src.m[12], src.m[1],
                       src.m[5], src.m[9], src.m[13], src.m[2], src.m[6],
                       src.m[10], src.m[14], src.m[3], src.m[7], src.m[11],
                       src.m[15]);
}
