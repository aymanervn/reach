#ifndef REACH_FEATURES_LAUNCHER_H
#define REACH_FEATURES_LAUNCHER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/render_commands.h"
#include "reach/core/theme.h"
#include "reach/core/ui_events.h"
#include "reach/features/common/text_edit.h"
#include "reach/features/feature_capsule.h"
#include "reach/services/icon_service.h"
#include "reach/services/search.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_launcher_state reach_launcher_state;

    typedef struct reach_launcher reach_launcher;

    reach_result reach_launcher_create(reach_launcher **out_launcher);
    void reach_launcher_destroy(reach_launcher *launcher);

    const reach_launcher_state *reach_launcher_state_ptr(reach_launcher *launcher);

    const reach_feature_capsule_ops *reach_launcher_capsule_ops(void);

    void reach_launcher_set_pointer_context(reach_launcher *launcher,
                                            const reach_launcher_layout *layout,
                                            const reach_pinned_app_model *pinned_apps,
                                            size_t pinned_app_count);

    reach_result reach_launcher_close(reach_launcher *launcher);
    reach_result reach_launcher_toggle(reach_launcher *launcher);
    reach_result reach_launcher_set_query(reach_launcher *launcher, const uint16_t *query);
    reach_result reach_launcher_set_results(reach_launcher *launcher,
                                            const reach_search_candidate *results, size_t count);
    reach_result reach_launcher_clear_results(reach_launcher *launcher);
    size_t reach_launcher_model_result_scroll_offset(const reach_launcher_model *launcher);

    void reach_launcher_attach_search(reach_launcher *launcher, reach_search_service *search);

    void reach_launcher_attach_icons(reach_launcher *launcher, reach_icon_service *icons);
    void reach_launcher_cancel_search(reach_launcher *launcher);
    int32_t reach_launcher_take_search_results(reach_launcher *launcher,
                                               reach_search_candidate *out_results,
                                               size_t *out_count);

    const reach_ui_event_type *reach_launcher_activation_events(size_t *out_count);

    void reach_launcher_clear_pressed(reach_launcher *launcher);
    void reach_launcher_set_focused(reach_launcher *launcher, int32_t focused);

    void reach_launcher_remember_restore_window(reach_launcher *launcher, uintptr_t window);
    void reach_launcher_clear_restore_window(reach_launcher *launcher);
    uintptr_t reach_launcher_take_restore_window(reach_launcher *launcher);

    typedef struct reach_launcher_text_event_result
    {
        int32_t redraw;
        int32_t relayout;
    } reach_launcher_text_event_result;

    void reach_launcher_handle_text_event(reach_launcher *launcher, const reach_ui_event *event,
                                          reach_launcher_text_event_result *out_result);
    void reach_launcher_reset_text_edit(reach_launcher *launcher);

    int32_t reach_launcher_is_open(reach_launcher *launcher);
    size_t reach_launcher_result_count(reach_launcher *launcher);
    const reach_search_candidate *reach_launcher_result_at(reach_launcher *launcher, size_t index);
    size_t reach_launcher_selected_result_index(reach_launcher *launcher);
    const uint16_t *reach_launcher_query_text(reach_launcher *launcher);

    void reach_launcher_clear_query(reach_launcher *launcher);

    reach_result reach_launcher_handle_event(reach_launcher *launcher, const reach_ui_event *event,
                                             reach_ui_intent *out_intent);

    typedef struct reach_launcher_render_input
    {
        const reach_theme *theme;
        const reach_launcher_model *model;
        const reach_launcher_layout *layout;

        const uint64_t *result_icon_ids;
        float dpi_scale;
        int32_t text_alignment_leading;

        int32_t caret_index;
        int32_t caret_visible;
        int32_t selection_start;
        int32_t selection_end;
    } reach_launcher_render_input;

    typedef enum reach_launcher_pointer_action_kind
    {
        REACH_LAUNCHER_POINTER_ACTION_NONE = 0,
        REACH_LAUNCHER_POINTER_ACTION_LAUNCH_PINNED = 1,
        REACH_LAUNCHER_POINTER_ACTION_OPEN_RESULT = 2,
        REACH_LAUNCHER_POINTER_ACTION_REVEAL_RESULT = 3
    } reach_launcher_pointer_action_kind;

    struct reach_launcher_state
    {
        reach_launcher_model model;
        int32_t pressed_launcher_hit_type;
        size_t pressed_launcher_index;
        reach_scrollbar_drag launcher_scrollbar_drag;

        reach_text_edit launcher_text_edit;
        int32_t launcher_focused;

        uintptr_t restore_window;
        int32_t restore_window_valid;
        double launcher_caret_blink_seconds;
        int32_t launcher_caret_visible;
    };

    reach_result reach_launcher_build_render_commands(const reach_launcher_render_input *input,
                                                      reach_render_command_buffer *out_commands);

    typedef struct reach_launcher_render_context
    {
        const reach_theme *theme;
        const reach_launcher_layout *layout;
        float dpi_scale;
    } reach_launcher_render_context;

    reach_result reach_launcher_append_render_commands(reach_launcher *launcher,
                                                       const reach_launcher_render_context *ctx,
                                                       reach_render_command_buffer *out_commands);
#ifdef __cplusplus
}
#endif

#endif
