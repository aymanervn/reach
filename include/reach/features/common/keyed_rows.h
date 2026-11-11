#ifndef REACH_FEATURES_COMMON_KEYED_ROWS_H
#define REACH_FEATURES_COMMON_KEYED_ROWS_H

#include "reach/core/theme.h"
#include "reach/support/motion.h"

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

    typedef enum reach_keyed_row_lifecycle
    {
        REACH_KEYED_ROW_EMPTY = 0,
        REACH_KEYED_ROW_LIVE,
        REACH_KEYED_ROW_RETIRING
    } reach_keyed_row_lifecycle;

    typedef struct reach_keyed_row
    {
        int32_t lifecycle;
        size_t index;
        reach_motion y;
        reach_motion opacity;
    } reach_keyed_row;

    typedef enum reach_keyed_row_change_kind
    {
        REACH_KEYED_ROW_ALLOCATED = 1,
        REACH_KEYED_ROW_REVIVED,
        REACH_KEYED_ROW_RELEASED
    } reach_keyed_row_change_kind;

    typedef struct reach_keyed_row_change
    {
        int32_t kind;
        size_t slot;
        size_t index;
        int32_t previous_lifecycle;
    } reach_keyed_row_change;

    typedef int32_t (*reach_keyed_rows_same_fn)(const void *context, size_t slot,
                                                size_t item_index);

    typedef struct reach_keyed_rows_sync_input
    {
        size_t count;
        const float *targets;
        reach_keyed_rows_same_fn same;
        const void *context;
        int32_t animate;
        const reach_theme *theme;
        double now_seconds;
    } reach_keyed_rows_sync_input;

#define REACH_KEYED_ROWS_MAX_CHANGES(capacity) (2 * (capacity))

    size_t reach_keyed_rows_sync(reach_keyed_row *rows, size_t capacity,
                                 const reach_keyed_rows_sync_input *input,
                                 reach_keyed_row_change *out_changes, size_t change_capacity);
    size_t reach_keyed_rows_release_settled(reach_keyed_row *rows, size_t capacity,
                                            reach_keyed_row_change *out_changes,
                                            size_t change_capacity);
    int32_t reach_keyed_rows_active(const reach_keyed_row *rows, size_t capacity);
    double reach_keyed_rows_deadline(const reach_keyed_row *rows, size_t capacity);

#ifdef __cplusplus
}
#endif

#endif
