#ifndef REACH_PORTS_INPUT_SOURCE_H
#define REACH_PORTS_INPUT_SOURCE_H

#include "reach/core/ui_events.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_input_source reach_input_source;

typedef void (*reach_input_event_callback)(void *user, const reach_ui_event *event);

typedef struct reach_input_source_ops {
    reach_result (*start)(reach_input_source *source, reach_input_event_callback callback, void *user);
    reach_result (*stop)(reach_input_source *source);
    reach_result (*get_pointer_position)(reach_input_source *source, reach_point_i32 *out_position);
    void (*destroy)(reach_input_source *source);
} reach_input_source_ops;

typedef struct reach_input_source_port {
    reach_input_source *source;
    reach_input_source_ops ops;
} reach_input_source_port;

#ifdef __cplusplus
}
#endif

#endif
