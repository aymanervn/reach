#ifndef REACH_PORTS_WINDOW_THUMBNAIL_H
#define REACH_PORTS_WINDOW_THUMBNAIL_H

#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/theme.h"
#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_window_thumbnails reach_window_thumbnails;

    typedef uint64_t reach_window_thumbnail_id;

#define REACH_WINDOW_THUMBNAIL_NONE ((reach_window_thumbnail_id)0)

    typedef enum reach_window_thumbnail_plane
    {
        REACH_WINDOW_THUMBNAIL_PLANE_TARGET = 0,
        REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET = 1
    } reach_window_thumbnail_plane;

    typedef struct reach_window_thumbnail_placement
    {
        reach_rect_f32 destination;
        reach_rect_f32 source_screen;
        float opacity;
        int32_t visible;
        int32_t source_screen_valid;
        reach_color background;
        int32_t background_visible;
    } reach_window_thumbnail_placement;

    typedef struct reach_window_thumbnail_ops
    {
        reach_result (*set_target)(reach_window_thumbnails *thumbnails, reach_window_id target);

        reach_result (*create)(reach_window_thumbnails *thumbnails, reach_window_id source,
                               reach_window_thumbnail_plane plane,
                               reach_window_thumbnail_id *out_id);

        reach_result (*set_placement)(reach_window_thumbnails *thumbnails,
                                      reach_window_thumbnail_id id,
                                      const reach_window_thumbnail_placement *placement);

        reach_result (*bring_to_front)(reach_window_thumbnails *thumbnails,
                                       reach_window_thumbnail_id id);

        reach_result (*release)(reach_window_thumbnails *thumbnails, reach_window_thumbnail_id id);

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
