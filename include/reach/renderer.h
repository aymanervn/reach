#ifndef REACH_RENDERER_H
#define REACH_RENDERER_H

#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_renderer reach_renderer;

typedef struct reach_renderer_desc {
    void *native_window;
    int32_t width;
    int32_t height;
} reach_renderer_desc;

reach_result reach_renderer_create(const reach_renderer_desc *desc, reach_renderer **out_renderer);
void reach_renderer_destroy(reach_renderer *renderer);
reach_result reach_renderer_resize(reach_renderer *renderer, int32_t width, int32_t height);
reach_result reach_renderer_begin(reach_renderer *renderer);
reach_result reach_renderer_end(reach_renderer *renderer);
reach_result reach_renderer_clear(reach_renderer *renderer, float r, float g, float b, float a);

#ifdef __cplusplus
}
#endif

#endif
