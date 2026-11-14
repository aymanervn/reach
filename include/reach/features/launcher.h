#ifndef REACH_FEATURES_LAUNCHER_H
#define REACH_FEATURES_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/ui_layout.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/features/common/keyed_rows.h"
#include "reach/features/common/scroll_view.h"
#include "reach/features/common/selection_pill.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/common/pressable.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/search.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_LAUNCHER_ROW_CAPACITY 16

    typedef struct reach_launcher_state reach_launcher_state;

    typedef struct reach_launcher reach_launcher;

    reach_result reach_launcher_create(reach_launcher **out_launcher);
    void reach_launcher_destroy(reach_launcher *launcher);

    const reach_launcher_state *reach_launcher_state_ptr(reach_launcher *launcher);

    const reach_feature_capsule_ops *reach_launcher_capsule_ops(void);

    typedef struct reach_launcher_arrange_context
    {
        const reach_theme *theme;
        reach_rect_f32 monitor_bounds;
        float dpi_scale;
    } reach_launcher_arrange_context;

    int32_t reach_launcher_arrange(reach_launcher *launcher,
                                   const reach_launcher_arrange_context *ctx);

    int32_t reach_launcher_set_open(reach_launcher *launcher, int32_t open);
    void reach_launcher_surface_hidden(reach_launcher *launcher);
    reach_result reach_launcher_set_query(reach_launcher *launcher, const uint16_t *query);
    reach_result reach_launcher_set_results(reach_launcher *launcher,
                                            const reach_search_candidate *results, size_t count);
    reach_result reach_launcher_clear_results(reach_launcher *launcher);

    void reach_launcher_attach_search(reach_launcher *launcher, reach_search_service *search);

    void reach_launcher_attach_icons(reach_launcher *launcher, reach_icon_service *icons);
    void reach_launcher_set_terminal_icon_ref(reach_launcher *launcher, const uint16_t *icon_ref);
    void reach_launcher_cancel_search(reach_launcher *launcher);

    const reach_ui_event_type *reach_launcher_activation_events(size_t *out_count);
    const reach_ui_event_type *reach_launcher_routed_events(size_t *out_count);

    int32_t reach_launcher_is_open(reach_launcher *launcher);

    typedef struct reach_launcher_render_input
    {
        const reach_theme *theme;
        const reach_launcher_model *model;
        const reach_launcher_layout *layout;

        const reach_keyed_row *rows;
        const reach_launcher_result *row_results;
        const uint64_t *row_icon_ids;
        size_t row_start;
        float row_pitch;
        const reach_scroll_view *scroll;
        reach_rect_motion selection;
        int32_t has_selection;
        const reach_motion *row_selected;
        reach_motion press_feedback;
        size_t press_index;
        int32_t has_press;
        reach_motion hover_opacity;
        size_t hover_index;
        int32_t has_hover;
        reach_motion search_icon_opacity;
        float dpi_scale;
        int32_t text_alignment_leading;

        reach_motion caret_opacity;
        int32_t caret_retained;
        int32_t caret_index;
        int32_t caret_visible;
        int32_t selection_start;
        int32_t selection_end;
        reach_motion body_height;
        reach_motion results_opacity;
    } reach_launcher_render_input;

    struct reach_launcher_state
    {
        reach_launcher_model model;
        reach_pressable pressable;
        reach_scroll_view scroll;

        reach_text_edit launcher_text_edit;
        int32_t launcher_caret_visible;
    };

    reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

    typedef struct reach_launcher_render_context
    {
        const reach_theme *theme;
        const reach_launcher_layout *layout;
        float dpi_scale;
        int32_t retained_supported;
    } reach_launcher_render_context;

    reach_result reach_launcher_append_render_commands(reach_launcher *launcher,
                                                       const reach_launcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);
    reach_result reach_launcher_append_surface_render_commands(
        reach_launcher *launcher, const reach_theme *theme, float dpi_scale,
        int32_t retained_supported, reach_render_command_buffer *out_commands);
#ifdef __cplusplus
}
#endif

#endif
