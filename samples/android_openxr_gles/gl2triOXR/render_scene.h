#pragma once
#include "util_render_target.h"
#include <openxr/openxr.h>

int init_gles_scene();
int render_gles_scene(render_target_t &rtarget, XrRect2Di imgRect);
