#include "reach/features/stage.h"

#include "stage_common.h"

#include <new>

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

    stage->state.animation_seconds = reach_stage_animation_seconds_default();
    reach_animation_manager_init(&stage->animations, stage->animation_tracks,
                                 REACH_STAGE_ANIMATION_COUNT);
    reach_pressable_init(&stage->pressable);
    *out_stage = stage;
    return REACH_OK;
}

void reach_stage_destroy(reach_stage *stage)
{
    delete stage;
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
                   reach_animation_manager_any_active(&stage->animations)
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

static int32_t reach_stage_has_desktop(const reach_stage_state *state)
{
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (!state->tiles[index].departing && state->tiles[index].desktop)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_stage_has_apps(const reach_stage_state *state)
{
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (!state->tiles[index].departing && !state->tiles[index].desktop)
        {
            return 1;
        }
    }
    return 0;
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
                                  : reach_stage_animation_seconds_default();
    *state = {};
    reach_pressable_reset(&stage->pressable, nullptr);
    stage->pressable_generation = 0;
    state->animation_seconds = animation_seconds;
    state->bounds = monitor_bounds;
    state->desktop_bounds = monitor_bounds;
    state->dpi_scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;

    size_t count = window_count < REACH_STAGE_MAX_TILES ? window_count : REACH_STAGE_MAX_TILES;
    for (size_t index = 0; index < count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        tile->window = windows[index].window;
        tile->icon_id = windows[index].icon_id;
        tile->minimized = windows[index].minimized;
        tile->desktop = windows[index].desktop;
        tile->monitor_index = windows[index].monitor_index;
        tile->monitor_portrait = windows[index].monitor_portrait;
        tile->presence = 1.0f;
        tile->presence_from = 1.0f;
        tile->source_rect = windows[index].frame;
        tile->current_rect = windows[index].frame;
        (void)reach_copy_utf16(tile->label, 260, windows[index].label);
    }
    state->tile_count = count;
    state->tile_generation = 1;

    if (count == 0)
    {
        return REACH_ERROR;
    }

    state->open = 1;
    state->backdrop_opacity = 1.0f;
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP, 1.0f);
    state->closing = 0;
    state->progress = 0.0f;
    state->desktop_progress = 0.0f;
    state->reflow = 1.0f;
    state->close_hover = 0.0f;

    if (reach_stage_has_apps(state))
    {
        reach_animation_manager_start(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f,
                                      1.0f, (double)state->animation_seconds,
                                      REACH_EASING_EASE_OUT);
    }
    else
    {
        reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 1.0f);
        state->progress = 1.0f;
    }
    if (reach_stage_has_desktop(state))
    {
        reach_animation_manager_start(
            &stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS, 0.0f, 1.0f,
            reach_stage_desktop_animation_seconds(state->animation_seconds), REACH_EASING_EASE_OUT);
    }
    else
    {
        reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS,
                                    1.0f);
        state->desktop_progress = 1.0f;
    }
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_REFLOW, 1.0f);
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f);
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_RETARGET, 0.0f);

    reach_stage_rebuild_layout(stage);
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        state->tiles[index].reflow_from = state->tiles[index].target_rect;
    }
    reach_stage_apply_progress(stage);
    return REACH_OK;
}

int32_t reach_stage_set_desktop_bounds(reach_stage *stage, reach_rect_f32 bounds)
{
    if (stage == nullptr || stage->state.closing || bounds.width <= 0.0f || bounds.height <= 0.0f ||
        reach_rect_equal(stage->state.desktop_bounds, bounds))
    {
        return 0;
    }

    stage->state.desktop_bounds = bounds;
    if (!stage->state.open)
    {
        return 1;
    }

    if (stage->state.progress <= 0.0f)
    {
        reach_stage_rebuild_layout(stage);
        for (size_t index = 0; index < stage->state.tile_count; ++index)
        {
            stage->state.tiles[index].reflow_from = stage->state.tiles[index].target_rect;
        }
        reach_stage_apply_progress(stage);
    }
    else
    {
        reach_stage_start_reflow(stage);
    }
    return 1;
}

void reach_stage_start_reflow(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        tile->reflow_from =
            reach_stage_interpolate_rect(tile->reflow_from, tile->target_rect, state->reflow);
        tile->presence_from = tile->presence;
        if (tile->departing)
        {
            tile->target_rect = tile->reflow_from;
        }
    }

    reach_stage_rebuild_layout(stage);

    state->reflow = 0.0f;
    reach_animation_manager_start(&stage->animations, REACH_STAGE_ANIMATION_REFLOW, 0.0f, 1.0f,
                                  reach_stage_reflow_seconds(), REACH_EASING_EASE_IN_OUT);
    reach_stage_apply_progress(stage);
}

void reach_stage_settle_reflow(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    size_t kept = 0;
    int32_t dropped = 0;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (state->tiles[index].departing)
        {
            dropped = 1;
            continue;
        }
        if (kept != index)
        {
            state->tiles[kept] = state->tiles[index];
        }
        state->tiles[kept].reflow_from = state->tiles[kept].target_rect;
        state->tiles[kept].presence_from = state->tiles[kept].presence;
        kept++;
    }

    for (size_t index = kept; index < state->tile_count; ++index)
    {
        state->tiles[index] = {};
    }
    state->tile_count = kept;

    if (dropped)
    {
        state->tile_generation++;
        state->has_hover = 0;
        state->hover_index = 0;
        state->close_hover_index = 0;
        state->close_hover = 0.0f;
        reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f);
    }
}

void reach_stage_depart_tile(reach_stage *stage, size_t index)
{
    if (stage == nullptr || index >= stage->state.tile_count)
    {
        return;
    }

    reach_stage_tile *tile = &stage->state.tiles[index];
    if (tile->departing)
    {
        return;
    }

    tile->departing = 1;
    stage->state.has_hover = 0;
    reach_stage_start_reflow(stage);
}

size_t reach_stage_tile_generation(const reach_stage *stage)
{
    return stage != nullptr ? stage->state.tile_generation : 0;
}

static reach_rect_f32 reach_stage_minimized_close_destination(const reach_stage_state *state,
                                                              reach_rect_f32 rect)
{
    rect.x = state->bounds.x - rect.width;
    rect.y = state->bounds.y - rect.height;
    return rect;
}

static reach_rect_f32 reach_stage_close_destination(const reach_stage_state *state,
                                                    const reach_stage_tile *tile,
                                                    reach_rect_f32 minimized_rect)
{
    return tile->minimized ? reach_stage_minimized_close_destination(state, minimized_rect)
                           : tile->source_rect;
}

void reach_stage_begin_close(reach_stage *stage)
{
    if (stage == nullptr || !stage->state.open || stage->state.closing)
    {
        return;
    }

    stage->state.closing = 1;
    stage->state.close_phase = REACH_STAGE_CLOSE_MOVING;
    stage->state.close_aligned_committed = 0;
    stage->state.retarget_progress = 0.0f;
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_RETARGET, 0.0f);
    for (size_t index = 0; index < stage->state.tile_count; ++index)
    {
        reach_stage_tile *tile = &stage->state.tiles[index];
        tile->close_from_rect = tile->current_rect;
        tile->close_destination_rect =
            reach_stage_close_destination(&stage->state, tile, tile->close_from_rect);
        tile->close_from_progress =
            tile->desktop ? stage->state.desktop_progress : stage->state.progress;
        tile->close_retargeting = 0;
    }
    reach_pressable_reset(&stage->pressable, nullptr);
    stage->pressable_generation = 0;
    stage->state.has_hover = 0;
    reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f,
                                       reach_stage_close_hover_seconds(), REACH_EASING_EASE_OUT);
    if (reach_stage_has_apps(&stage->state))
    {
        reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f,
                                           (double)stage->state.animation_seconds,
                                           REACH_EASING_EASE_OUT);
    }
    else
    {
        reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f);
    }
    if (reach_stage_has_desktop(&stage->state))
    {
        reach_animation_manager_animate_to(
            &stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS, 0.0f,
            reach_stage_desktop_animation_seconds(stage->state.animation_seconds),
            REACH_EASING_EASE_OUT);
    }
    else
    {
        reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS,
                                    0.0f);
    }
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
    reach_pressable_reset(&stage->pressable, nullptr);
    stage->pressable_generation = 0;
    state->animation_seconds = animation_seconds;
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_RETARGET);
}

static int32_t reach_stage_holds_window(const reach_stage_state *state, uintptr_t window)
{
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (state->tiles[index].window == window)
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_stage_update_closing_windows(reach_stage *stage,
                                                  const reach_stage_open_window *windows,
                                                  size_t window_count)
{
    reach_stage_state *state = &stage->state;
    int32_t changed = 0;
    int32_t restart_retarget = 0;
    int32_t presentation_changed = 0;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        const reach_stage_open_window *source = nullptr;
        for (size_t candidate = 0; candidate < window_count; ++candidate)
        {
            if (windows[candidate].window == tile->window)
            {
                source = &windows[candidate];
                break;
            }
        }
        if (source == nullptr)
        {
            continue;
        }

        int32_t frame_changed = !reach_rect_equal(tile->source_rect, source->frame);
        int32_t minimized_changed = tile->minimized != source->minimized;
        if (frame_changed)
        {
            tile->source_rect = source->frame;
            changed = 1;
        }
        if (minimized_changed)
        {
            tile->minimized = source->minimized;
            presentation_changed = 1;
            changed = 1;
        }
        reach_rect_f32 destination = tile->source_rect;
        if (tile->minimized)
        {
            destination = minimized_changed
                              ? reach_stage_minimized_close_destination(state, tile->current_rect)
                              : tile->close_destination_rect;
        }
        if (!reach_rect_equal(tile->close_destination_rect, destination))
        {
            float progress = tile->desktop ? state->desktop_progress : state->progress;
            tile->close_from_rect = tile->current_rect;
            tile->close_from_progress = progress;
            tile->close_destination_rect = destination;
            if (progress <= 0.0f || tile->close_retargeting)
            {
                tile->close_retargeting = 1;
                restart_retarget = 1;
            }
        }
        if (tile->icon_id != source->icon_id)
        {
            tile->icon_id = source->icon_id;
            presentation_changed = 1;
            changed = 1;
        }
    }

    if (restart_retarget)
    {
        for (size_t index = 0; index < state->tile_count; ++index)
        {
            reach_stage_tile *tile = &state->tiles[index];
            if (tile->close_retargeting)
            {
                tile->close_from_rect = tile->current_rect;
            }
        }
        state->close_phase = REACH_STAGE_CLOSE_MOVING;
        state->close_aligned_committed = 0;
        state->retarget_progress = 1.0f;
        reach_animation_manager_start(&stage->animations, REACH_STAGE_ANIMATION_RETARGET, 1.0f,
                                      0.0f, reach_theme_default()->surface_close_seconds,
                                      REACH_EASING_EASE_OUT);
    }
    else if (presentation_changed && state->close_phase == REACH_STAGE_CLOSE_ALIGNED)
    {
        state->close_aligned_committed = 0;
    }
    return changed;
}

int32_t reach_stage_update_windows(reach_stage *stage, const reach_stage_open_window *windows,
                                   size_t window_count)
{
    if (stage == nullptr || !stage->state.open)
    {
        return 0;
    }
    if (window_count > 0 && windows == nullptr)
    {
        return 0;
    }
    if (window_count > REACH_STAGE_MAX_TILES)
    {
        window_count = REACH_STAGE_MAX_TILES;
    }
    if (stage->state.closing)
    {
        return reach_stage_update_closing_windows(stage, windows, window_count);
    }

    reach_stage_state *state = &stage->state;
    int32_t matched[REACH_STAGE_MAX_TILES] = {};
    int32_t changed = 0;
    int32_t tiles_changed = 0;

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        if (tile->departing)
        {
            continue;
        }

        const reach_stage_open_window *source = nullptr;
        for (size_t candidate = 0; candidate < window_count; ++candidate)
        {
            if (!matched[candidate] && windows[candidate].window == tile->window)
            {
                matched[candidate] = 1;
                source = &windows[candidate];
                break;
            }
        }

        if (source == nullptr)
        {
            tile->departing = 1;
            tiles_changed = 1;
            continue;
        }

        if (source->minimized != tile->minimized)
        {
            tile->minimized = source->minimized;
            changed = 1;
        }
        if (source->icon_id != tile->icon_id)
        {
            tile->icon_id = source->icon_id;
            changed = 1;
        }
    }

    for (size_t candidate = 0; candidate < window_count; ++candidate)
    {
        if (state->tile_count >= REACH_STAGE_MAX_TILES)
        {
            break;
        }
        if (matched[candidate] || reach_stage_holds_window(state, windows[candidate].window))
        {
            continue;
        }

        reach_stage_tile *tile = &state->tiles[state->tile_count];
        *tile = {};
        tile->window = windows[candidate].window;
        tile->icon_id = windows[candidate].icon_id;
        tile->minimized = windows[candidate].minimized;
        tile->desktop = windows[candidate].desktop;
        tile->monitor_index = windows[candidate].monitor_index;
        tile->monitor_portrait = windows[candidate].monitor_portrait;
        tile->source_rect = windows[candidate].frame;
        tile->target_rect = windows[candidate].frame;
        tile->reflow_from = windows[candidate].frame;
        tile->current_rect = windows[candidate].frame;
        (void)reach_copy_utf16(tile->label, 260, windows[candidate].label);
        state->tile_count++;
        tiles_changed = 1;
    }

    if (tiles_changed)
    {
        state->tile_generation++;
        reach_stage_start_reflow(stage);
        return 1;
    }

    return changed;
}

void reach_stage_refresh_tile_frames(reach_stage *stage, const reach_stage_open_window *windows,
                                     size_t window_count)
{
    if (stage == nullptr || !stage->state.open)
    {
        return;
    }
    if (window_count > 0 && windows == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        if (tile->departing)
        {
            continue;
        }

        for (size_t candidate = 0; candidate < window_count; ++candidate)
        {
            if (windows[candidate].window == tile->window)
            {
                tile->source_rect = windows[candidate].frame;
                break;
            }
        }
    }
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

    out_placement->window = tile->window;
    out_placement->destination = tile->current_rect;
    out_placement->source_screen = tile->source_rect;
    out_placement->opacity = tile->presence;
    out_placement->backdrop_opacity = state->backdrop_opacity;
    out_placement->visible =
        state->open && !state->close_failed && !tile->minimized && tile->presence > 0.0f ? 1 : 0;
    out_placement->source_screen_valid = tile->desktop;
    out_placement->minimized = tile->minimized;
    out_placement->desktop = tile->desktop;
    out_placement->behind_surface = tile->desktop;
    return REACH_OK;
}

static void reach_stage_capsule_reset(void *capsule)
{
    reach_stage_force_close(static_cast<reach_stage *>(capsule));
}

static void reach_stage_begin_reveal(reach_stage *stage)
{
    stage->state.close_phase = REACH_STAGE_CLOSE_REVEALING;
    reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP, 0.0f,
                                       reach_theme_default()->surface_close_seconds,
                                       REACH_EASING_EASE_OUT);
}

static void reach_stage_capsule_presentation_committed(void *capsule, reach_result result,
                                                       reach_feature_tick_result *out)
{
    reach_stage *stage = static_cast<reach_stage *>(capsule);
    if (result != REACH_OK && stage->state.closing &&
        stage->state.close_phase < REACH_STAGE_CLOSE_REVEALING)
    {
        stage->state.close_failed = 1;
        reach_stage_begin_reveal(stage);
    }
    else if (stage->state.close_phase == REACH_STAGE_CLOSE_ALIGNED)
    {
        stage->state.close_aligned_committed = 1;
        if (!stage->state.close_handoff_pending)
        {
            reach_stage_begin_reveal(stage);
        }
    }
    else if (stage->state.close_phase == REACH_STAGE_CLOSE_TRANSPARENT)
    {
        stage->state.close_phase = REACH_STAGE_CLOSE_FINISHED;
    }
    out->redraw = 1;
    out->request_update = 1;
}

static void reach_stage_capsule_set_close_handoff_pending(void *capsule, int32_t pending,
                                                          reach_feature_tick_result *out)
{
    reach_stage *stage = static_cast<reach_stage *>(capsule);
    if (stage == nullptr)
    {
        return;
    }
    stage->state.close_handoff_pending = pending ? 1 : 0;
    if (!stage->state.close_handoff_pending && stage->state.closing &&
        stage->state.close_phase == REACH_STAGE_CLOSE_ALIGNED &&
        stage->state.close_aligned_committed)
    {
        reach_stage_begin_reveal(stage);
    }
    if (out != nullptr)
    {
        out->redraw = 1;
        out->request_update = 1;
    }
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
    if (state->close_phase == REACH_STAGE_CLOSE_FINISHED)
    {
        reach_stage_force_close(stage);
        out->redraw = 1;
        out->request_update = 1;
        return;
    }
    int32_t was_active = reach_animation_manager_any_active(&stage->animations);
    if (!was_active && !state->closing)
    {
        return;
    }

    int32_t reflow_was_active =
        reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);
    int32_t retarget_was_active =
        reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_RETARGET);

    reach_animation_manager_tick(&stage->animations, delta_seconds);
    state->progress =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
    state->desktop_progress =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS);
    state->backdrop_opacity =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP);
    state->reflow = reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);
    state->close_hover =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER);
    state->retarget_progress =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_RETARGET);

    reach_stage_apply_progress(stage);

    if (retarget_was_active &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_RETARGET))
    {
        for (size_t index = 0; index < state->tile_count; ++index)
        {
            state->tiles[index].close_retargeting = 0;
        }
    }

    if (reflow_was_active &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_REFLOW))
    {
        reach_stage_settle_reflow(stage);
    }

    if (state->close_phase == REACH_STAGE_CLOSE_MOVING &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS) &&
        !reach_animation_manager_active(&stage->animations,
                                        REACH_STAGE_ANIMATION_DESKTOP_PROGRESS) &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_RETARGET))
    {
        state->close_phase = REACH_STAGE_CLOSE_ALIGNED;
        state->close_aligned_committed = 0;
    }
    if (state->close_phase == REACH_STAGE_CLOSE_REVEALING &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP))
    {
        state->close_phase = REACH_STAGE_CLOSE_TRANSPARENT;
    }

    if (out != nullptr)
    {
        out->redraw = 1;
        out->request_update = 1;
    }
}

static int32_t reach_stage_is_closing(const reach_stage *stage)
{
    return stage != nullptr && stage->state.open && stage->state.closing ? 1 : 0;
}

static int32_t reach_stage_capsule_is_open(const void *capsule)
{
    const reach_stage *stage = static_cast<const reach_stage *>(capsule);
    return reach_stage_is_open(stage) && !reach_stage_is_closing(stage);
}

static int32_t reach_stage_capsule_presentation_visible(const void *capsule)
{
    return reach_stage_is_open(static_cast<const reach_stage *>(capsule));
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
    const reach_stage *stage = static_cast<const reach_stage *>(capsule);
    if (stage == nullptr || !stage->state.open)
    {
        return 0;
    }
    if (reach_stage_animation_active(stage))
    {
        return 1;
    }
    return stage->state.close_phase == REACH_STAGE_CLOSE_MOVING ||
                   (stage->state.close_phase == REACH_STAGE_CLOSE_ALIGNED &&
                    !stage->state.close_aligned_committed) ||
                   stage->state.close_phase == REACH_STAGE_CLOSE_TRANSPARENT ||
                   stage->state.close_phase == REACH_STAGE_CLOSE_FINISHED
               ? 1
               : 0;
}

static int32_t reach_stage_capsule_wants_pointer_move(const void *capsule)
{
    return reach_stage_is_open(static_cast<const reach_stage *>(capsule));
}

static int32_t reach_stage_capsule_pointer_sequence_active(const void *capsule)
{
    const reach_stage *stage = static_cast<const reach_stage *>(capsule);
    return stage != nullptr && reach_pressable_tracking(&stage->pressable);
}

static void reach_stage_capsule_surface_geometry(const void *capsule,
                                                 reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_stage *stage = static_cast<const reach_stage *>(capsule);
    if (stage != nullptr)
    {
        out->visible_bounds = stage->state.bounds;
        out->synchronize_presentation = (stage->state.close_phase == REACH_STAGE_CLOSE_ALIGNED &&
                                         !stage->state.close_aligned_committed) ||
                                        stage->state.close_phase == REACH_STAGE_CLOSE_TRANSPARENT;
    }
}

void reach_stage_handle_pointer(void *capsule, const reach_pointer_event *event,
                                reach_capsule_pointer_result *out);

void reach_stage_attach_services(reach_stage *stage, reach_window_tracking *windows,
                                 reach_icon_service *icons, reach_app_control *apps)
{
    if (stage != nullptr)
    {
        stage->windows = windows;
        stage->icons = icons;
        stage->apps = apps;
    }
}

void reach_stage_set_display(reach_stage *stage, const reach_display_environment *display)
{
    if (stage != nullptr && display != nullptr)
    {
        stage->display = *display;
    }
}

static size_t reach_stage_monitor_index_for(const reach_stage *stage, reach_rect_f32 frame,
                                            int32_t *out_portrait)
{
    *out_portrait = 0;
    if (stage->display.monitor_count == 0)
    {
        return 0;
    }

    float center_x = frame.x + frame.width * 0.5f;
    float center_y = frame.y + frame.height * 0.5f;
    size_t match = stage->display.monitor_count;
    for (size_t index = 0;
         index < stage->display.monitor_count && match == stage->display.monitor_count; ++index)
    {
        reach_rect_f32 bounds = stage->display.monitors[index];
        if (center_x >= bounds.x && center_x < bounds.x + bounds.width && center_y >= bounds.y &&
            center_y < bounds.y + bounds.height)
        {
            match = index;
        }
    }
    if (match == stage->display.monitor_count)
    {
        match = 0;
    }

    reach_rect_f32 matched = stage->display.monitors[match];
    *out_portrait = matched.height > matched.width ? 1 : 0;

    size_t rank = 0;
    for (size_t index = 0; index < stage->display.monitor_count; ++index)
    {
        if (index == match)
        {
            continue;
        }
        reach_rect_f32 bounds = stage->display.monitors[index];
        if (bounds.x < matched.x || (bounds.x == matched.x && bounds.y < matched.y))
        {
            ++rank;
        }
    }
    return rank;
}

static size_t reach_stage_collect_windows(reach_stage *stage, reach_stage_open_window *out_windows,
                                          size_t capacity)
{
    if (stage == nullptr || out_windows == nullptr || capacity == 0)
    {
        return 0;
    }

    const reach_window_snapshot *windows = reach_window_tracking_windows(stage->windows);
    size_t window_count = reach_window_tracking_window_count(stage->windows);
    if (windows == nullptr)
    {
        return 0;
    }

    size_t collected = 0;
    for (size_t index = 0; index < window_count && collected < capacity; ++index)
    {
        const reach_window_snapshot *snapshot = &windows[index];
        if (!snapshot->visible || snapshot->id == 0)
        {
            continue;
        }

        reach_rect_f32 frame = {};
        if (reach_app_control_window_frame_bounds(stage->apps, snapshot->id, &frame) != REACH_OK ||
            frame.width <= 0.0f || frame.height <= 0.0f)
        {
            continue;
        }

        reach_stage_open_window *entry = &out_windows[collected];
        *entry = {};
        entry->window = snapshot->id;
        entry->label = snapshot->title;
        entry->minimized = snapshot->minimized;
        entry->frame = frame;
        int32_t portrait = 0;
        entry->monitor_index = (uint32_t)reach_stage_monitor_index_for(stage, frame, &portrait);
        entry->monitor_portrait = portrait;
        entry->icon_id =
            reach_icon_service_get(stage->icons, snapshot->icon_ref, stage->display.icon_size_px);
        collected++;
    }

    if (collected < capacity && stage->display.desktop_window != 0)
    {
        static const uint16_t desktop_label[] = {0};
        reach_stage_open_window *entry = &out_windows[collected];
        *entry = {};
        entry->window = stage->display.desktop_window;
        entry->label = desktop_label;
        entry->desktop = 1;
        entry->frame = stage->display.primary_bounds;
        int32_t portrait = 0;
        entry->monitor_index =
            (uint32_t)reach_stage_monitor_index_for(stage, entry->frame, &portrait);
        entry->monitor_portrait = portrait;
        collected++;
    }

    return collected;
}

int32_t reach_stage_set_open(reach_stage *stage, int32_t open)
{
    if (stage == nullptr)
    {
        return 0;
    }

    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_stage_collect_windows(stage, windows, REACH_STAGE_MAX_TILES);

    if (!open)
    {
        if (!reach_stage_is_open(stage))
        {
            return 0;
        }
        reach_stage_refresh_tile_frames(stage, windows, count);
        reach_stage_begin_close(stage);
        return 0;
    }

    if (reach_stage_is_open(stage) || count == 0)
    {
        return 0;
    }
    return reach_stage_open(stage, stage->display.primary_bounds, stage->display.dpi_scale, windows,
                            count) == REACH_OK;
}

int32_t reach_stage_sync_windows(reach_stage *stage)
{
    if (stage == nullptr || !reach_stage_is_open(stage))
    {
        return 0;
    }
    reach_stage_open_window windows[REACH_STAGE_MAX_TILES] = {};
    size_t count = reach_stage_collect_windows(stage, windows, REACH_STAGE_MAX_TILES);
    return reach_stage_update_windows(stage, windows, count);
}

const reach_feature_capsule_ops *reach_stage_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {reach_stage_capsule_reset,
                                                  reach_stage_capsule_tick,
                                                  reach_stage_capsule_is_open,
                                                  reach_stage_capsule_on_game_mode,
                                                  reach_stage_capsule_needs_frame,
                                                  reach_stage_capsule_wants_pointer_move,
                                                  reach_stage_handle_pointer,
                                                  reach_stage_capsule_pointer_sequence_active,
                                                  nullptr,
                                                  reach_stage_capsule_surface_geometry,
                                                  nullptr,
                                                  nullptr,
                                                  nullptr,
                                                  reach_stage_capsule_presentation_visible,
                                                  reach_stage_capsule_presentation_committed,
                                                  reach_stage_capsule_set_close_handoff_pending};
    return &ops;
}
