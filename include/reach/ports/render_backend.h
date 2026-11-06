#ifndef REACH_PORTS_RENDER_BACKEND_H
#define REACH_PORTS_RENDER_BACKEND_H

#include "reach/core/render_commands.h"
#include "reach/ports/text_measure.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_render_backend reach_render_backend;

    typedef struct reach_render_backend_ops
    {
        reach_result (*begin_frame)(reach_render_backend *backend);
        reach_result (*end_frame)(reach_render_backend *backend);
        reach_result (*execute)(reach_render_backend *backend,
                                const reach_render_command_buffer *commands);
        void (*release_icon)(reach_render_backend *backend, uint64_t icon_id);
        void (*set_ui_font)(reach_render_backend *backend, int32_t use_bundled_font);
        reach_text_measure_fn measure_text;
        void (*destroy)(reach_render_backend *backend);
        reach_result (*synchronize)(reach_render_backend *backend);
        int32_t (*retained_supported)(const reach_render_backend *backend);
        reach_result (*presentation_complete)(reach_render_backend *backend, uint64_t generation,
                                              int32_t *out_complete);
        void (*suspend)(reach_render_backend *backend);
        int32_t (*content_sampling_required)(const reach_render_backend *backend);
        reach_result (*sample_content)(reach_render_backend *backend);
    } reach_render_backend_ops;

    typedef struct reach_render_backend_port
    {
        reach_render_backend *backend;
        reach_render_backend_ops ops;
    } reach_render_backend_port;

    static inline int32_t
    reach_render_backend_content_sampling_active(const reach_render_backend_port *renderer,
                                                 const reach_render_command_buffer *commands)
    {
        return commands != 0 &&
               (commands->requires_content_sampling ||
                (renderer != 0 && renderer->ops.content_sampling_required != 0 &&
                 renderer->ops.content_sampling_required(renderer->backend)) ||
                (reach_render_command_buffer_motion_active(commands) && renderer != 0 &&
                 (renderer->ops.retained_supported == 0 ||
                  !renderer->ops.retained_supported(renderer->backend))));
    }

#ifdef __cplusplus
}
#endif

#endif
