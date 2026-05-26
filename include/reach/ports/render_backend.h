#ifndef REACH_PORTS_RENDER_BACKEND_H
#define REACH_PORTS_RENDER_BACKEND_H

#include "reach/core/render_commands.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_render_backend reach_render_backend;

typedef struct reach_render_backend_ops {
    reach_result (*begin_frame)(reach_render_backend *backend);
    reach_result (*end_frame)(reach_render_backend *backend);
    reach_result (*execute)(reach_render_backend *backend, const reach_render_command_buffer *commands);
    void (*release_icon)(reach_render_backend *backend, uint64_t icon_id);
    void (*destroy)(reach_render_backend *backend);
} reach_render_backend_ops;

typedef struct reach_render_backend_port {
    reach_render_backend *backend;
    reach_render_backend_ops ops;
} reach_render_backend_port;

#ifdef __cplusplus
}
#endif

#endif
