#pragma once
#include <android/log.h>
#include <android_native_app_glue.h>

namespace vuloxr {
namespace android {

struct UserData {
  android_app *pApp = nullptr;
  bool _active = false;
  ANativeWindow *_window = nullptr;
  const char *_appName = "app";

  static const char *cmd_name(int32_t cmd) {
    switch (cmd) {
    case APP_CMD_INPUT_CHANGED:
      return "APP_CMD_INPUT_CHANGED";
    case APP_CMD_INIT_WINDOW:
      return "APP_CMD_INIT_WINDOW";
    case APP_CMD_TERM_WINDOW:
      return "APP_CMD_TERM_WINDOW";
    case APP_CMD_WINDOW_RESIZED:
      return "APP_CMD_WINDOW_RESIZED";
    case APP_CMD_WINDOW_REDRAW_NEEDED:
      return "APP_CMD_WINDOW_REDRAW_NEEDED";
    case APP_CMD_CONTENT_RECT_CHANGED:
      return "APP_CMD_CONTENT_RECT_CHANGED";
    case APP_CMD_GAINED_FOCUS:
      return "APP_CMD_GAINED_FOCUS";
    case APP_CMD_LOST_FOCUS:
      return "APP_CMD_LOST_FOCUS";
    case APP_CMD_CONFIG_CHANGED:
      return "APP_CMD_CONFIG_CHANGED";
    case APP_CMD_LOW_MEMORY:
      return "APP_CMD_LOW_MEMORY";
    case APP_CMD_START:
      return "APP_CMD_START";
    case APP_CMD_RESUME:
      return "APP_CMD_RESUME";
    case APP_CMD_SAVE_STATE:
      return "APP_CMD_SAVE_STATE";
    case APP_CMD_PAUSE:
      return "APP_CMD_PAUSE";
    case APP_CMD_STOP:
      return "APP_CMD_STOP";
    case APP_CMD_DESTROY:
      return "APP_CMD_DESTROY";
    };
    return "unknown";
  }

  static void on_app_cmd(android_app *pApp, int32_t cmd) {
    auto *userdata = static_cast<UserData *>(pApp->userData);
    switch (cmd) {
    case APP_CMD_RESUME:
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName, "# %s",
                          cmd_name(cmd));
      userdata->_active = true;
      break;

    case APP_CMD_INIT_WINDOW:
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName, "# %s",
                          cmd_name(cmd));
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName,
                          "# APP_CMD_RESUME");
      userdata->_window = pApp->window;
      break;

    case APP_CMD_PAUSE:
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName, "# %s",
                          cmd_name(cmd));
      userdata->_active = false;
      break;

    case APP_CMD_TERM_WINDOW:
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName, "# %s",
                          cmd_name(cmd));
      userdata->_window = nullptr;
      break;

    default:
      __android_log_print(ANDROID_LOG_INFO, userdata->_appName,
                          "# %s: not handled", cmd_name(cmd));
      break;
    }
  }
};

inline bool wait_window(android_app *app, UserData *userdata) {
  while (true) {
    struct android_poll_source *source;
    int events;
    if (ALooper_pollOnce(-1, nullptr, &events, (void **)&source) < 0) {
      continue;
    }
    if (source) {
      source->process(app, source);
    }
    if (app->destroyRequested) {
      return true;
    }
    if (userdata->_window) {
      return false;
    }
  }
}

} // namespace android
} // namespace vuloxr
