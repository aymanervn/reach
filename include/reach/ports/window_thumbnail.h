#ifndef REACH_PORTS_WINDOW_THUMBNAIL_H
#define REACH_PORTS_WINDOW_THUMBNAIL_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_window_thumbnails reach_window_thumbnails;

    typedef uint64_t reach_window_thumbnail_id;

#define REACH_WINDOW_THUMBNAIL_NONE ((reach_window_thumbnail_id)0)

    typedef struct reach_window_thumbnail_placement
    {
        reach_rect_f32 destination;
        reach_rect_f32 source_screen;
        float opacity;
        int32_t visible;
        int32_t source_screen_valid;
    } reach_window_thumbnail_placement;

    typedef struct reach_window_thumbnail_ops
    {
        reach_result (*set_target)(reach_window_thumbnails *thumbnails, reach_window_id target);

        reach_result (*create)(reach_window_thumbnails *thumbnails, reach_window_id source,
                               reach_window_thumbnail_id *out_id);

        reach_result (*set_placement)(reach_window_thumbnails *thumbnails,
                                      reach_window_thumbnail_id id,
                                      const reach_window_thumbnail_placement *placement);

        reach_result (*destroy_all)(reach_window_thumbnails *thumbnails);

        void (*destroy)(reach_window_thumbnails *thumbnails);
    } reach_window_thumbnail_ops;

    typedef struct reach_window_thumbnail_port
    {
        reach_window_thumbnails *thumbnails;
        reach_window_thumbnail_ops ops;
    } reach_window_thumbnail_port;

#ifdef __cplusplus
}
#endif

#endif
