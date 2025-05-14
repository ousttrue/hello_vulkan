#include "android.hpp"
#include "common.hpp"
#include "dispatcher.h"

struct UserData {
  android_app *pApp = nullptr;
  Dispatcher *dispatcher = nullptr;
};

static void engineHandleCmd(android_app *pApp, int32_t cmd) {
  auto *dispatcher = static_cast<UserData *>(pApp->userData)->dispatcher;
  switch (cmd) {
  case APP_CMD_RESUME:
    dispatcher->onResume();
    break;

  case APP_CMD_PAUSE:
    dispatcher->onPause();
    break;

  case APP_CMD_INIT_WINDOW:
    dispatcher->onInitWindow(pApp->window, pApp->activity->assetManager);
    break;

  case APP_CMD_TERM_WINDOW:
    dispatcher->onTermWindow();
    break;

  default:
    LOGI("%d not handled", cmd);
    break;
  }
}

void android_main(android_app *state) {
#ifdef NDEBUG
  LOGI("#### [release][android_main] ####");
#else
  LOGI("#### [debug][android_main] ####");
#endif

  MaliSDK::AndroidPlatform platform;
  Dispatcher dispatcher{
      .pPlatform = &platform,
      .active = false,
  };
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

    if (!dispatcher.onFrame()) {
      break;
    }
  }
}
