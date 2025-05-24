// Copyright (c) 2017-2024, The Khronos Group Inc.
//
// SPDX-License-Identifier: Apache-2.0

#pragma once
#include "fmt.h"

#define CHK_STRINGIFY(x) #x
#define TOSTRING(x) CHK_STRINGIFY(x)
#define FILE_AND_LINE __FILE__ ":" TOSTRING(__LINE__)

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
