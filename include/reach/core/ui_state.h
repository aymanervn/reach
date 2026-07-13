#ifndef REACH_CORE_UI_STATE_H
#define REACH_CORE_UI_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/core/geometry.h"
#include "reach/core/pinned_app.h"
#include "reach/core/scrollbar.h"
#include "reach/support/search_types.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_MAX_SEARCH_CHARS 255

    typedef struct reach_dock_model
    {
        float height;
        float width;
        float icon_size;
        float gap;
        int32_t visible;
        int32_t auto_hide;
    } reach_dock_model;

    typedef struct reach_launcher_model
    {
        int32_t open;
        uint16_t query[REACH_MAX_SEARCH_CHARS + 1];
        size_t query_length;
        reach_search_candidate results[REACH_SEARCH_MAX_RESULTS];
        size_t result_count;
        size_t selected_result_index;
        reach_scrollbar_model result_scrollbar;
    } reach_launcher_model;

    /*
     * Shared canonical app data. pinned_apps is NOT part of reach_ui_state — it is
     * composition-owned (reach_host) and fed into the dock/launcher layout as an
     * input, because it is shared by dock/context-menu/config/icons and outlives
     * any one feature. See reach_ui_layout_input.pinned_app_count.
     */
    /*
     * The launcher model left this aggregate in E (it now lives in the reach_launcher
     * capsule, reach/features/launcher.h). reach_launcher_model stays defined here as
     * a neutral core data type so core layout (reach_launcher_layout_compute) can name
     * it. reach_ui_state now carries only the dock model.
     */
    /* Default dock configuration (the former reach_ui_state aggregate is gone:
     * the dock model is the only member it had left). */
    void reach_dock_model_defaults(reach_dock_model *dock);

#ifdef __cplusplus
}
#endif

#endif
