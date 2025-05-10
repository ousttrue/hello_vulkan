#pragma once

#ifdef ANDROID
#ifndef APP_NAME
#define APP_NAME "VULFWK"
#endif
#include <android/log.h>
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, APP_NAME, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, APP_NAME, __VA_ARGS__)
#else

#include <stdarg.h>
#include <stdio.h>

inline void LOGI(const char *fmt, ...) {
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vfprintf(stderr, fmt, arg_ptr);
  va_end(arg_ptr);

  fprintf(stderr, "\n");
}
inline void LOGE(const char *fmt, ...) {
  va_list arg_ptr;
  va_start(arg_ptr, fmt);
  vfprintf(stderr, fmt, arg_ptr);
  va_end(arg_ptr);

  fprintf(stderr, "\n");
}

#endif
