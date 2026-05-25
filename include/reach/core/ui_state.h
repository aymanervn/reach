#ifndef REACH_CORE_UI_STATE_H
#define REACH_CORE_UI_STATE_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/search_types.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

#define REACH_MAX_PINNED_APPS 12
#define REACH_MAX_SEARCH_CHARS 255

typedef struct reach_color {
    float r;
    float g;
    float b;
    float a;
} reach_color;

typedef struct reach_rect_f32 {
    float x;
    float y;
    float width;
    float height;
} reach_rect_f32;

typedef struct reach_pinned_app_model {
    uint32_t id;
    uint16_t title[128];
    uint16_t path[260];
    uint16_t icon_ref[260];
} reach_pinned_app_model;

typedef struct reach_dock_model {
    float height;
    float width;
    float icon_size;
    float gap;
    int32_t visible;
    int32_t auto_hide;
} reach_dock_model;

typedef struct reach_launcher_model {
    int32_t open;
    uint16_t query[REACH_MAX_SEARCH_CHARS + 1];
    size_t query_length;
    size_t caret_index;
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS];
    uint64_t result_icon_ids[REACH_SEARCH_MAX_RESULTS];
    int32_t result_icon_wants_backplate[REACH_SEARCH_MAX_RESULTS];
    size_t result_count;
    size_t selected_result_index;
} reach_launcher_model;

typedef struct reach_ui_state {
    reach_dock_model dock;
    reach_launcher_model launcher;
    reach_pinned_app_model pinned_apps[REACH_MAX_PINNED_APPS];
    size_t pinned_app_count;
} reach_ui_state;

void reach_ui_state_init(reach_ui_state *state);
reach_result reach_ui_state_set_pinned_apps(reach_ui_state *state, const reach_pinned_app_model *apps, size_t count);
reach_result reach_ui_state_open_launcher(reach_ui_state *state);
reach_result reach_ui_state_close_launcher(reach_ui_state *state);
reach_result reach_ui_state_toggle_launcher(reach_ui_state *state);
reach_result reach_ui_state_set_query(reach_ui_state *state, const uint16_t *query);
reach_result reach_ui_state_insert_launcher_text(reach_ui_state *state, const uint16_t *text);
reach_result reach_ui_state_delete_launcher_previous_character(reach_ui_state *state);
reach_result reach_ui_state_delete_launcher_next_character(reach_ui_state *state);
reach_result reach_ui_state_delete_launcher_previous_word(reach_ui_state *state);
reach_result reach_ui_state_move_launcher_caret_left(reach_ui_state *state, int32_t by_word);
reach_result reach_ui_state_move_launcher_caret_right(reach_ui_state *state, int32_t by_word);
reach_result reach_ui_state_move_launcher_caret_home(reach_ui_state *state);
reach_result reach_ui_state_move_launcher_caret_end(reach_ui_state *state);
reach_result reach_ui_state_set_launcher_results(reach_ui_state *state, const reach_search_candidate *results, size_t count);
reach_result reach_ui_state_set_launcher_result_icon(reach_ui_state *state, size_t index, uint64_t icon_id, int32_t wants_backplate);
reach_result reach_ui_state_select_next_launcher_result(reach_ui_state *state);
reach_result reach_ui_state_select_previous_launcher_result(reach_ui_state *state);
reach_result reach_ui_state_clear_launcher_results(reach_ui_state *state);
int32_t reach_ui_state_should_show_pinned_apps(const reach_ui_state *state);
int32_t reach_ui_state_should_show_search_placeholder(const reach_ui_state *state);

#ifdef __cplusplus
}
#endif

#endif
