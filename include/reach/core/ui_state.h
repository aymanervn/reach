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

    typedef enum reach_launcher_result_action_kind
    {
        REACH_LAUNCHER_RESULT_OPEN_SEARCH = 0,
        REACH_LAUNCHER_RESULT_RUN_TERMINAL_COMMAND = 1
    } reach_launcher_result_action_kind;

    typedef union reach_launcher_result_payload
    {
        reach_search_candidate search;
        uint16_t terminal_command[REACH_MAX_SEARCH_CHARS + 1];
    } reach_launcher_result_payload;

    typedef struct reach_launcher_result
    {
        uint16_t title[REACH_SEARCH_RESULT_NAME_CAPACITY];
        uint16_t subtitle[REACH_SEARCH_RESULT_PATH_CAPACITY];
        uint16_t icon_path[REACH_SEARCH_RESULT_PATH_CAPACITY];
        reach_search_result_kind visual_kind;
        reach_launcher_result_action_kind action;
        reach_launcher_result_payload payload;
    } reach_launcher_result;

    typedef struct reach_dock_model
    {
        float height;
        float icon_size;
        float gap;
        int32_t visible;
    } reach_dock_model;

    typedef struct reach_launcher_model
    {
        int32_t open;
        uint16_t query[REACH_MAX_SEARCH_CHARS + 1];
        size_t query_length;
        reach_launcher_result results[REACH_SEARCH_MAX_RESULTS];
        size_t result_count;
        size_t selected_result_index;
        int32_t search_error;
        reach_scrollbar_model result_scrollbar;
    } reach_launcher_model;

    void reach_dock_model_defaults(reach_dock_model *dock);

#ifdef __cplusplus
}
#endif

#endif
