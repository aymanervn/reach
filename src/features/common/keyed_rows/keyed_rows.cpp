#include "reach/features/common/keyed_rows.h"

#include "reach/support/util.h"

#include <math.h>

static const size_t REACH_KEYED_ROWS_MAX_CAPACITY = 256;

static int32_t reach_keyed_row_lifecycle_valid(int32_t lifecycle)
{
    return lifecycle == REACH_KEYED_ROW_EMPTY || lifecycle == REACH_KEYED_ROW_LIVE ||
           lifecycle == REACH_KEYED_ROW_RETIRING;
}

static void reach_keyed_rows_check(const reach_keyed_row *rows, size_t capacity)
{
    REACH_ASSERT(rows != nullptr);
    REACH_ASSERT(capacity > 0 && capacity <= REACH_KEYED_ROWS_MAX_CAPACITY);
    for (size_t slot = 0; rows != nullptr && slot < capacity; ++slot)
        REACH_ASSERT(reach_keyed_row_lifecycle_valid(rows[slot].lifecycle));
}

static void reach_keyed_rows_report(reach_keyed_row_change *changes, size_t change_capacity,
                                    size_t *change_count, int32_t kind, size_t slot, size_t index,
                                    int32_t previous_lifecycle)
{
    REACH_ASSERT(*change_count < change_capacity);
    if (*change_count >= change_capacity || changes == nullptr)
        return;
    changes[(*change_count)++] = {kind, slot, index, previous_lifecycle};
}

static void reach_keyed_rows_fade_to(const reach_theme *theme, reach_motion *motion,
                                     int32_t visible, double now_seconds)
{
    float target = visible ? 1.0f : 0.0f;
    if (motion->generation != 0 && motion->to == target)
        return;
    reach_motion_animate_spec_to_at(
        motion, target,
        reach_theme_motion_preset(theme, visible ? REACH_MOTION_PRESET_FADE_IN
                                                 : REACH_MOTION_PRESET_FADE_OUT),
        now_seconds);
}

static size_t reach_keyed_rows_allocate(const reach_keyed_row *rows, size_t capacity,
                                        const bool *claimed)
{
    for (size_t slot = 0; slot < capacity; ++slot)
    {
        if (!claimed[slot] && rows[slot].lifecycle == REACH_KEYED_ROW_EMPTY)
            return slot;
    }
    return capacity;
}

size_t reach_keyed_rows_sync(reach_keyed_row *rows, size_t capacity,
                             const reach_keyed_rows_sync_input *input,
                             reach_keyed_row_change *out_changes, size_t change_capacity)
{
    reach_keyed_rows_check(rows, capacity);
    REACH_ASSERT(input != nullptr && input->same != nullptr);
    if (rows == nullptr || capacity == 0 || capacity > REACH_KEYED_ROWS_MAX_CAPACITY ||
        input == nullptr || input->same == nullptr)
        return 0;
    size_t count = input->count < capacity ? input->count : capacity;
    REACH_ASSERT(count == 0 || input->targets != nullptr);
    for (size_t index = 0; index < count; ++index)
        REACH_ASSERT(isfinite(input->targets[index]));
    reach_motion_spec smooth = reach_theme_motion_preset(input->theme, REACH_MOTION_PRESET_SMOOTH);
    size_t slots[REACH_KEYED_ROWS_MAX_CAPACITY] = {};
    bool claimed[REACH_KEYED_ROWS_MAX_CAPACITY] = {};
    size_t change_count = 0;
    const int32_t passes[] = {REACH_KEYED_ROW_LIVE, REACH_KEYED_ROW_RETIRING};
    for (size_t index = 0; index < count; ++index)
    {
        slots[index] = capacity;
        for (int32_t lifecycle : passes)
        {
            for (size_t slot = 0; slot < capacity && slots[index] == capacity; ++slot)
            {
                if (!claimed[slot] && rows[slot].lifecycle == lifecycle &&
                    input->same(input->context, slot, index))
                {
                    slots[index] = slot;
                    claimed[slot] = true;
                }
            }
        }
    }
    for (size_t slot = 0; slot < capacity; ++slot)
    {
        reach_keyed_row &row = rows[slot];
        if (claimed[slot] || row.lifecycle == REACH_KEYED_ROW_EMPTY)
            continue;
        if (!input->animate)
        {
            reach_keyed_rows_report(out_changes, change_capacity, &change_count,
                                    REACH_KEYED_ROW_RELEASED, slot, row.index, row.lifecycle);
            row = {};
        }
        else if (row.lifecycle == REACH_KEYED_ROW_LIVE)
        {
            row.lifecycle = REACH_KEYED_ROW_RETIRING;
            reach_keyed_rows_fade_to(input->theme, &row.opacity, 0, input->now_seconds);
        }
    }
    for (size_t index = 0; index < count; ++index)
    {
        size_t slot = slots[index];
        float target = input->targets[index];
        if (slot == capacity)
        {
            slot = reach_keyed_rows_allocate(rows, capacity, claimed);
            REACH_ASSERT(slot < capacity || input->animate);
            if (slot == capacity)
                continue;
            claimed[slot] = true;
            rows[slot] = {};
            reach_motion_set(&rows[slot].y, target);
            reach_motion_set(&rows[slot].opacity, input->animate ? 0.0f : 1.0f);
            reach_keyed_rows_report(out_changes, change_capacity, &change_count,
                                    REACH_KEYED_ROW_ALLOCATED, slot, index, REACH_KEYED_ROW_EMPTY);
        }
        else if (rows[slot].lifecycle == REACH_KEYED_ROW_RETIRING)
            reach_keyed_rows_report(out_changes, change_capacity, &change_count,
                                    REACH_KEYED_ROW_REVIVED, slot, index, REACH_KEYED_ROW_RETIRING);
        reach_keyed_row &row = rows[slot];
        row.lifecycle = REACH_KEYED_ROW_LIVE;
        row.index = index;
        if (!input->animate)
        {
            reach_motion_set(&row.y, target);
            reach_motion_set(&row.opacity, 1.0f);
            continue;
        }
        reach_keyed_rows_fade_to(input->theme, &row.opacity, 1, input->now_seconds);
        if (row.y.to != target)
            reach_motion_animate_spec_to_at(&row.y, target, smooth, input->now_seconds);
    }
    return change_count;
}

size_t reach_keyed_rows_release_settled(reach_keyed_row *rows, size_t capacity,
                                        reach_keyed_row_change *out_changes, size_t change_capacity)
{
    reach_keyed_rows_check(rows, capacity);
    size_t change_count = 0;
    for (size_t slot = 0; rows != nullptr && slot < capacity; ++slot)
    {
        reach_keyed_row &row = rows[slot];
        if (row.lifecycle == REACH_KEYED_ROW_RETIRING && !reach_motion_active(&row.opacity))
        {
            reach_keyed_rows_report(out_changes, change_capacity, &change_count,
                                    REACH_KEYED_ROW_RELEASED, slot, row.index, row.lifecycle);
            row = {};
        }
    }
    return change_count;
}

int32_t reach_keyed_rows_active(const reach_keyed_row *rows, size_t capacity)
{
    for (size_t slot = 0; rows != nullptr && slot < capacity; ++slot)
    {
        if (rows[slot].lifecycle != REACH_KEYED_ROW_EMPTY &&
            (reach_motion_active(&rows[slot].y) || reach_motion_active(&rows[slot].opacity)))
            return 1;
    }
    return 0;
}

double reach_keyed_rows_deadline(const reach_keyed_row *rows, size_t capacity)
{
    double deadline = 0;
    for (size_t slot = 0; rows != nullptr && slot < capacity; ++slot)
    {
        if (rows[slot].lifecycle != REACH_KEYED_ROW_EMPTY)
            deadline = fmax(deadline, fmax(reach_motion_deadline(&rows[slot].y),
                                           reach_motion_deadline(&rows[slot].opacity)));
    }
    return deadline;
}
