#ifndef REACH_ADAPTERS_WINDOWS_RENDER_D2D_RETAINED_POLICY_H
#define REACH_ADAPTERS_WINDOWS_RENDER_D2D_RETAINED_POLICY_H

#include <stdint.h>

uint64_t reach_d2d_layer_bytes(float width, float height, bool has_paint, bool has_shadow);

#endif
