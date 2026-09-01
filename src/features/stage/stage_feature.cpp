#include "reach/features/stage.h"

#include "stage_common.h"

#include <new>

#define REACH_STAGE_BACKDROP_FADE_RATIO 0.35

static double reach_stage_backdrop_fade_seconds(const reach_stage *stage)
{
    double animation_seconds =
        stage != nullptr ? (double)stage->state.animation_seconds
                         : (double)reach_stage_animation_seconds_default();
    return animation_seconds * REACH_STAGE_BACKDROP_FADE_RATIO;
}

static void reach_stage_start_backdrop_close_fade(reach_stage *stage)
{
    if (stage == nullptr || !stage->state.closing ||
        reach_animation_manager_active(&stage->animations,
                                       REACH_STAGE_ANIMATION_BACKDROP_OPACITY) ||
        reach_animation_manager_target(&stage->animations,
                                       REACH_STAGE_ANIMATION_BACKDROP_OPACITY) <= 0.0f)
    {
        return;
    }

    double remaining_seconds = reach_animation_track_time_to_value(
        &stage->animation_tracks[REACH_STAGE_ANIMATION_PROGRESS], 0.0f);
    if (remaining_seconds <= 0.0 ||
        remaining_seconds > reach_stage_backdrop_fade_seconds(stage))
    {
        return;
    }

    reach_animation_manager_animate_to(
        &stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY, 0.0f, remaining_seconds,
        REACH_EASING_EASE_IN);
}

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
    state->closing = 0;
    state->progress = 0.0f;
    state->backdrop_opacity = 0.0f;
    state->reflow = 1.0f;
    state->close_hover = 0.0f;
    state->selected_index = 0;
    state->has_selection = 0;

    reach_animation_manager_start(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS, 0.0f, 1.0f,
                                  (double)state->animation_seconds, REACH_EASING_EASE_OUT);
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY, 0.0f);
    stage->backdrop_open_pending = 1;
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_REFLOW, 1.0f);
    reach_animation_manager_set(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f);

    reach_stage_rebuild_layout(stage);
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        state->tiles[index].reflow_from = state->tiles[index].target_rect;
    }
    reach_stage_apply_progress(stage);
    return REACH_OK;
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
        state->has_selection = 0;
        state->selected_index = 0;
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

void reach_stage_begin_close(reach_stage *stage)
{
    if (stage == nullptr || !stage->state.open || stage->state.closing)
    {
        return;
    }

    stage->state.closing = 1;
    stage->backdrop_open_pending = 0;
    reach_pressable_reset(&stage->pressable, nullptr);
    stage->pressable_generation = 0;
    stage->state.has_hover = 0;
    reach_animation_manager_set(
        &stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY,
        reach_animation_manager_value(&stage->animations,
                                      REACH_STAGE_ANIMATION_BACKDROP_OPACITY));
    reach_animation_manager_animate_to(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER, 0.0f,
                                       reach_stage_close_hover_seconds(), REACH_EASING_EASE_OUT);
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
    reach_pressable_reset(&stage->pressable, nullptr);
    stage->pressable_generation = 0;
    stage->backdrop_open_pending = 0;
    state->animation_seconds = animation_seconds;
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);
    reach_animation_manager_reset(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER);
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

int32_t reach_stage_update_windows(reach_stage *stage, const reach_stage_open_window *windows,
                                   size_t window_count)
{
    if (stage == nullptr || !stage->state.open || stage->state.closing)
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
    int32_t suppressed_by_selection =
        state->closing && state->has_selection && index != state->selected_index;

    out_placement->window = tile->window;
    out_placement->destination = tile->current_rect;
    out_placement->source_screen = tile->source_rect;
    out_placement->opacity = tile->presence;
    out_placement->visible =
        state->open && !tile->minimized && !suppressed_by_selection && tile->presence > 0.0f ? 1
                                                                                             : 0;
    out_placement->source_screen_valid = tile->desktop;
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
    reach_stage_start_backdrop_close_fade(stage);
    int32_t was_active = reach_animation_manager_any_active(&stage->animations);
    if (!was_active && !state->closing)
    {
        return;
    }

    int32_t reflow_was_active =
        reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);

    reach_animation_manager_tick(&stage->animations, delta_seconds);
    if (stage->backdrop_open_pending)
    {
        stage->backdrop_open_pending = 0;
        reach_animation_manager_start(
            &stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY, 0.0f, 1.0f,
            reach_stage_backdrop_fade_seconds(stage), REACH_EASING_EASE_OUT);
    }
    state->progress =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS);
    state->backdrop_opacity = reach_animation_manager_value(
        &stage->animations, REACH_STAGE_ANIMATION_BACKDROP_OPACITY);
    state->reflow = reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_REFLOW);
    state->close_hover =
        reach_animation_manager_value(&stage->animations, REACH_STAGE_ANIMATION_CLOSE_HOVER);

    reach_stage_apply_progress(stage);

    if (reflow_was_active &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_REFLOW))
    {
        reach_stage_settle_reflow(stage);
    }

    if (state->closing &&
        !reach_animation_manager_active(&stage->animations, REACH_STAGE_ANIMATION_PROGRESS))
    {
        reach_stage_force_close(stage);
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
    return reach_stage_animation_active(stage) || reach_stage_is_closing(stage);
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
    for (size_t index = 0; index < stage->display.monitor_count && match == stage->display.monitor_count;
         ++index)
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
        static const uint16_t desktop_label[] = {'D', 'e', 's', 'k', 't', 'o', 'p', 0};
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
                                                  reach_stage_capsule_presentation_visible};
    return &ops;
}
