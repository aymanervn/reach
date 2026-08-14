#include "reach/features/stage.h"

#include "stage_common.h"

#include <new>

#define REACH_STAGE_DEFAULT_ANIMATION_SECONDS 0.28f

reach_result reach_stage_create(reach_stage **out_stage)
{
    REACH_ASSERT(out_stage != nullptr);
    if (out_stage == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_stage = nullptr;
    reach_stage *stage = new (std::nothrow) reach_stage();
    if (stage == nullptr)
    {
        return REACH_ERROR;
    }

    stage->state.animation_seconds = REACH_STAGE_DEFAULT_ANIMATION_SECONDS;
    reach_animation_manager_init(&stage->animations, stage->animation_tracks,
                                 REACH_STAGE_ANIMATION_COUNT);
    *out_stage = stage;
    return REACH_OK;
}

void reach_stage_destroy(reach_stage *stage)
{
    delete stage;
}

void reach_stage_attach_services(reach_stage *stage, reach_icon_service *icons,
                                 reach_window_tracking *windows)
{
    if (stage != nullptr)
    {
        stage->icons = icons;
        stage->windows = windows;
    }
}

const reach_stage_state *reach_stage_state_ptr(const reach_stage *stage)
{
    return stage != nullptr ? &stage->state : nullptr;
}

int32_t reach_stage_is_open(const reach_stage *stage)
{
    return stage != nullptr && stage->state.open ? 1 : 0;
}

int32_t reach_stage_animation_active(const reach_stage *stage)
{
    return stage != nullptr && stage->state.open &&
                   reach_animation_manager_active(&stage->animations,
                                                  REACH_STAGE_ANIMATION_PROGRESS)
               ? 1
               : 0;
}

void reach_stage_set_animation_seconds(reach_stage *stage, float seconds)
{
    if (stage != nullptr && seconds > 0.0f)
    {
        stage->state.animation_seconds = seconds;
    }
}

reach_result reach_stage_open(reach_stage *stage, reach_rect_f32 monitor_bounds, float dpi_scale,
                              const reach_stage_open_window *windows, size_t window_count)
{
    if (stage == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (window_count > 0 && windows == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_stage_state *state = &stage->state;
    float animation_seconds = state->animation_seconds > 0.0f
                                  ? state->animation_seconds
                                  : REACH_STAGE_DEFAULT_ANIMATION_SECONDS;
    *state = {};
    state->animation_seconds = animation_seconds;
    state->bounds = monitor_bounds;
    state->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;

    size_t count = window_count < REACH_STAGE_MAX_TILES ? window_count : REACH_STAGE_MAX_TILES;
    for (size_t index = 0; index < count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        tile->window = windows[index].window;
        tile->icon_id = windows[index].icon_id;
        tile->minimized = windows[index].minimized;
        tile->desktop = windows[index].desktop;
        tile->source_rect = windows[index].frame;
        tile->current_rect = windows[index].frame;
        (void)reach_copy_utf16(tile->label, 260, windows[index].label);
    }
    state->tile_count = count;

    if (count == 0)
    {
        return REACH_ERROR;
    }

    state->open = 1;
    state->closing = 0;
    state->progress = 0.0f;
    state->selected_index = 0;
    state->has_selection = 0;

    reach_animation_manager_start(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f, 1.0f,
                                  (double)state->animation_seconds, REACH_EASING_EASE_OUT);

    reach_stage_rebuild_layout(stage);
    reach_stage_apply_progress(stage);
    return REACH_OK;
}

void reach_stage_begin_close(reach_stage *stage)
{
    if (stage == nullptr || !stage->state.open || stage->state.closing)
    {
        return;
    }

    stage->state.closing = 1;
    stage->state.has_hover = 0;
    stage->closing_settled = 0;
    reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f,
                                       (double)stage->state.animation_seconds,
                                       REACH_EASING_EASE_OUT);
}

void reach_stage_force_close(reach_stage *stage)
{
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    float animation_seconds = state->animation_seconds;
    *state = {};
    state->animation_seconds = animation_seconds;
    stage->closing_settled = 0;
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
}

int32_t reach_stage_sync_window_states(reach_stage *stage)
{
    if (stage == nullptr || stage->windows == nullptr || !stage->state.open ||
        stage->state.closing)
    {
        return 0;
    }

    reach_stage_state *state = &stage->state;
    int32_t changed = 0;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        if (tile->desktop)
        {
            continue;
        }

        const reach_window_snapshot *snapshot =
            reach_window_tracking_window_by_id(stage->windows, tile->window);
        int32_t minimized =
            snapshot == nullptr ||
            reach_window_tracking_window_is_minimized(stage->windows, tile->window);
        if (minimized != tile->minimized)
        {
            tile->minimized = minimized;
            changed = 1;
        }
    }
    return changed;
}

size_t reach_stage_thumbnail_count(const reach_stage *stage)
{
    return stage != nullptr ? stage->state.tile_count : 0;
}

reach_result reach_stage_thumbnail_at(const reach_stage *stage, size_t index,
                                      reach_stage_thumbnail_placement *out_placement)
{
    if (stage == nullptr || out_placement == nullptr || index >= stage->state.tile_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_placement = {};

    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    int32_t suppressed_by_selection =
        state->closing && state->has_selection && index != state->selected_index;

    out_placement->window = tile->window;
    out_placement->destination = tile->current_rect;
    out_placement->opacity = 1.0f;
    out_placement->visible =
        state->open && !tile->minimized && !suppressed_by_selection ? 1 : 0;
    out_placement->minimized = tile->minimized;
    out_placement->desktop = tile->desktop;
    return REACH_OK;
}

static void reach_stage_capsule_reset(void *capsule)
{
    reach_stage_force_close(static_cast<reach_stage *>(capsule));
}

static void reach_stage_capsule_tick(void *capsule, double delta_seconds,
                                     reach_feature_tick_result *out)
{
    reach_stage *stage = static_cast<reach_stage *>(capsule);
    if (stage == nullptr || !stage->state.open)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    int32_t was_active =
        reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
    if (!was_active && !state->closing)
    {
        return;
    }

    reach_animation_manager_tick(&stage->animations, delta_seconds);
    state->progress =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);

    if (state->closing &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS))
    {
        if (stage->closing_settled)
        {
            reach_stage_force_close(stage);
        }
        else
        {
            stage->closing_settled = 1;
            reach_stage_apply_progress(stage);
        }
    }
    else
    {
        reach_stage_apply_progress(stage);
    }

    if (out != nullptr)
    {
        out->redraw = 1;
        out->request_update = 1;
    }
}

static int32_t reach_stage_capsule_is_open(const void *capsule)
{
    return reach_stage_is_open(static_cast<const reach_stage *>(capsule));
}

static void reach_stage_capsule_force_close(void *capsule)
{
    reach_stage_force_close(static_cast<reach_stage *>(capsule));
}

static void reach_stage_capsule_on_game_mode(void *capsule, int32_t enabled)
{
    if (enabled)
    {
        reach_stage_force_close(static_cast<reach_stage *>(capsule));
    }
}

static int32_t reach_stage_capsule_needs_frame(const void *capsule)
{
    return reach_stage_animation_active(static_cast<const reach_stage *>(capsule));
}

static int32_t reach_stage_capsule_wants_pointer_move(const void *capsule)
{
    return reach_stage_is_open(static_cast<const reach_stage *>(capsule));
}

void reach_stage_handle_pointer(void *capsule, const reach_pointer_event *event,
                                reach_capsule_pointer_result *out);

const reach_feature_capsule_ops *reach_stage_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {reach_stage_capsule_reset,
                                                  reach_stage_capsule_tick,
                                                  reach_stage_capsule_is_open,
                                                  reach_stage_capsule_force_close,
                                                  reach_stage_capsule_on_game_mode,
                                                  reach_stage_capsule_needs_frame,
                                                  reach_stage_capsule_wants_pointer_move,
                                                  reach_stage_handle_pointer};
    return &ops;
}

const reach_ui_event_type *reach_stage_activation_events(size_t *out_count)
{
    static const reach_ui_event_type events[] = {REACH_UI_EVENT_STAGE_TOGGLE};
    if (out_count != nullptr)
    {
        *out_count = sizeof(events) / sizeof(events[0]);
    }
    return events;
}
