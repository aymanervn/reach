#ifndef REACH_PORTS_SCREEN_CAPTURE_H
#define REACH_PORTS_SCREEN_CAPTURE_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/image.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_screen_capture reach_screen_capture;

    typedef struct reach_screen_image
    {
        uint64_t image_id;
        reach_rect_f32 screen_bounds;
        reach_image_view pixels;
    } reach_screen_image;

    typedef struct reach_screen_capture_ops
    {
        reach_result (*capture)(reach_screen_capture *capture, reach_rect_f32 screen_bounds,
                                reach_screen_image *out_image);
        void (*release)(reach_screen_capture *capture, uint64_t image_id);
        void (*destroy)(reach_screen_capture *capture);
    } reach_screen_capture_ops;

    typedef struct reach_screen_capture_port
    {
        reach_screen_capture *capture;
        reach_screen_capture_ops ops;
    } reach_screen_capture_port;

#ifdef __cplusplus
}
#endif

#endif
