#pragma once
#include <openxr/openxr.h>

int init_gles_scene ();
int render_gles_scene (uint32_t fbo_id, XrRect2Di imgRect);
