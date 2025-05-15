#include "common.hpp"
#include "dispatcher.h"

#include <magic_enum/magic_enum.hpp>

struct UserData {
  android_app *pApp = nullptr;
  Dispatcher *dispatcher = nullptr;
};

enum class cmd_names {
  APP_CMD_INPUT_CHANGED,
  APP_CMD_INIT_WINDOW,
  APP_CMD_TERM_WINDOW,
  APP_CMD_WINDOW_RESIZED,
  APP_CMD_WINDOW_REDRAW_NEEDED,
  APP_CMD_CONTENT_RECT_CHANGED,
  APP_CMD_GAINED_FOCUS,
  APP_CMD_LOST_FOCUS,
  APP_CMD_CONFIG_CHANGED,
  APP_CMD_LOW_MEMORY,
  APP_CMD_START,
  APP_CMD_RESUME,
  APP_CMD_SAVE_STATE,
  APP_CMD_PAUSE,
  APP_CMD_STOP,
  APP_CMD_DESTROY,
};

static void engineHandleCmd(android_app *pApp, int32_t _cmd) {
  auto *dispatcher = static_cast<UserData *>(pApp->userData)->dispatcher;
  auto cmd = static_cast<cmd_names>(_cmd);
  switch (cmd) {
  case cmd_names::APP_CMD_RESUME:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onResume();
    break;

  case cmd_names::APP_CMD_PAUSE:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onPause();
    break;

  case cmd_names::APP_CMD_INIT_WINDOW:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onInitWindow(pApp->window, pApp->activity->assetManager);
    break;

  case cmd_names::APP_CMD_TERM_WINDOW:
    LOGI("# %s", magic_enum::enum_name(cmd).data());
    dispatcher->onTermWindow();
    break;

  default:
    LOGI("# %s: not handled", magic_enum::enum_name(cmd).data());
    break;
  }
}

void android_main(android_app *state) {
#ifdef NDEBUG
  LOGI("#### [release][android_main] ####");
#else
  LOGI("#### [debug][android_main] ####");
#endif

  Dispatcher dispatcher;
  UserData engine{
      .pApp = state,
      .dispatcher = &dispatcher,
  };
  state->userData = &engine;
  state->onAppCmd = engineHandleCmd;
  state->onInputEvent = [](android_app *, AInputEvent *) { return 0; };

  for (;;) {
    struct android_poll_source *source;
    int events;
    while ((ALooper_pollOnce(dispatcher.isReady() ? 0 : -1, nullptr, &events,
                             (void **)&source)) >= 0) {
      if (source) {
        source->process(state, source);
      }

      if (state->destroyRequested) {
        return;
      }
    }

    if (!dispatcher.onFrame(state->activity->assetManager)) {
      break;
    }
  }
}
