// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)

#define THROW(msg) Throw(msg, nullptr, FILE_AND_LINE);
#define CHECK(exp)                                                             \
  {                                                                            \
    if (!(exp)) {                                                              \
      Throw("Check failed", #exp, FILE_AND_LINE);                              \
    }                                                                          \
  }
#define CHECK_MSG(exp, msg)                                                    \
  {                                                                            \
    if (!(exp)) {                                                              \
      Throw(msg, #exp, FILE_AND_LINE);                                         \
    }                                                                          \
  }

#define THROW_XR(xr, cmd) ThrowXrResult(xr, #cmd, FILE_AND_LINE);
#define CHECK_XRCMD(cmd) CheckXrResult(cmd, #cmd, FILE_AND_LINE);
#define CHECK_XRRESULT(res, cmdStr) CheckXrResult(res, cmdStr, FILE_AND_LINE);

