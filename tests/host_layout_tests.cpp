#include "motion_clock_test_support.h"

#include "host_internal.h"

#include "reach/features/battery.h"
#include "reach/features/clipboard.h"
#include "reach/features/context_menu.h"
#include "reach/features/dock.h"
#include "reach/features/launcher.h"
#include "reach/features/quick_settings.h"
#include "reach/features/stage.h"
#include "reach/features/switcher.h"
#include "reach/features/system_hud.h"
#include "reach/features/top_bar.h"

#include <stdint.h>
#include <stdio.h>
#include <memory>

enum fake_window_call_kind
{
    FAKE_WINDOW_CALL_SHOW = 1,
    FAKE_WINDOW_CALL_HIDE = 2,
    FAKE_WINDOW_CALL_SET_TOPMOST = 3,
    FAKE_WINDOW_CALL_PLACE_BEHIND = 4,
    FAKE_WINDOW_CALL_SET_INPUT_PASSTHROUGH = 5
};

typedef struct fake_window_call
{
    fake_window_call_kind kind;
    reach_window_id window;
    reach_window_id target;
} fake_window_call;

static fake_window_call calls[32];
static size_t call_count;
static int failures;
static reach_host order_repair_host;
static reach_host app_band_host;
static reach_host manipulation_host;
static reach_host monitor_entry_host;
static reach_host transition_frame_host;
static reach_host registry_host;
static reach_host input_passthrough_host;
static reach_host generic_frame_host;
static reach_host native_overlay_host;
static reach_host closing_stage_host;
static reach_host focus_restore_host;
static reach_host foreground_reconciliation_host;
static reach_host closable_host;
static reach_host dismiss_guard_host;
static reach_host bar_conditions_host;
static reach_host bar_enter_host;
static reach_host pointer_release_host;
static reach_host popup_trigger_host;
static reach_host popup_owner_host;
static reach_host window_list_region_host;
static reach_host popup_render_host;
static reach_window_manipulation observed_manipulation;
static reach_point_i32 observed_pointer;
static reach_monitor_info primary_monitor = {1, {0, 0, 1000, 800}, {}, 96, 96, 1, 60};
static reach_rect_f32 observed_bounds;
static size_t observed_set_bounds_count;
static size_t render_count;
static float observed_last_command_alpha;
static float observed_presentation_opacity;
static size_t thumbnail_create_count;
static reach_window_id thumbnail_targets[256];
static int32_t thumbnail_released[256];
static size_t thumbnail_target_plane_create_count;
static size_t thumbnail_behind_plane_create_count;
static size_t thumbnail_place_count;
static size_t thumbnail_background_place_count;
static float thumbnail_background_alpha;
static size_t thumbnail_release_count;
static size_t thumbnail_bring_to_front_count;
static reach_window_thumbnail_id thumbnail_front_id;
static reach_window_id thumbnail_unavailable_source;
static reach_result thumbnail_placement_result = REACH_OK;
static int32_t captured_release_active;
static int32_t exclusive_release_open;
static size_t captured_release_count;
static size_t exclusive_release_count;
static int32_t fake_source_popup_trigger;
static size_t unrelated_popup_down_count;
static reach_window_id observed_foreground_window;
static int32_t foreground_window_available;

typedef struct fake_presentation_capsule
{
    int32_t open;
    int32_t needs_frame;
    int32_t presentation_visible;
} fake_presentation_capsule;

static int32_t fake_presentation_is_open(const void *capsule)
{
    const fake_presentation_capsule *state =
        static_cast<const fake_presentation_capsule *>(capsule);
    return state != nullptr && state->open;
}

static int32_t fake_presentation_needs_frame(const void *capsule)
{
    const fake_presentation_capsule *state =
        static_cast<const fake_presentation_capsule *>(capsule);
    return state != nullptr && state->needs_frame;
}

static int32_t fake_presentation_visible(const void *capsule)
{
    const fake_presentation_capsule *state =
        static_cast<const fake_presentation_capsule *>(capsule);
    return state != nullptr && state->presentation_visible;
}

static int32_t fake_captured_release_active(const void *capsule)
{
    return capsule != nullptr && captured_release_active;
}

static int32_t fake_exclusive_release_open(const void *capsule)
{
    return capsule != nullptr && exclusive_release_open;
}

static void fake_captured_release(void *capsule, const reach_pointer_event *event,
                                  reach_capsule_pointer_result *out)
{
    if (capsule != nullptr && event != nullptr && out != nullptr &&
        event->kind == REACH_POINTER_EVENT_UP)
    {
        ++captured_release_count;
        out->handled = 1;
    }
}

static void fake_exclusive_release(void *capsule, const reach_pointer_event *event,
                                   reach_capsule_pointer_result *out)
{
    if (capsule != nullptr && event != nullptr && out != nullptr &&
        event->kind == REACH_POINTER_EVENT_UP)
    {
        ++exclusive_release_count;
        out->handled = 1;
    }
}

static void fake_popup_trigger_source(void *capsule, const reach_pointer_event *event,
                                      reach_capsule_pointer_result *out)
{
    if (capsule != nullptr && event != nullptr && out != nullptr &&
        event->kind == REACH_POINTER_EVENT_DOWN)
    {
        out->handled = 1;
        out->control = {REACH_TOP_BAR_CONTROL_BATTERY, 0, 1, fake_source_popup_trigger};
    }
}

static void fake_unrelated_popup_down(void *capsule, const reach_pointer_event *event,
                                      reach_capsule_pointer_result *out)
{
    if (capsule != nullptr && event != nullptr && out != nullptr &&
        event->kind == REACH_POINTER_EVENT_DOWN)
    {
        ++unrelated_popup_down_count;
        out->handled = 1;
        out->cancel_source_sequence = 1;
    }
}

static reach_result fake_get_window_manipulation(reach_input_source *source,
                                                 reach_window_manipulation *out_manipulation)
{
    (void)source;
    if (out_manipulation == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_manipulation = observed_manipulation;
    return REACH_OK;
}

static reach_result fake_get_pointer_position(reach_input_source *source,
                                              reach_point_i32 *out_position)
{
    (void)source;
    if (out_position == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_position = observed_pointer;
    return REACH_OK;
}

static reach_window_id fake_current_foreground(const reach_foreground_watcher *watcher)
{
    (void)watcher;
    return observed_foreground_window;
}

static size_t fake_foreground_window_count(const reach_window_manager *manager)
{
    (void)manager;
    return foreground_window_available ? 1 : 0;
}

static reach_result fake_foreground_window_at(const reach_window_manager *manager, size_t index,
                                              reach_window_snapshot *out_window)
{
    (void)manager;
    if (!foreground_window_available || index != 0 || out_window == nullptr)
    {
        return REACH_ERROR;
    }
    *out_window = {};
    out_window->id = observed_foreground_window;
    out_window->visible = 1;
    out_window->title[0] = 'A';
    return REACH_OK;
}

static reach_result fake_preview_frame_bounds(const reach_window_manager *manager,
                                              reach_window_id window, reach_rect_f32 *out)
{
    (void)manager;
    (void)window;
    *out = {100.0f, 100.0f, 800.0f, 600.0f};
    return REACH_OK;
}

static size_t fake_monitor_count(const reach_monitor_list *list)
{
    (void)list;
    return 1;
}

static const reach_monitor_info *fake_primary_monitor(const reach_monitor_list *list)
{
    (void)list;
    return &primary_monitor;
}

static reach_window_id fake_window_id(const reach_platform_window *window)
{
    return (reach_window_id)(uintptr_t)window;
}

static reach_result fake_thumbnail_create(reach_window_thumbnails *thumbnails,
                                          reach_window_id target, reach_window_id source,
                                          reach_window_thumbnail_plane plane,
                                          reach_window_thumbnail_id *out_id)
{
    (void)thumbnails;
    if (target == 0 || source == 0 || out_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (source == thumbnail_unavailable_source)
    {
        return REACH_ERROR;
    }
    if (plane == REACH_WINDOW_THUMBNAIL_PLANE_TARGET)
    {
        thumbnail_target_plane_create_count++;
    }
    else if (plane == REACH_WINDOW_THUMBNAIL_PLANE_BEHIND_TARGET)
    {
        thumbnail_behind_plane_create_count++;
    }
    else
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_id = ++thumbnail_create_count;
    thumbnail_targets[*out_id] = target;
    thumbnail_released[*out_id] = 0;
    return REACH_OK;
}

static reach_result fake_thumbnail_set_placement(reach_window_thumbnails *thumbnails,
                                                 reach_window_thumbnail_id id,
                                                 const reach_window_thumbnail_placement *placement)
{
    (void)thumbnails;
    if (id == REACH_WINDOW_THUMBNAIL_NONE || placement == nullptr || thumbnail_released[id])
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (thumbnail_placement_result != REACH_OK)
    {
        return thumbnail_placement_result;
    }
    thumbnail_place_count++;
    if (placement->background_visible)
    {
        thumbnail_background_place_count++;
        thumbnail_background_alpha = placement->background.a;
    }
    return REACH_OK;
}

static reach_result fake_thumbnail_bring_to_front(reach_window_thumbnails *thumbnails,
                                                  reach_window_thumbnail_id id)
{
    (void)thumbnails;
    if (id == REACH_WINDOW_THUMBNAIL_NONE)
    {
        return REACH_INVALID_ARGUMENT;
    }
    thumbnail_bring_to_front_count++;
    thumbnail_front_id = id;
    return REACH_OK;
}

static reach_result fake_thumbnail_release(reach_window_thumbnails *thumbnails,
                                           reach_window_thumbnail_id id)
{
    (void)thumbnails;
    if (id == REACH_WINDOW_THUMBNAIL_NONE)
    {
        return REACH_INVALID_ARGUMENT;
    }
    thumbnail_release_count++;
    thumbnail_released[id] = 1;
    return REACH_OK;
}

static void record_call(fake_window_call_kind kind, const reach_platform_window *window,
                        reach_window_id target)
{
    if (call_count < sizeof(calls) / sizeof(calls[0]))
    {
        calls[call_count++] = {kind, fake_window_id(window), target};
    }
}

static reach_result fake_show(reach_platform_window *window)
{
    record_call(FAKE_WINDOW_CALL_SHOW, window, 0);
    return REACH_OK;
}

static reach_result fake_hide(reach_platform_window *window)
{
    record_call(FAKE_WINDOW_CALL_HIDE, window, 0);
    return REACH_OK;
}

static reach_result fake_set_bounds(reach_platform_window *window, reach_rect_f32 bounds)
{
    (void)window;
    observed_bounds = bounds;
    ++observed_set_bounds_count;
    return REACH_OK;
}

static reach_result fake_begin_frame(reach_render_backend *backend)
{
    (void)backend;
    return REACH_OK;
}

static reach_result fake_end_frame(reach_render_backend *backend)
{
    (void)backend;
    return REACH_OK;
}

static reach_result fake_execute(reach_render_backend *backend,
                                 const reach_render_command_buffer *commands)
{
    (void)backend;
    if (commands != nullptr && commands->count > 0)
    {
        ++render_count;
        observed_last_command_alpha = commands->commands[commands->count - 1].color.a;
        observed_presentation_opacity =
            commands->has_presentation ? commands->presentation.opacity.to : -1.0f;
    }
    return REACH_OK;
}

static reach_result fake_set_topmost(reach_platform_window *window, int32_t enabled)
{
    record_call(FAKE_WINDOW_CALL_SET_TOPMOST, window, (reach_window_id)enabled);
    return REACH_OK;
}

static reach_result fake_place_behind(reach_platform_window *window, reach_window_id target)
{
    record_call(FAKE_WINDOW_CALL_PLACE_BEHIND, window, target);
    return REACH_OK;
}

static reach_result fake_set_input_passthrough(reach_platform_window *window, int32_t enabled)
{
    record_call(FAKE_WINDOW_CALL_SET_INPUT_PASSTHROUGH, window, (reach_window_id)enabled);
    return REACH_OK;
}

static reach_window_id fake_native_id(const reach_platform_window *window)
{
    return fake_window_id(window);
}

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        fprintf(stderr, "FAILED: %s\n", message);
    }
}

static size_t count_calls(fake_window_call_kind kind)
{
    size_t count = 0;
    for (size_t index = 0; index < call_count; ++index)
    {
        if (calls[index].kind == kind)
        {
            ++count;
        }
    }
    return count;
}

static int32_t has_call(fake_window_call_kind kind, reach_window_id window, reach_window_id target)
{
    for (size_t index = 0; index < call_count; ++index)
    {
        if (calls[index].kind == kind && calls[index].window == window &&
            calls[index].target == target)
        {
            return 1;
        }
    }
    return 0;
}

static void attach_window(reach_host *host, reach_surface_id id, reach_surface_role role,
                          reach_window_id window_id, int32_t layer)
{
    reach_surface_runtime *surface = host->feature_runtimes[id].surface;
    surface->window.window = (reach_platform_window *)(uintptr_t)window_id;
    surface->window.ops.show = fake_show;
    surface->window.ops.hide = fake_hide;
    surface->window.ops.set_topmost = fake_set_topmost;
    surface->window.ops.place_behind = fake_place_behind;
    surface->window.ops.native_id = fake_native_id;

    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    expect_true(runtime->definition->surface.role == role,
                "test window role matches its registered definition");
    expect_true(runtime->definition->surface.layer == layer,
                "test window layer matches its registered definition");

    reach_layout_participant participant = 0;
    expect_true(reach_layout_register(&host->layout_manager, layer, &participant) == REACH_OK,
                "layout participant registers");
    host->surface_participants[id] = participant;
    host->layout_targets[participant].runtime = runtime;
}

static void initialize_host(reach_host *host)
{
    reach_host_init_feature_registry(host);
    attach_window(host, REACH_SURFACE_ID_TOP_BAR, REACH_SURFACE_TOP_BAR, 101, 0);
    attach_window(host, REACH_SURFACE_ID_DOCK, REACH_SURFACE_DOCK, 102, 110);
    attach_window(host, REACH_SURFACE_ID_STAGE, REACH_SURFACE_STAGE, 103, 50);
    reach_layout_register_override(&host->layout_manager,
                                   host->surface_participants[REACH_SURFACE_ID_TOP_BAR],
                                   REACH_LAYOUT_CONDITION_BARS_FORCED, 130);
    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_FORCED, 1);
}

static void test_order_invalidation_rechains_without_replaying_visibility(void)
{
    reach_host *host = &order_repair_host;
    initialize_host(host);

    call_count = 0;
    reach_host_apply_layout(host);
    expect_true(count_calls(FAKE_WINDOW_CALL_SHOW) == 3,
                "the first apply shows every visible surface");
    expect_true(count_calls(FAKE_WINDOW_CALL_SET_TOPMOST) == 1,
                "the first apply seeds the topmost chain once");
    expect_true(count_calls(FAKE_WINDOW_CALL_PLACE_BEHIND) == 2,
                "the first apply orders the remaining surfaces");

    call_count = 0;
    reach_host_apply_layout(host);
    expect_true(call_count == 0, "an unchanged truthful plan emits no operations");

    reach_host_invalidate_surface_z_order(host, REACH_SURFACE_ID_STAGE);
    expect_true(host->dirty.z_order, "a banded surface press invalidates native order");
    expect_true(host->dirty.update_requested, "order invalidation schedules reconciliation");

    call_count = 0;
    reach_host_apply_layout(host);
    expect_true(count_calls(FAKE_WINDOW_CALL_SHOW) == 0, "order repair does not replay visibility");
    expect_true(count_calls(FAKE_WINDOW_CALL_HIDE) == 0, "order repair does not hide surfaces");
    expect_true(count_calls(FAKE_WINDOW_CALL_SET_TOPMOST) == 1,
                "order repair reseeds the topmost chain");
    expect_true(count_calls(FAKE_WINDOW_CALL_PLACE_BEHIND) == 2,
                "order repair restores every relative layer");
    expect_true(!host->dirty.z_order, "a completed repair clears the dirty state");
}

static void test_app_band_surface_does_not_invalidate_topmost_order(void)
{
    reach_host *host = &app_band_host;
    initialize_host(host);
    reach_host_apply_layout(host);

    reach_layout_set_condition(&host->layout_manager, REACH_LAYOUT_CONDITION_BARS_FORCED, 0);
    reach_host_apply_layout(host);
    host->dirty.update_requested = 0;
    host->dirty.z_order = 0;

    reach_host_invalidate_surface_z_order(host, REACH_SURFACE_ID_TOP_BAR);
    expect_true(!host->dirty.z_order, "an app-band surface leaves native order to Windows");
    expect_true(!host->dirty.update_requested, "an app-band press schedules no order repair");
}

static void test_window_manipulation_relevance_survives_unavailable_pointer(void)
{
    reach_host *host = &manipulation_host;
    reach_host_init_feature_registry(host);
    host->input_source.ops.get_window_manipulation = fake_get_window_manipulation;
    host->window_manipulation.manual = {501, 1};
    host->window_manipulation.manual_relevant = 1;
    host->window_manipulation.active_window = 501;
    host->window_manipulation.relevant = 1;

    observed_manipulation = {501, 1};
    reach_host_sync_window_manipulation(host);
    expect_true(host->window_manipulation.manual_relevant,
                "a failed pointer read retains the last known manipulation relevance");
    expect_true(host->window_manipulation.relevant &&
                    host->window_manipulation.active_window == 501,
                "an active manipulation retains its last known relevant state");

    observed_manipulation = {};
    reach_host_sync_window_manipulation(host);
    expect_true(!host->window_manipulation.manual_relevant && !host->window_manipulation.relevant &&
                    host->window_manipulation.active_window == 0,
                "manipulation end clears the retained relevance");
}

static void test_window_manipulation_tracks_pointer_monitor_membership(void)
{
    reach_host *host = &monitor_entry_host;
    reach_host_init_feature_registry(host);
    host->input_source.ops.get_pointer_position = fake_get_pointer_position;
    host->input_source.ops.get_window_manipulation = fake_get_window_manipulation;
    host->monitors.list = reinterpret_cast<reach_monitor_list *>(&primary_monitor);
    host->monitors.ops.count = fake_monitor_count;
    host->monitors.ops.primary = fake_primary_monitor;
    host->window_manipulation.manual = {502, 1};

    observed_manipulation = {502, 1};
    observed_pointer = {1500, 400};
    reach_host_sync_window_manipulation(host);
    expect_true(!host->window_manipulation.relevant,
                "an off-monitor drag remains irrelevant while its pointer stays away");

    observed_pointer = {0, 400};
    reach_host_sync_window_manipulation(host);
    expect_true(host->window_manipulation.manual_relevant && host->window_manipulation.relevant &&
                    host->window_manipulation.active_window == 502,
                "the primary monitor's left boundary makes the active drag relevant");

    observed_pointer = {-1, 400};
    reach_host_sync_window_manipulation(host);
    expect_true(!host->window_manipulation.relevant && host->window_manipulation.active_window == 0,
                "leaving the primary monitor releases manipulation suppression");

    observed_pointer = {500, 400};
    reach_host_sync_window_manipulation(host);
    expect_true(host->window_manipulation.relevant &&
                    host->window_manipulation.active_window == 502,
                "re-entering the primary monitor restores manipulation suppression");

    observed_manipulation = {};
    reach_host_sync_window_manipulation(host);
    expect_true(!host->window_manipulation.relevant,
                "ending a drag that entered the monitor releases suppression");
}

static void test_scaled_presentation_keeps_native_envelope_stationary(void)
{
    reach_rect_f32 translation = {0.0f, 0.0f, 0.0f, 10.0f};
    reach_host_surface_presentation_frame offset_frame =
        reach_host_surface_presentation_frame_compute({710.0f, 900.0f, 500.0f, 72.0f},
                                                      {710.0f, 900.0f, 500.0f, 300.0f}, {},
                                                      translation, 1.08f);
    reach_host_surface_presentation_frame settled_frame =
        reach_host_surface_presentation_frame_compute({710.0f, 900.0f, 500.0f, 72.0f},
                                                      {710.0f, 900.0f, 500.0f, 300.0f}, {},
                                                      translation, 1.08f);
    expect_true(
        reach_host_scalar_equal(offset_frame.window_bounds.y, settled_frame.window_bounds.y),
        "a scaled transition keeps its native window envelope stationary");
    expect_true(reach_host_scalar_equal(offset_frame.render_transform.offset_y,
                                        settled_frame.render_transform.offset_y),
                "the retained surface keeps a stable content origin during motion");
    expect_true(reach_host_scalar_equal(offset_frame.content_rect.y, settled_frame.content_rect.y),
                "the native input presentation keeps the same base content bounds");
}

static void test_presentation_extent_covers_spring_overshoot(void)
{
    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f};
    reach_motion_spec spec =
        reach_theme_motion_preset(reach_theme_default(), REACH_MOTION_PRESET_BOUNCY);
    bool covered = true, overshot = false;
    for (float dpi : dpis)
    {
        float origin = -40.0f * dpi;
        reach_render_group group = {};
        reach_motion_start_spec_at(&group.y, origin, 0.0f, -origin * 6.0f, spec, 10.0);
        reach_motion_start_spec_at(&group.scale, 0.9f, 1.0f, 0.6f, spec, 10.0);
        reach_rect_f32 translation = {0.0f, origin, 0.0f, -origin};
        float max_scale = 1.0f;
        reach_host_surface_presentation_extent(&group, &translation, &max_scale);
        double deadline =
            fmax(reach_motion_deadline(&group.y), reach_motion_deadline(&group.scale));
        for (double at = 10.0; at <= deadline; at += 0.0005)
        {
            float y = reach_motion_value_at(&group.y, at);
            float scale = reach_motion_value_at(&group.scale, at);
            overshot = overshot || y > 0.5f || scale > 1.001f;
            covered = covered && y >= translation.y - 0.001f &&
                      y <= translation.y + translation.height + 0.001f && scale <= max_scale;
        }
    }
    expect_true(overshot, "bouncy presentation springs overshoot their resting transform");
    expect_true(covered, "presentation envelopes cover every sampled overshoot extreme");

    reach_render_group monotone = {};
    reach_motion_start_at(&monotone.y, -40.0f, 0.0f, 0.2, REACH_EASING_EASE_OUT, 10.0);
    reach_motion_start_at(&monotone.scale, 0.96f, 1.0f, 0.2, REACH_EASING_EASE_OUT, 10.0);
    reach_rect_f32 translation = {0.0f, -40.0f, 0.0f, 40.0f};
    float max_scale = 1.08f;
    reach_host_surface_presentation_extent(&monotone, &translation, &max_scale);
    expect_true(translation.x == 0.0f && translation.y == -40.0f && translation.width == 0.0f &&
                    translation.height == 40.0f && max_scale == 1.08f,
                "monotone presentation envelopes stay exactly as declared");
}

static void test_window_state_commits_terminal_subpixel_position(void)
{
    reach_platform_window_port window = {};
    window.window = reinterpret_cast<reach_platform_window *>(1);
    window.ops.set_bounds = fake_set_bounds;
    reach_rect_f32 last_bounds = {};
    int32_t bounds_valid = 0;
    int32_t changed = 0;
    observed_set_bounds_count = 0;

    expect_true(reach_host_apply_window_state(&window, {100.25f, 39.90f, 280.0f, 200.0f}, {},
                                              &last_bounds, &bounds_valid, &changed) == REACH_OK &&
                    changed,
                "the near-final presentation position reaches the platform window");
    expect_true(reach_host_apply_window_state(&window, {100.25f, 40.25f, 280.0f, 200.0f}, {},
                                              &last_bounds, &bounds_valid, &changed) == REACH_OK &&
                    changed,
                "the terminal presentation position reaches the platform window");
    expect_true(observed_set_bounds_count == 2 && observed_bounds.y == 40.25f,
                "subpixel logical movement that crosses a native pixel is not suppressed");
    expect_true(reach_host_apply_window_state(&window, {100.25f, 40.25f, 280.0f, 200.0f}, {},
                                              &last_bounds, &bounds_valid, &changed) == REACH_OK &&
                    !changed && observed_set_bounds_count == 2,
                "an identical platform window request remains cached");
}

static void test_popup_pointer_coordinates_are_surface_local(void)
{
    reach_surface_runtime surface = {};
    surface.bounds_valid = 1;
    surface.last_bounds = {120.0f, 45.0f, 300.0f, 200.0f};

    reach_feature_definition definition = {};
    definition.surface.cls = REACH_SURFACE_CLASS_POPUP;
    reach_feature_runtime popup = {};
    popup.definition = &definition;
    popup.surface = &surface;

    reach_ui_event event = {};
    event.x = 155;
    event.y = 81;
    event.wheel_delta = 120;
    event.button = REACH_POINTER_BUTTON_PRIMARY;

    reach_pointer_event pointer =
        reach_host_surface_pointer_event(&popup, &event, REACH_POINTER_EVENT_DOWN);
    expect_true(pointer.coordinate_space == REACH_POINTER_COORDINATE_SURFACE_LOCAL,
                "popup pointer events declare surface-local coordinates");
    expect_true(pointer.x == 35 && pointer.y == 36,
                "popup pointer events are translated from screen to surface coordinates");

    definition.surface.cls = REACH_SURFACE_CLASS_PERSISTENT;
    pointer = reach_host_surface_pointer_event(&popup, &event, REACH_POINTER_EVENT_DOWN);
    expect_true(pointer.coordinate_space == REACH_POINTER_COORDINATE_SCREEN,
                "non-popup pointer events retain screen coordinates");
    expect_true(pointer.x == event.x && pointer.y == event.y,
                "non-popup pointer positions remain unchanged");
}

static void test_popup_activation_uses_owner_identity(void)
{
    reach_feature_layout_anchor top_bar_power = {REACH_SURFACE_ID_TOP_BAR,
                                                 REACH_TOP_BAR_CONTROL_POWER, 0};
    reach_feature_layout_anchor dock_item = {REACH_SURFACE_ID_DOCK, REACH_DOCK_CONTROL_ITEM, 3};

    expect_true(reach_host_popup_activation_decide(0, nullptr, &top_bar_power,
                                                   REACH_POPUP_ACTIVATION_TOGGLE) ==
                    REACH_POPUP_ACTIVATION_PRESENT,
                "a closed popup presents for its requested owner");
    expect_true(reach_host_popup_activation_decide(1, &top_bar_power, &top_bar_power,
                                                   REACH_POPUP_ACTIVATION_TOGGLE) ==
                    REACH_POPUP_ACTIVATION_CLOSE,
                "a repeated owner activation toggles its popup closed");
    expect_true(reach_host_popup_activation_decide(1, &top_bar_power, &dock_item,
                                                   REACH_POPUP_ACTIVATION_TOGGLE) ==
                    REACH_POPUP_ACTIVATION_PRESENT,
                "a different owner replaces the open popup");
    expect_true(reach_host_popup_activation_decide(1, &dock_item, &dock_item,
                                                   REACH_POPUP_ACTIVATION_PASSIVE) ==
                    REACH_POPUP_ACTIVATION_NONE,
                "repeated passive presentation for one owner is unchanged");
    expect_true(reach_host_popup_activation_decide(1, &dock_item, &dock_item,
                                                   REACH_POPUP_ACTIVATION_REPLACE) ==
                    REACH_POPUP_ACTIVATION_PRESENT,
                "an explicit content replacement preserves the owner");
}

static void test_power_popup_resolves_its_exact_top_bar_owner(void)
{
    reach_context_menu *menu = nullptr;
    expect_true(reach_context_menu_create(&menu) == REACH_OK,
                "a context menu is created for owner resolution");
    if (menu == nullptr)
    {
        return;
    }

    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.anchor_button = {1800.0f, 8.0f, 32.0f, 24.0f};
    ctx.bar_edge_y = 40.0f;
    ctx.drop_direction = REACH_POPUP_DROP_DOWN;
    ctx.anchored = 1;
    reach_context_menu_open_power(menu, &ctx);

    reach_host *host = &popup_owner_host;
    reach_host_init_feature_registry(host);
    host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU].capsule = menu;
    reach_feature_layout_anchor owner = {};
    expect_true(reach_host_resolve_popup_owner(
                    &host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU], &owner) &&
                    owner.surface == REACH_SURFACE_ID_TOP_BAR &&
                    owner.slot == REACH_TOP_BAR_CONTROL_POWER && owner.index == 0,
                "the Power popup owner exactly matches the Top Bar Power control token");

    reach_context_menu_destroy(menu);
}

static void test_window_list_region_crossing_uses_the_observed_point(void)
{
    reach_context_menu *menu = nullptr;
    expect_true(reach_context_menu_create(&menu) == REACH_OK,
                "a context menu is created for the region crossing");
    if (menu == nullptr)
    {
        return;
    }

    reach_menu_request request = {};
    request.window_count = 1;
    request.windows[0].window = 11;
    reach_context_menu_open_context ctx = {};
    ctx.theme = reach_theme_default();
    ctx.dpi_scale = 1.0f;
    ctx.anchored = 1;
    ctx.anchor_button = {380.0f, 1000.0f, 40.0f, 40.0f};
    ctx.bar_edge_y = 1000.0f;
    ctx.drop_direction = REACH_POPUP_DROP_UP;
    ctx.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    ctx.request = &request;
    reach_context_menu_open_window_list(menu, 0, &ctx);

    reach_rect_union shape = {};
    expect_true(reach_context_menu_window_list_hold_shape(menu, &shape) && shape.count > 0,
                "the open window list publishes a hold shape");
    reach_rect_f32 popup = shape.rects[0];
    reach_point_i32 inside = {(int32_t)(popup.x + popup.width * 0.5f),
                              (int32_t)(popup.y + popup.height * 0.5f)};
    reach_point_i32 above = {inside.x, (int32_t)(popup.y - 200.0f)};

    reach_host *host = &window_list_region_host;
    reach_host_init_feature_registry(host);
    host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU].capsule = menu;
    host->input_source.ops.get_pointer_position = fake_get_pointer_position;
    reach_host_bind_interfeature_routes(host);

    reach_ui_event crossing = {};
    crossing.type = REACH_UI_EVENT_POINTER_REGION_CHANGED;
    crossing.id = REACH_SURFACE_ID_CONTEXT_MENU;
    crossing.x = inside.x;
    crossing.y = inside.y;
    observed_pointer = above;
    (void)reach_host_handle_event(host, &crossing);
    expect_true(reach_context_menu_window_list_is_open(menu),
                "a crossing observed inside the hold keeps the list open");

    crossing.x = above.x;
    crossing.y = above.y;
    observed_pointer = inside;
    (void)reach_host_handle_event(host, &crossing);
    expect_true(!reach_context_menu_window_list_is_open(menu),
                "a crossing observed outside closes the list even when the cursor read lags");

    reach_host_clear_interfeature_routes(host);
    host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU].capsule = nullptr;
    reach_context_menu_destroy(menu);
}

static void test_captured_release_precedes_exclusive_popup(void)
{
    reach_host *host = &pointer_release_host;
    reach_host_init_feature_registry(host);
    host->has_layout = 1;

    reach_feature_capsule_ops capture_ops = {};
    capture_ops.handle_pointer = fake_captured_release;
    capture_ops.pointer_capture_active = fake_captured_release_active;
    host->feature_definitions[REACH_SURFACE_ID_DOCK].capsule_ops = &capture_ops;
    host->feature_runtimes[REACH_SURFACE_ID_DOCK].capsule = host;

    reach_feature_capsule_ops exclusive_ops = {};
    exclusive_ops.is_open = fake_exclusive_release_open;
    exclusive_ops.handle_pointer = fake_exclusive_release;
    host->feature_definitions[REACH_SURFACE_ID_CONTEXT_MENU].capsule_ops = &exclusive_ops;
    host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU].capsule = host;

    reach_host_surface_event_binding binding = {host,
                                                &host->feature_runtimes[REACH_SURFACE_ID_DOCK]};
    reach_ui_event event = {};
    event.type = REACH_UI_EVENT_POINTER_UP;
    event.button = REACH_POINTER_BUTTON_PRIMARY;

    captured_release_active = 1;
    exclusive_release_open = 1;
    captured_release_count = 0;
    exclusive_release_count = 0;
    reach_host_on_registered_surface_event(&binding, &event);
    expect_true(captured_release_count == 1 && exclusive_release_count == 0,
                "a captured sequence receives release before an open exclusive popup");

    captured_release_active = 0;
    captured_release_count = 0;
    exclusive_release_count = 0;
    reach_host_on_registered_surface_event(&binding, &event);
    expect_true(captured_release_count == 0 && exclusive_release_count == 1,
                "an exclusive popup receives release when no captured sequence exists");
}

static void test_popup_trigger_press_bypasses_unrelated_open_popup(void)
{
    reach_host *host = &popup_trigger_host;
    reach_host_init_feature_registry(host);
    host->has_layout = 1;

    reach_feature_capsule_ops source_ops = {};
    source_ops.handle_pointer = fake_popup_trigger_source;
    host->feature_definitions[REACH_SURFACE_ID_TOP_BAR].capsule_ops = &source_ops;
    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].capsule = host;

    reach_feature_capsule_ops popup_ops = {};
    popup_ops.is_open = fake_exclusive_release_open;
    popup_ops.handle_pointer = fake_unrelated_popup_down;
    host->feature_definitions[REACH_SURFACE_ID_QUICK_SETTINGS].capsule_ops = &popup_ops;
    host->feature_runtimes[REACH_SURFACE_ID_QUICK_SETTINGS].capsule = host;

    reach_host_surface_event_binding binding = {host,
                                                &host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR]};
    reach_ui_event event = {};
    event.type = REACH_UI_EVENT_POINTER_DOWN;
    event.button = REACH_POINTER_BUTTON_PRIMARY;

    exclusive_release_open = 1;
    fake_source_popup_trigger = 1;
    unrelated_popup_down_count = 0;
    reach_host_on_registered_surface_event(&binding, &event);
    expect_true(unrelated_popup_down_count == 0,
                "an open popup cannot cancel a press on a different popup trigger");

    fake_source_popup_trigger = 0;
    reach_host_on_registered_surface_event(&binding, &event);
    expect_true(unrelated_popup_down_count == 1,
                "an ordinary outside press is still offered to the open popup");
}

static void test_focus_restore_follows_the_close_intent(void)
{
    reach_host *host = &focus_restore_host;
    reach_host_init_feature_registry(host);

    expect_true(host->feature_runtimes[REACH_SURFACE_ID_LAUNCHER]
                    .definition->surface.restores_focus_on_close,
                "the Launcher declares that closing restores the previous foreground window");
    expect_true(!host->feature_runtimes[REACH_SURFACE_ID_SWITCHER]
                     .definition->surface.restores_focus_on_close,
                "the Switcher never needs focus restoration because it does not activate");
    expect_true(
        (host->feature_runtimes[REACH_SURFACE_ID_SWITCHER].definition->surface.behavior_flags &
         REACH_SURFACE_BEHAVIOR_ACTIVATES) == 0,
        "the Switcher remains non-activating while an app-switch gesture is active");

    host->focus_restore_window[REACH_SURFACE_ID_LAUNCHER] = 4242;
    reach_host_arm_focus_restore(host, REACH_SURFACE_ID_LAUNCHER);
    expect_true(host->focus_restore_pending[REACH_SURFACE_ID_LAUNCHER],
                "a dismissed surface arms its focus restore");

    reach_host_cancel_focus_restore(host, REACH_SURFACE_ID_LAUNCHER);
    expect_true(!host->focus_restore_pending[REACH_SURFACE_ID_LAUNCHER] &&
                    host->focus_restore_window[REACH_SURFACE_ID_LAUNCHER] == 0,
                "a superseded close drops the remembered window instead of stealing focus back");

    host->focus_restore_window[REACH_SURFACE_ID_LAUNCHER] = 4242;
    reach_host_arm_focus_restore(host, REACH_SURFACE_ID_LAUNCHER);
    reach_host_flush_focus_restore(host, REACH_SURFACE_ID_LAUNCHER);
    expect_true(!host->focus_restore_pending[REACH_SURFACE_ID_LAUNCHER] &&
                    host->focus_restore_window[REACH_SURFACE_ID_LAUNCHER] == 0,
                "flushing a restore consumes it so a later hide cannot replay it");

    host->focus_restore_window[REACH_SURFACE_ID_DOCK] = 4242;
    reach_host_arm_focus_restore(host, REACH_SURFACE_ID_DOCK);
    expect_true(!host->focus_restore_pending[REACH_SURFACE_ID_DOCK],
                "a surface that does not declare focus restore never arms one");
}

static void test_foreground_reconciles_after_window_discovery(void)
{
    reach_host *host = &foreground_reconciliation_host;
    reach_host_init_feature_registry(host);

    reach_window_manager_port window_manager = {};
    window_manager.ops.window_count = fake_foreground_window_count;
    window_manager.ops.window_at = fake_foreground_window_at;
    expect_true(reach_window_tracking_create(window_manager, &host->window_tracking) == REACH_OK,
                "foreground reconciliation creates window tracking");

    observed_foreground_window = 4242;
    foreground_window_available = 0;
    host->foreground_watcher.ops.foreground = fake_current_foreground;
    reach_host_apply_foreground_change(host);

    expect_true(reach_window_tracking_current_foreground(host->window_tracking) == 4242,
                "untracked native foreground is remembered");
    expect_true(reach_window_tracking_foreground(host->window_tracking) == 0,
                "untracked native foreground does not enter app history");

    foreground_window_available = 1;
    expect_true(reach_host_refresh_open_windows(host, nullptr) == REACH_OK,
                "newly discovered foreground window refreshes");
    reach_host_apply_foreground_change(host);

    expect_true(reach_window_tracking_current_foreground(host->window_tracking) == 4242,
                "native foreground remains unchanged after discovery");
    expect_true(reach_window_tracking_foreground(host->window_tracking) == 4242,
                "newly discovered foreground reconciles into app history");

    reach_window_tracking_destroy(host->window_tracking);
    host->window_tracking = nullptr;
}

static void test_every_dismissable_surface_reaches_the_shared_close_path(void)
{
    reach_host *host = &closable_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered feature factories create every capsule");

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        reach_surface_class cls = runtime->definition->surface.cls;
        if (cls != REACH_SURFACE_CLASS_TRANSIENT && cls != REACH_SURFACE_CLASS_POPUP &&
            cls != REACH_SURFACE_CLASS_OVERLAY)
        {
            continue;
        }
        expect_true(reach_host_surface_closable(runtime),
                    "every dismissable surface can be closed through the shared close path");
    }

    reach_feature_runtime *launcher = &host->feature_runtimes[REACH_SURFACE_ID_LAUNCHER];
    expect_true(launcher->definition->force_close == nullptr &&
                    reach_host_surface_closable(launcher),
                "a capsule-controlled surface is closable without a force_close callback");

    expect_true(launcher->definition->control_ops->set_open(launcher->capsule, 1, nullptr),
                "the launcher capsule opens through its control operation");
    reach_host_close_registered_surface(host, REACH_SURFACE_ID_LAUNCHER,
                                        REACH_SURFACE_CLOSE_SUPERSEDED);
    expect_true(!reach_host_surface_is_open(launcher),
                "the shared close path closes a surface that owns no force_close callback");

    reach_host_destroy_registered_features(host);
}

static void test_every_popup_names_the_control_that_holds_it_open(void)
{
    reach_host *host = &dismiss_guard_host;
    reach_host_init_feature_registry(host);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_definition *definition = host->feature_runtimes[index].definition;
        const reach_surface_spec *spec = &definition->surface;
        if (spec->cls != REACH_SURFACE_CLASS_POPUP && spec->cls != REACH_SURFACE_CLASS_TRANSIENT)
        {
            continue;
        }
        int32_t declares_guard = spec->dismiss_guard_surface < REACH_HOST_SURFACE_COUNT;
        int32_t anchored = definition->layout.anchor < REACH_HOST_SURFACE_COUNT ||
                           (definition->surface_ops != nullptr &&
                            definition->surface_ops->layout_anchor != nullptr);
        expect_true(declares_guard || anchored,
                    "every dismissable surface names the control a press must not dismiss it from");
    }

    const reach_surface_spec *launcher =
        &host->feature_runtimes[REACH_SURFACE_ID_LAUNCHER].definition->surface;
    expect_true(launcher->dismiss_guard_surface == REACH_SURFACE_ID_DOCK &&
                    launcher->dismiss_guard_any_control,
                "a press anywhere on the dock is the dock's to handle while the launcher is open");

    const reach_surface_spec *stage =
        &host->feature_runtimes[REACH_SURFACE_ID_STAGE].definition->surface;
    expect_true(stage->dismiss_guard_surface == REACH_SURFACE_ID_DOCK &&
                    stage->dismiss_guard_slot == REACH_DOCK_CONTROL_TRIGGER &&
                    !stage->dismiss_guard_any_control,
                "the stage is held open only by the dock trigger that opens it");

    const reach_feature_definition *battery =
        host->feature_runtimes[REACH_SURFACE_ID_BATTERY].definition;
    expect_true(battery->surface.dismiss_guard_surface >= REACH_HOST_SURFACE_COUNT &&
                    battery->layout.anchor == REACH_SURFACE_ID_TOP_BAR &&
                    battery->layout.anchor_slot == REACH_TOP_BAR_CONTROL_BATTERY,
                "a top-bar popup is held open by the control it already anchors to");

    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_DOCK].definition->capsule_ops->control_at_point !=
                nullptr &&
            host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR]
                    .definition->capsule_ops->control_at_point != nullptr,
        "both bars can name the control under a screen point");
    expect_true(host->feature_runtimes[REACH_SURFACE_ID_DOCK]
                            .definition->capsule_ops->pointer_capture_active != nullptr &&
                    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR]
                            .definition->capsule_ops->pointer_capture_active != nullptr,
                "both bars publish captured pressable sequences to generic release routing");
}

static void test_registered_feature_lifecycle(void)
{
    reach_host *host = &registry_host;
    reach_host_init_feature_registry(host);

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        const reach_feature_runtime *runtime = &host->feature_runtimes[index];
        const reach_feature_definition *definition = runtime->definition;
        expect_true(definition != nullptr, "every runtime references an immutable definition");
        expect_true(definition != nullptr && definition->id == index,
                    "every definition has the runtime's stable surface id");
        expect_true(definition != nullptr && definition->capsule_ops != nullptr,
                    "every definition owns the capsule operation contract");
        expect_true(definition != nullptr && definition->surface_ops != nullptr,
                    "every registered surface uses the generic frame contract");
        expect_true(definition != nullptr && runtime->definition == definition,
                    "every runtime references its sole immutable definition");
    }

    const reach_feature_definition *launcher =
        host->feature_runtimes[REACH_SURFACE_ID_LAUNCHER].definition;
    expect_true(launcher->capsule_ops->surface_geometry != nullptr,
                "Launcher owns its presentation while composition applies its transform");
    expect_true(host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU]
                        .definition->surface_ops->layout_anchor != nullptr,
                "Context Menu declares runtime-selected layout anchoring");
    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_STAGE].definition->surface_ops->native_overlay !=
            nullptr,
        "Stage declares its native overlay contract");
    const reach_surface_id presentation_owned_surfaces[] = {
        REACH_SURFACE_ID_LAUNCHER,       REACH_SURFACE_ID_CLIPBOARD, REACH_SURFACE_ID_TRAY,
        REACH_SURFACE_ID_QUICK_SETTINGS, REACH_SURFACE_ID_BATTERY,   REACH_SURFACE_ID_CONTEXT_MENU,
        REACH_SURFACE_ID_SWITCHER,       REACH_SURFACE_ID_STAGE};
    for (size_t index = 0;
         index < sizeof(presentation_owned_surfaces) / sizeof(presentation_owned_surfaces[0]);
         ++index)
    {
        expect_true(host->feature_runtimes[presentation_owned_surfaces[index]]
                            .definition->capsule_ops->presentation_visible != nullptr,
                    "every capsule-owned presentation declares when its native surface is visible");
    }
    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_DOCK].definition->resolve_anchor != nullptr &&
            host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].definition->resolve_anchor != nullptr,
        "dynamic anchor owners publish generic anchor resolvers");
    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_DOCK].definition->layout.reservation_edge ==
                REACH_LAYOUT_RESERVATION_BOTTOM &&
            host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].definition->layout.reservation_edge ==
                REACH_LAYOUT_RESERVATION_TOP &&
            host->feature_runtimes[REACH_SURFACE_ID_STAGE].definition->layout.uses_reserved_bounds,
        "Stage consumes the generic bounds reserved by the bars");
    expect_true(
        (host->feature_runtimes[REACH_SURFACE_ID_SYSTEM_HUD].definition->surface.behavior_flags &
         REACH_SURFACE_BEHAVIOR_INPUT_PASSTHROUGH) != 0,
        "System HUD declares native input passthrough");

    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered feature factories create every capsule");
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        expect_true(host->feature_runtimes[index].capsule != nullptr,
                    "every registered surface receives its capsule");
    }
    expect_true(host->feature_runtimes[REACH_SURFACE_ID_TRAY].capsule ==
                    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].capsule,
                "the tray surface reuses its registered top-bar owner");

    reach_host_destroy_registered_features(host);
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        expect_true(host->feature_runtimes[index].capsule == nullptr,
                    "destroying registered features clears every surface capsule");
    }
}

static void test_registered_surface_input_passthrough(void)
{
    reach_host *host = &input_passthrough_host;
    reach_host_init_feature_registry(host);

    reach_host_dependencies dependencies = {};
    dependencies.launcher_window.window = reinterpret_cast<reach_platform_window *>(1);
    dependencies.launcher_window.ops.set_input_passthrough = fake_set_input_passthrough;
    dependencies.system_hud_window.window = reinterpret_cast<reach_platform_window *>(2);
    dependencies.system_hud_window.ops.set_input_passthrough = fake_set_input_passthrough;

    call_count = 0;
    reach_host_bind_registered_surface_ports(host, &dependencies);

    expect_true(has_call(FAKE_WINDOW_CALL_SET_INPUT_PASSTHROUGH, 1, 0),
                "ordinary surfaces keep native input enabled");
    expect_true(has_call(FAKE_WINDOW_CALL_SET_INPUT_PASSTHROUGH, 2, 1),
                "System HUD enables native input passthrough");
}

static void test_frame_scheduling_does_not_present_a_closed_surface(void)
{
    fake_presentation_capsule capsule = {};
    capsule.needs_frame = 1;

    reach_feature_capsule_ops capsule_ops = {};
    capsule_ops.is_open = fake_presentation_is_open;
    capsule_ops.needs_frame = fake_presentation_needs_frame;
    capsule_ops.presentation_visible = fake_presentation_visible;

    reach_feature_definition definition = {};
    definition.capsule_ops = &capsule_ops;

    reach_feature_runtime runtime = {};
    runtime.definition = &definition;
    runtime.capsule = &capsule;

    expect_true(reach_host_surface_needs_frame(&runtime),
                "background work can keep scheduling a closed surface");
    expect_true(!reach_host_surface_presented(&runtime),
                "background frame scheduling does not present a closed native surface");

    capsule.presentation_visible = 1;
    expect_true(reach_host_surface_presented(&runtime),
                "capsule-owned close presentation remains visible when declared explicitly");
}

static void test_popup_surface_retains_presentation(void)
{
    reach_host *host = &popup_render_host;
    host->layout_dpi_scale = 1.0f;

    reach_surface_runtime surface = {};
    surface.renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    surface.renderer.ops.begin_frame = fake_begin_frame;
    surface.renderer.ops.end_frame = fake_end_frame;
    surface.renderer.ops.execute = fake_execute;

    auto content_storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer &content = *content_storage;
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.color = {0.1f, 0.2f, 0.3f, 0.8f};
    content.has_presentation = 1;
    reach_motion_set(&content.presentation.opacity, 0.25f);
    expect_true(reach_render_command_buffer_push(&content, &command) == REACH_OK,
                "popup presentation test records its content command");

    observed_last_command_alpha = -1.0f;
    expect_true(reach_host_render_popup_surface(host, REACH_SURFACE_ID_QUICK_SETTINGS, &surface,
                                                {0.0f, 0.0f, 320.0f, 240.0f}, 160.0f, 0,
                                                &content) == REACH_OK,
                "popup rendering succeeds with retained presentation");
    expect_true(reach_host_scalar_equal(observed_last_command_alpha, 0.8f) &&
                    reach_host_scalar_equal(observed_presentation_opacity, 0.25f),
                "popup opacity belongs to retained presentation instead of raster content");
}

static void test_registered_surface_frame_uses_declared_anchor(void)
{
    reach_host *host = &generic_frame_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered features are available to the generic frame");
    reach_host_init_layout(host);
    host->layout_dpi_scale = 1.0f;

    reach_feature_runtime *dock = &host->feature_runtimes[REACH_SURFACE_ID_DOCK];
    dock->resolved_bounds = {710.0f, 1000.0f, 500.0f, 64.0f};
    dock->resolved_bounds_valid = 1;

    reach_brightness_state brightness = {};
    brightness.available = 1;
    brightness.level = 0.75f;
    reach_system_hud_show_brightness(
        reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD),
        &brightness);

    reach_feature_runtime *hud = &host->feature_runtimes[REACH_SURFACE_ID_SYSTEM_HUD];
    hud->surface->window.window = reinterpret_cast<reach_platform_window *>(1);
    hud->surface->window.ops.set_bounds = fake_set_bounds;
    hud->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    hud->surface->renderer.ops.begin_frame = fake_begin_frame;
    hud->surface->renderer.ops.end_frame = fake_end_frame;
    hud->surface->renderer.ops.execute = fake_execute;
    hud->surface->dirty_flags = 1;

    observed_bounds = {};
    render_count = 0;
    reach_host_frame_context frame = {};
    frame.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    expect_true(reach_host_frame_registered_surface(host, hud, &frame) == REACH_OK,
                "the generic registered surface frame succeeds");

    const reach_system_hud_state *state = reach_system_hud_state_ptr(
        reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD));
    expect_true(
        reach_host_scalar_equal(state->layout.bounds.y + state->layout.bounds.height, 988.0f),
        "the generic frame arranges from the declared Dock anchor");
    expect_true(hud->resolved_bounds_valid &&
                    reach_rect_equal(hud->resolved_bounds, state->layout.bounds),
                "the generic frame publishes the resolved HUD bounds");
    expect_true(observed_bounds.width >= state->layout.bounds.width,
                "the generic frame applies native bounds with surface padding");
    expect_true(render_count == 1, "the generic frame executes the feature command buffer");

    reach_host_destroy_registered_features(host);
}

static void test_registered_surface_frame_syncs_native_overlay(void)
{
    reach_host *host = &native_overlay_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered native-overlay feature is available");
    reach_host_init_layout(host);
    host->layout_dpi_scale = 1.0f;

    static const uint16_t label[] = {'W', 'i', 'n', 'd', 'o', 'w', 0};
    reach_stage_open_window windows[2] = {};
    windows[0].window = 42;
    windows[0].label = label;
    windows[0].frame = {100.0f, 100.0f, 800.0f, 600.0f};
    windows[1].window = 84;
    windows[1].desktop = 1;
    windows[1].frame = {0.0f, 0.0f, 1920.0f, 1080.0f};
    reach_rect_f32 monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    expect_true(
        reach_stage_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE),
                         monitor, 1.0f, windows, 2) == REACH_OK,
        "Stage opens for native-overlay frame testing");

    reach_feature_runtime *stage = &host->feature_runtimes[REACH_SURFACE_ID_STAGE];
    stage->surface->window.window = reinterpret_cast<reach_platform_window *>(9);
    stage->surface->window.ops.set_bounds = fake_set_bounds;
    stage->surface->window.ops.native_id = fake_window_id;
    stage->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    stage->surface->renderer.ops.begin_frame = fake_begin_frame;
    stage->surface->renderer.ops.end_frame = fake_end_frame;
    stage->surface->renderer.ops.execute = fake_execute;
    stage->surface->dirty_flags = 1;
    host->window_thumbnails.thumbnails = reinterpret_cast<reach_window_thumbnails *>(1);
    host->window_thumbnails.ops.create = fake_thumbnail_create;
    host->window_thumbnails.ops.set_placement = fake_thumbnail_set_placement;
    host->window_thumbnails.ops.bring_to_front = fake_thumbnail_bring_to_front;
    host->window_thumbnails.ops.release = fake_thumbnail_release;

    thumbnail_create_count = 0;
    thumbnail_target_plane_create_count = 0;
    thumbnail_behind_plane_create_count = 0;
    thumbnail_place_count = 0;
    thumbnail_background_place_count = 0;
    thumbnail_background_alpha = 0.0f;
    thumbnail_release_count = 0;
    thumbnail_bring_to_front_count = 0;
    thumbnail_front_id = REACH_WINDOW_THUMBNAIL_NONE;
    observed_bounds = {};
    reach_host_frame_context frame = {};
    frame.monitor_bounds = monitor;
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "generic frame renders a native-overlay surface");
    expect_true(reach_rect_equal(observed_bounds, monitor),
                "Stage animation keeps the fullscreen surface on the monitor bounds");
    expect_true(thumbnail_create_count == 2 && thumbnail_place_count == 2,
                "generic frame registers and places the Stage thumbnail");
    expect_true(thumbnail_target_plane_create_count == 1 &&
                    thumbnail_behind_plane_create_count == 1,
                "generic frame separates app and Desktop thumbnail planes");
    expect_true(thumbnail_background_place_count == 1 && thumbnail_background_alpha == 1.0f,
                "Desktop lower plane carries one fully opaque Stage background");

    reach_window_thumbnail_id app_thumbnail = stage->native_overlay_ids[0];
    size_t creates_before_front = thumbnail_create_count;
    size_t releases_before_front = thumbnail_release_count;
    reach_host_set_native_overlay_front_source(host, stage, windows[0].window);
    expect_true(thumbnail_bring_to_front_count == 1 && thumbnail_front_id == app_thumbnail,
                "the requested app relationship becomes the visual front owner");
    expect_true(thumbnail_create_count == creates_before_front &&
                    thumbnail_release_count == releases_before_front,
                "front ownership preserves every native relationship");
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "the promoted native overlay keeps presenting");
    expect_true(thumbnail_bring_to_front_count == 1,
                "the promoted relationship is not recreated on every frame");

    reach_stage_open_window joined[3] = {windows[0], {}, windows[1]};
    joined[1].window = 126;
    joined[1].label = label;
    joined[1].frame = {300.0f, 200.0f, 900.0f, 700.0f};
    thumbnail_unavailable_source = 126;
    expect_true(
        reach_stage_update_windows(
            reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE), joined, 3),
        "a new source starts a Stage reflow");
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "an unavailable new thumbnail does not abort the Stage frame");
    expect_true(reach_host_surface_presented(stage),
                "Stage remains presented while the new source becomes available");
    expect_true(thumbnail_create_count == 2 && thumbnail_behind_plane_create_count == 1 &&
                    thumbnail_release_count == 0,
                "joining a source preserves every existing native relationship");
    thumbnail_unavailable_source = 0;
    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(0.016);
    stage->definition->capsule_ops->tick(stage->capsule, 0.016, &tick);
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "the unavailable relationship is retried on the next frame");
    expect_true(thumbnail_create_count == 3 && thumbnail_behind_plane_create_count == 1,
                "the retry adds only the new app relationship");

    thumbnail_placement_result = REACH_ERROR;
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "a transient native placement failure does not abort an animated Stage frame");
    thumbnail_placement_result = REACH_OK;

    reach_stage_open_window remaining[2] = {joined[1], joined[2]};
    expect_true(
        reach_stage_update_windows(
            reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE), remaining, 2),
        "a closed source starts a Stage reflow");
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "the departing source keeps its relationship through the transition");
    reach_test_motion_clock_advance(1.0);
    stage->definition->capsule_ops->tick(stage->capsule, 1.0, &tick);
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "Stage presents the settled layout after the source departs");
    expect_true(reach_stage_thumbnail_count(
                    reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE)) == 2,
                "the closed source is removed after its reflow");
    expect_true(thumbnail_create_count == 3 && thumbnail_behind_plane_create_count == 1 &&
                    thumbnail_release_count == 1,
                "settling the reflow releases only the departed app relationship");

    reach_window_manager_port preview_port = {};
    preview_port.ops.window_count = fake_foreground_window_count;
    preview_port.ops.window_at = fake_foreground_window_at;
    preview_port.ops.frame_bounds = fake_preview_frame_bounds;
    reach_window_tracking *preview_windows = nullptr;
    expect_true(reach_window_tracking_create(preview_port, &preview_windows) == REACH_OK,
                "second preview consumer has window tracking");
    observed_foreground_window = 126;
    foreground_window_available = 1;
    (void)reach_window_tracking_refresh(preview_windows, nullptr);
    reach_feature_runtime *switcher = &host->feature_runtimes[REACH_SURFACE_ID_SWITCHER];
    reach_switcher_attach_services(static_cast<reach_switcher *>(switcher->capsule), nullptr,
                                   preview_windows);
    switcher->surface->window.window = reinterpret_cast<reach_platform_window *>(10);
    switcher->surface->window.ops.set_bounds = fake_set_bounds;
    switcher->surface->window.ops.native_id = fake_window_id;
    switcher->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    switcher->surface->renderer.ops.begin_frame = fake_begin_frame;
    switcher->surface->renderer.ops.end_frame = fake_end_frame;
    switcher->surface->renderer.ops.execute = fake_execute;
    reach_ui_event begin = {};
    begin.type = REACH_UI_EVENT_APP_SWITCH_BEGIN;
    reach_capsule_event_result begin_result = {};
    switcher->definition->capsule_ops->handle_event(switcher->capsule, &begin, &begin_result);
    expect_true(reach_host_frame_registered_surface(host, switcher, &frame) == REACH_OK,
                "another surface registers its own preview destination");
    reach_window_thumbnail_id switcher_id = switcher->native_overlay_ids[0];
    expect_true(switcher_id != 0 && thumbnail_targets[switcher_id] == 10,
                "preview creation carries its owning destination");
    reach_window_thumbnail_id stage_id = stage->native_overlay_ids[0];
    expect_true(!thumbnail_released[stage_id] && thumbnail_targets[stage_id] == 9,
                "second destination preserves Stage relationships");
    switcher->surface->window.window = reinterpret_cast<reach_platform_window *>(11);
    expect_true(reach_host_frame_registered_surface(host, switcher, &frame) == REACH_OK,
                "a destination replacement reconciles without a generation change");
    expect_true(thumbnail_released[switcher_id] && !thumbnail_released[stage_id],
                "destination replacement releases only its own relationships");
    switcher_id = switcher->native_overlay_ids[0];
    expect_true(thumbnail_targets[switcher_id] == 11,
                "replacement relationship belongs to the new destination");

    size_t releases_before_close = thumbnail_release_count;
    reach_stage_force_close(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE));
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "generic frame handles native-overlay closure");
    expect_true(thumbnail_release_count == releases_before_close + 2,
                "generic frame releases native overlays when the capsule closes");
    expect_true(stage->native_overlay_front_source == 0 &&
                    stage->native_overlay_front_id == REACH_WINDOW_THUMBNAIL_NONE,
                "native overlay closure clears visual front ownership");

    expect_true(!thumbnail_released[switcher_id] &&
                    reach_host_frame_registered_surface(host, switcher, &frame) == REACH_OK,
                "closing Stage leaves the other preview consumer valid");
    expect_true(switcher->native_overlay_ids[0] == switcher_id,
                "the surviving consumer retains its stable relationship");
    reach_host_release_native_overlay(host, switcher);
    expect_true(thumbnail_released[switcher_id], "second consumer releases its own relationship");
    reach_host_destroy_registered_features(host);
    reach_window_tracking_destroy(preview_windows);
    foreground_window_available = 0;
}

static void test_forced_bars_hold_through_a_closing_stage(void)
{
    reach_host *host = &closing_stage_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered Stage feature is available");
    reach_host_init_layout(host);
    host->layout_dpi_scale = 1.0f;

    reach_feature_runtime *stage = &host->feature_runtimes[REACH_SURFACE_ID_STAGE];
    expect_true(stage->definition->surface.bar_shown_while_open,
                "Stage is the surface that forces the bars shown");

    static const uint16_t label[] = {'W', 'i', 'n', 'd', 'o', 'w', 0};
    reach_stage_open_window window = {};
    window.window = 42;
    window.label = label;
    window.frame = {100.0f, 100.0f, 800.0f, 600.0f};
    reach_rect_f32 monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    reach_stage *capsule = reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE);
    expect_true(reach_stage_open(capsule, monitor, 1.0f, &window, 1) == REACH_OK,
                "Stage opens for close-animation testing");
    expect_true(reach_host_surface_presented(stage), "an open Stage is presented");

    reach_stage_begin_close(capsule);
    expect_true(!stage->definition->capsule_ops->is_open(stage->capsule),
                "the Stage capsule reports closed as soon as it begins closing");
    expect_true(reach_host_surface_presented(stage),
                "a Stage animating closed is still presented, so the bars stay forced shown");

    reach_feature_tick_result tick = {};
    reach_test_motion_clock_advance(5.0);
    stage->definition->capsule_ops->tick(stage->capsule, 5.0, &tick);
    expect_true(reach_host_surface_presented(stage),
                "bars remain forced through the aligned frame");
    stage->definition->capsule_ops->presentation_committed(stage->capsule, REACH_OK, &tick);
    reach_test_motion_clock_advance(5.0);
    stage->definition->capsule_ops->tick(stage->capsule, 5.0, &tick);
    expect_true(reach_host_surface_presented(stage),
                "bars remain forced through the transparent frame");
    stage->definition->capsule_ops->presentation_committed(stage->capsule, REACH_OK, &tick);
    reach_test_motion_clock_advance(0.016);
    stage->definition->capsule_ops->tick(stage->capsule, 0.016, &tick);
    expect_true(!reach_host_surface_presented(stage),
                "a fully closed Stage stops forcing the bars shown");

    reach_host_destroy_registered_features(host);
}

static reach_screen_hotspot_callback bar_hotspot_callback;
static void *bar_hotspot_user;
static int bar_hotspot_tokens[REACH_HOST_SURFACE_COUNT];
static size_t bar_hotspot_created;

static reach_result fake_hotspot_noop(reach_screen_hotspot *hotspot)
{
    (void)hotspot;
    return REACH_OK;
}

static reach_result fake_hotspot_set_bounds(reach_screen_hotspot *hotspot, reach_rect_f32 bounds)
{
    (void)hotspot;
    (void)bounds;
    return REACH_OK;
}

static reach_result fake_hotspot_set_callback(reach_screen_hotspot *hotspot,
                                              reach_screen_hotspot_callback callback, void *user)
{
    reach_host_edge_reveal_runtime *runtime = static_cast<reach_host_edge_reveal_runtime *>(user);
    if (runtime->owner->definition->id == REACH_SURFACE_ID_TOP_BAR)
    {
        bar_hotspot_callback = callback;
        bar_hotspot_user = user;
    }
    (void)hotspot;
    return REACH_OK;
}

static reach_result fake_hotspot_create(void *factory, reach_screen_hotspot_port *out_port)
{
    (void)factory;
    *out_port = {};
    out_port->hotspot =
        reinterpret_cast<reach_screen_hotspot *>(&bar_hotspot_tokens[bar_hotspot_created++]);
    out_port->ops.set_bounds = fake_hotspot_set_bounds;
    out_port->ops.show = fake_hotspot_noop;
    out_port->ops.hide = fake_hotspot_noop;
    out_port->ops.set_callback = fake_hotspot_set_callback;
    return REACH_OK;
}

static reach_result fake_trespassing_outer_bounds(const reach_window_manager *manager,
                                                  reach_window_id window, reach_rect_f32 *out)
{
    (void)manager;
    (void)window;
    *out = {100.0f, 0.0f, 800.0f, 600.0f};
    return REACH_OK;
}

typedef struct bar_enter_fixture
{
    reach_host *host;
    reach_window_tracking *windows;
    reach_app_control *apps;
    reach_top_bar *top_bar;
} bar_enter_fixture;

static void bar_enter_reconcile(bar_enter_fixture *fixture)
{
    reach_feature_surface_geometry geometry = {};
    (void)reach_host_reconcile_bar_visibility(fixture->host, REACH_SURFACE_ID_TOP_BAR,
                                              {100.0f, 0.0f, 800.0f, 40.0f},
                                              {0.0f, 0.0f, 1000.0f, 800.0f}, &geometry);
}

static int32_t bar_enter_hidden(const bar_enter_fixture *fixture)
{
    return reach_top_bar_state_ptr(fixture->top_bar)->visibility.target_hidden;
}

static void bar_enter_setup(bar_enter_fixture *fixture)
{
    reach_host *host = fixture->host;
    bar_hotspot_created = 0;
    bar_hotspot_callback = nullptr;
    bar_hotspot_user = nullptr;
    reach_host_init_feature_registry(host);
    host->screen_hotspots.ops.create = fake_hotspot_create;
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered top bar is available for the dropped-enter scenario");
    expect_true(reach_host_create_edge_reveals(host) == REACH_OK,
                "edge reveal hotspots are created for the dropped-enter scenario");
    reach_host_init_layout(host);
    expect_true(reach_host_start_edge_reveals(host) == REACH_OK,
                "edge reveal callbacks are installed for the dropped-enter scenario");
    host->layout_dpi_scale = 1.0f;
    host->input_source.ops.get_pointer_position = fake_get_pointer_position;

    reach_window_manager_port port = {};
    port.ops.window_count = fake_foreground_window_count;
    port.ops.window_at = fake_foreground_window_at;
    port.ops.outer_bounds = fake_trespassing_outer_bounds;
    observed_foreground_window = 126;
    foreground_window_available = 1;
    expect_true(reach_window_tracking_create(port, &fixture->windows) == REACH_OK,
                "window tracking is available for the dropped-enter scenario");
    (void)reach_window_tracking_refresh(fixture->windows, nullptr);
    expect_true(reach_app_control_create({}, {}, {}, port, nullptr, nullptr, &fixture->apps) ==
                    REACH_OK,
                "app control is available for the dropped-enter scenario");

    fixture->top_bar = reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR);
    reach_top_bar_attach_services(fixture->top_bar, nullptr, nullptr, fixture->windows, nullptr,
                                  nullptr, nullptr);
    reach_top_bar_attach_app_control(fixture->top_bar, fixture->apps);
    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].bar_auto_hide_active = 1;

    observed_pointer = {500, 400};
    bar_enter_reconcile(fixture);
    expect_true(bar_enter_hidden(fixture), "the bar settles hidden while the pointer is away");
    expect_true(bar_hotspot_callback != nullptr, "the top bar hotspot callback was captured");
}

static void bar_enter_teardown(bar_enter_fixture *fixture)
{
    reach_host_destroy_edge_reveals(fixture->host);
    reach_host_destroy_registered_features(fixture->host);
    reach_app_control_destroy(fixture->apps);
    reach_window_tracking_destroy(fixture->windows);
    foreground_window_available = 0;
}

static void bar_enter_fire(reach_screen_hotspot_event event)
{
    bar_hotspot_callback(bar_hotspot_user, event);
}

static void test_dropped_bar_enter_reveals_after_manipulation_clears(void)
{
    bar_enter_fixture fixture = {&bar_enter_host};
    bar_enter_setup(&fixture);
    reach_host *host = fixture.host;

    observed_pointer = {500, 0};
    host->window_manipulation.relevant = 1;
    bar_enter_fire(REACH_SCREEN_HOTSPOT_ENTER);
    host->window_manipulation.relevant = 0;
    bar_enter_reconcile(&fixture);
    expect_true(!bar_enter_hidden(&fixture),
                "an enter dropped during manipulation reveals the bar once it clears");

    bar_enter_teardown(&fixture);
}

static void test_dropped_bar_enter_is_discarded_by_leave_or_committed_hide(void)
{
    bar_enter_fixture fixture = {&bar_enter_host};
    bar_enter_setup(&fixture);
    reach_host *host = fixture.host;

    observed_pointer = {500, 0};
    host->window_manipulation.relevant = 1;
    bar_enter_fire(REACH_SCREEN_HOTSPOT_ENTER);
    bar_enter_fire(REACH_SCREEN_HOTSPOT_LEAVE);
    host->window_manipulation.relevant = 0;
    bar_enter_reconcile(&fixture);
    expect_true(bar_enter_hidden(&fixture), "a leave discards a dropped enter");

    host->window_manipulation.relevant = 1;
    bar_enter_fire(REACH_SCREEN_HOTSPOT_ENTER);
    bar_enter_reconcile(&fixture);
    host->window_manipulation.relevant = 0;
    bar_enter_reconcile(&fixture);
    expect_true(bar_enter_hidden(&fixture), "a committed hide discards a dropped enter");

    bar_enter_teardown(&fixture);
}

static void bar_enter_frame_surface(bar_enter_fixture *fixture)
{
    reach_host_frame_context frame = {};
    frame.monitor_bounds = {0.0f, 0.0f, 1000.0f, 800.0f};
    expect_true(reach_host_frame_registered_surface(
                    fixture->host, &fixture->host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR],
                    &frame) == REACH_OK,
                "the top bar frames");
}

static void bar_enter_settle(bar_enter_fixture *fixture)
{
    for (int32_t step = 0; step < 8; ++step)
    {
        reach_test_motion_clock_advance(0.5);
        bar_enter_frame_surface(fixture);
    }
    fixture->host->dirty = {};
    fixture->host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].surface->dirty_flags = 0;
}

static void test_bar_visibility_request_reconciles_idle_top_bar(void)
{
    bar_enter_fixture fixture = {&bar_enter_host};
    bar_enter_setup(&fixture);
    reach_host *host = fixture.host;
    reach_feature_runtime *top_bar = &host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR];
    top_bar->surface->window.window = reinterpret_cast<reach_platform_window *>(9);
    top_bar->surface->window.ops.set_bounds = fake_set_bounds;
    top_bar->surface->window.ops.native_id = fake_window_id;
    top_bar->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    top_bar->surface->renderer.ops.begin_frame = fake_begin_frame;
    top_bar->surface->renderer.ops.end_frame = fake_end_frame;
    top_bar->surface->renderer.ops.execute = fake_execute;
    host->has_layout = 1;

    observed_pointer = {500, 400};
    top_bar->surface->dirty_flags = 1;
    bar_enter_settle(&fixture);
    expect_true(top_bar->bar_auto_hide_active, "the dynamic top bar keeps auto hide active");
    expect_true(bar_enter_hidden(&fixture), "the bar settles hidden while the pointer is away");

    observed_pointer = {500, 0};
    bar_enter_fire(REACH_SCREEN_HOTSPOT_ENTER);
    expect_true(top_bar->surface->dirty_flags == 0, "the idle top bar has no pending redraw");
    bar_enter_frame_surface(&fixture);
    expect_true(!bar_enter_hidden(&fixture), "an enter reveals the idle top bar on the next frame");

    bar_enter_reconcile(&fixture);
    bar_enter_settle(&fixture);
    expect_true(!bar_enter_hidden(&fixture), "the bar rests revealed while the pointer is on it");
    observed_pointer = {500, 400};
    reach_host_request_bar_visibility_update(host);
    bar_enter_frame_surface(&fixture);
    expect_true(bar_enter_hidden(&fixture), "a leave hides the idle top bar on the next frame");

    bar_enter_teardown(&fixture);
}

static int32_t bar_condition_active(const reach_host *host, reach_layout_condition condition)
{
    return (host->layout_manager.active_conditions & (1u << (uint32_t)condition)) != 0;
}

static void test_bar_conditions_sync_once_from_host_state(void)
{
    reach_host *host = &bar_conditions_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered popup and Stage features are available");
    reach_host_init_layout(host);

    reach_host_sync_bar_layout_conditions(host);
    expect_true(!bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_FORCED) &&
                    !bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_HELD),
                "closed surfaces leave both global bar conditions clear");

    static const uint16_t label[] = {'W', 'i', 'n', 'd', 'o', 'w', 0};
    reach_stage_open_window window = {};
    window.window = 42;
    window.label = label;
    window.frame = {100.0f, 100.0f, 800.0f, 600.0f};
    reach_stage *stage = reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE);
    expect_true(reach_stage_open(stage, {0.0f, 0.0f, 1920.0f, 1080.0f}, 1.0f, &window, 1) ==
                    REACH_OK,
                "Stage opens for the forced-bar condition");
    host->dirty = {};
    reach_host_sync_bar_layout_conditions(host);
    expect_true(host->dirty.bar_visibility, "Stage becoming presented requests a bar update");
    host->dirty = {};
    reach_host_sync_bar_layout_conditions(host);
    expect_true(!host->dirty.bar_visibility, "an unchanged condition requests no bar update");
    expect_true(bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_FORCED),
                "the host-level sync publishes the forced-bar condition");

    reach_battery_open_context battery_context = {};
    battery_context.theme = reach_theme_default();
    battery_context.monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    battery_context.dpi_scale = 1.0f;
    reach_battery_open(reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY),
                       &battery_context);
    reach_host_sync_bar_layout_conditions(host);
    expect_true(bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_HELD),
                "the host-level sync publishes the popup-held condition");

    reach_stage_force_close(stage);
    reach_battery_force_close(
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY));
    host->dirty = {};
    reach_host_sync_bar_layout_conditions(host);
    expect_true(host->dirty.bar_visibility, "Stage no longer presented requests a bar update");
    expect_true(!bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_FORCED) &&
                    !bar_condition_active(host, REACH_LAYOUT_CONDITION_BARS_HELD),
                "one host-level sync clears both global conditions after closure");

    reach_host_destroy_registered_features(host);
}

static void test_switcher_publishes_arranged_surface_geometry(void)
{
    reach_switcher *switcher = nullptr;
    expect_true(reach_switcher_create(&switcher) == REACH_OK, "switcher is created");

    reach_switcher_arrange_context arrange = {};
    arrange.theme = reach_theme_default();
    arrange.monitor_bounds = {100.0f, 50.0f, 1200.0f, 800.0f};
    arrange.dpi_scale = 1.0f;
    arrange.transition_visible = 1;
    expect_true(reach_switcher_arrange(switcher, &arrange),
                "initial switcher arrangement publishes a layout change");

    reach_feature_surface_geometry geometry = {};
    reach_switcher_capsule_ops()->surface_geometry(switcher, &geometry);
    expect_true(geometry.visible_bounds.width > 0.0f && geometry.visible_bounds.height > 0.0f,
                "switcher publishes non-empty surface geometry");
    expect_true(reach_host_scalar_equal(
                    geometry.visible_bounds.x + geometry.visible_bounds.width * 0.5f, 700.0f),
                "switcher surface geometry is centered on the monitor");

    reach_switcher_destroy(switcher);
}

static void test_native_overlay_placement_follows_surface_presentation(void)
{
    reach_feature_surface_geometry geometry = {};
    geometry.visible_bounds = {100.0f, 200.0f, 400.0f, 200.0f};
    reach_feature_surface_context ctx = {};
    reach_shadow_pad pad = {};
    pad.left = 12.0f;
    pad.top = 18.0f;
    reach_window_thumbnail_placement placement = {};
    placement.destination = {150.0f, 240.0f, 100.0f, 60.0f};
    placement.opacity = 0.8f;
    placement.background.a = 0.6f;
    reach_window_thumbnail_placement resolved =
        reach_host_native_overlay_placement(placement, &geometry, &ctx, pad, 11.0);
    expect_true(reach_rect_equal(resolved.destination, {62.0f, 58.0f, 100.0f, 60.0f}),
                "native placement accounts for ordinary client shadow offsets");
    ctx.content_transform_active = 1;
    ctx.content_rect = {30.0f, 40.0f, 400.0f, 200.0f};
    ctx.render_transform = {1.0f, 1.0f, 30.0f, 40.0f};
    geometry.presentation.retained = 1;
    reach_motion_set(&geometry.presentation.group.x, 10.0f);
    reach_motion_set(&geometry.presentation.group.y, 8.0f);
    reach_motion_set(&geometry.presentation.group.opacity, 0.25f);
    geometry.presentation.group.scale = {0.5f, 1.0f, 10.0, 2.0, REACH_EASING_EASE_IN_OUT, 1};
    resolved = reach_host_native_overlay_placement(placement, &geometry, &ctx, pad, 11.0);
    expect_true(reach_rect_equal(resolved.destination, {127.5f, 103.0f, 75.0f, 45.0f}),
                "native placement samples the shared translation and centered scale");
    expect_true(reach_host_scalar_equal(resolved.opacity, 0.2f) &&
                    reach_host_scalar_equal(resolved.background.a, 0.15f),
                "native content and helper background follow the shared opacity");
}

static reach_host stats_host;

static reach_result fake_stats_measure_text(void *context, const uint16_t *text, float text_size,
                                            int32_t text_weight, float *out_width)
{
    (void)context;
    (void)text_size;
    (void)text_weight;
    float width = 0.0f;
    for (; *text != 0; ++text)
        width += *text == 'M' ? 8.0f : 4.0f;
    *out_width = width;
    return REACH_OK;
}

static void stats_notify(reach_host *host, float cpu, uint64_t received, int32_t valid)
{
    reach_feature_notification notification = {};
    notification.kind = REACH_FEATURE_NOTIFICATION_SYSTEM_STATS_CHANGED;
    notification.system_stats.valid = valid;
    notification.system_stats.cpu_percent = cpu;
    notification.system_stats.memory_percent = 40.0f;
    notification.system_stats.network_received_bytes_per_second = received;
    reach_host_notify_registered_features(host, &notification);
}

static void stats_settle(reach_host *host, reach_top_bar *top_bar)
{
    reach_top_bar_build_context build = {};
    build.theme = reach_theme_default();
    build.monitor_bounds = {0.0f, 0.0f, 1920.0f, 1080.0f};
    build.dpi_scale = 1.0f;
    build.text_measure = {reinterpret_cast<void *>(1), fake_stats_measure_text};
    reach_top_bar_build_layout(top_bar, &build);
    host->dirty = {};
    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].surface->dirty_flags = 0;
}

static void test_system_stats_change_repaints_without_relayout(void)
{
    reach_host *host = &stats_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered top bar is available for the stats scenario");
    reach_host_init_layout(host);
    reach_feature_runtime *runtime = &host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR];
    runtime->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    runtime->surface->renderer.ops.measure_text = fake_stats_measure_text;
    reach_top_bar *top_bar =
        reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TOP_BAR);
    stats_settle(host, top_bar);

    stats_notify(host, 0.0f, 0, 0);
    expect_true(runtime->surface->dirty_flags == 0 && !host->dirty.layout && !host->dirty.render,
                "an invalid sample leaves the top bar clean");

    stats_notify(host, 50.0f, 0, 1);
    expect_true(runtime->surface->dirty_flags == 1 && host->dirty.layout,
                "the first valid sample relayouts because the stats appear");
    stats_settle(host, top_bar);

    stats_notify(host, 50.0f, 1200, 1);
    expect_true(runtime->surface->dirty_flags == 1 && !host->dirty.layout && !host->dirty.render,
                "a text change inside its slot repaints only the top bar");
    stats_settle(host, top_bar);

    stats_notify(host, 50.0f, 1300, 1);
    expect_true(runtime->surface->dirty_flags == 0 && !host->dirty.layout && !host->dirty.render &&
                    !host->dirty.update_requested,
                "an unchanged formatted text causes no repaint");

    stats_notify(host, 60.0f, 1300, 1);
    expect_true(runtime->surface->dirty_flags == 1 && !host->dirty.layout && !host->dirty.render,
                "another text change inside its slot repaints only the top bar");
    stats_settle(host, top_bar);

    stats_notify(host, 60.0f, UINT64_C(500) * 1024 * 1024, 1);
    expect_true(runtime->surface->dirty_flags == 1 && host->dirty.layout,
                "a text wider than its slot relayouts");
    stats_settle(host, top_bar);

    stats_notify(host, 60.0f, UINT64_C(400) * 1024 * 1024, 1);
    expect_true(runtime->surface->dirty_flags == 1 && !host->dirty.layout && !host->dirty.render,
                "a text of the same width repaints without relayout");
    stats_settle(host, top_bar);

    stats_notify(host, 0.0f, 0, 0);
    expect_true(runtime->surface->dirty_flags == 1 && host->dirty.layout,
                "losing the sample relayouts because the stats disappear");

    reach_host_destroy_registered_features(host);
}

static reach_host tick_scope_host;

static void test_feature_tick_redraw_marks_only_its_own_surface(void)
{
    reach_host *host = &tick_scope_host;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered features are available to the tick scenario");
    reach_host_init_layout(host);
    reach_feature_runtime *dock = &host->feature_runtimes[REACH_SURFACE_ID_DOCK];
    reach_feature_runtime *quick_settings =
        &host->feature_runtimes[REACH_SURFACE_ID_QUICK_SETTINGS];
    host->dirty = {};
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
        host->feature_runtimes[index].surface->dirty_flags = 0;

    reach_feature_tick_result tick = {};
    tick.redraw = 1;
    reach_host_apply_feature_tick_result(host, quick_settings, &tick);
    expect_true(quick_settings->surface->dirty_flags == 1 && dock->surface->dirty_flags == 0 &&
                    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].surface->dirty_flags == 0,
                "a ticking capsule redraws only its own surface");
    expect_true(!host->dirty.render && !host->dirty.layout,
                "a tick redraw sets no host-wide dirty state");

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
        host->feature_runtimes[index].surface->dirty_flags = 0;
    reach_feature_runtime *tray = &host->feature_runtimes[REACH_SURFACE_ID_TRAY];
    reach_host_apply_feature_tick_result(host, tray, &tick);
    expect_true(tray->surface->dirty_flags == 1 &&
                    host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].surface->dirty_flags == 1 &&
                    dock->surface->dirty_flags == 0,
                "a Tray redraw also redraws the Top Bar that shares its capsule");

    tick.relayout = 1;
    reach_host_apply_feature_tick_result(host, quick_settings, &tick);
    expect_true(host->dirty.layout && !host->dirty.render,
                "a tick relayout still relayouts every surface");

    reach_host_destroy_registered_features(host);
}

static void test_close_handoff_readiness_waits_for_observed_restore(void)
{
    expect_true(!reach_close_handoff_ready(0, REACH_OK, 0),
                "a handoff is pending until its activation completes");
    expect_true(!reach_close_handoff_ready(0, REACH_ERROR, 0),
                "a failure result is only meaningful once completed");
    expect_true(reach_close_handoff_ready(1, REACH_ERROR, 1),
                "a failed activation releases the handoff at once");
    expect_true(!reach_close_handoff_ready(1, REACH_OK, 1),
                "a completed activation with a stale minimized snapshot stays held");
    expect_true(reach_close_handoff_ready(1, REACH_OK, 0),
                "a later snapshot with the target restored releases the handoff");
}

static reach_host presentation_render_host;
static int presentation_render_failures;
static size_t presentation_render_count;
static size_t presentation_max_groups;

static int32_t presentation_root_compiles(const reach_render_command_buffer *commands,
                                          double now_seconds)
{
    const reach_motion *motions[] = {&commands->presentation.opacity, &commands->presentation.scale,
                                     &commands->presentation.y, &commands->presentation_center_x,
                                     &commands->presentation_center_y};
    reach_motion_segment segments[REACH_MOTION_MAX_SEGMENTS];
    for (const reach_motion *motion : motions)
    {
        if (motion->generation != 0 && reach_motion_active_at(motion, now_seconds) &&
            reach_motion_compile_at(motion, now_seconds, segments) == 0)
            return 0;
    }
    return 1;
}

static reach_result validating_execute(reach_render_backend *backend,
                                       const reach_render_command_buffer *commands)
{
    (void)backend;
    ++presentation_render_count;
    if (commands != nullptr && commands->group_count > presentation_max_groups)
        presentation_max_groups = commands->group_count;
    if (commands == nullptr || commands->group_count > REACH_MAX_RENDER_GROUPS ||
        commands->count > REACH_MAX_RENDER_COMMANDS ||
        reach_render_command_buffer_validate_groups(commands) != REACH_OK ||
        !commands->has_presentation || commands->presentation_center_x.generation == 0 ||
        commands->presentation_center_y.generation == 0 ||
        !presentation_root_compiles(commands, reach_motion_now_seconds()))
        ++presentation_render_failures;
    return REACH_OK;
}

static void render_presented_surface(reach_host *host, reach_surface_id id,
                                     reach_feature_surface_context ctx, const char *label)
{
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    const reach_feature_definition *definition = runtime->definition;
    (void)definition->surface_ops->arrange(runtime->capsule, &ctx);
    reach_feature_surface_geometry geometry = {};
    definition->capsule_ops->surface_geometry(runtime->capsule, &geometry);
    expect_true(geometry.presentation.retained && geometry.visible_bounds.width > 0.0f &&
                    geometry.visible_bounds.height > 0.0f,
                label);
    reach_rect_f32 translation = geometry.presentation.translation_bounds;
    float max_scale = geometry.presentation.max_scale;
    reach_host_surface_presentation_extent(&geometry.presentation.group, &translation, &max_scale);
    reach_host_surface_presentation_frame frame = reach_host_surface_presentation_frame_compute(
        geometry.visible_bounds, geometry.envelope_bounds, reach_host_surface_shadow_pad(host, id),
        translation, max_scale);
    ctx.visible_bounds = geometry.visible_bounds;
    ctx.content_rect = frame.content_rect;
    ctx.render_transform = frame.render_transform;
    ctx.content_transform_active = 1;
    ctx.retained_supported = 1;

    auto storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer *commands = storage.get();
    reach_render_command_buffer_clear(commands);
    reach_result result =
        definition->surface_ops->append_render_commands(runtime->capsule, &ctx, commands);
    expect_true(result == REACH_OK, label);
    commands->has_presentation = 1;
    commands->presentation = geometry.presentation.group;
    reach_render_command_buffer_set_content_transform(commands, ctx.content_rect,
                                                      ctx.render_transform);
    commands->presentation.bounds = commands->content_rect;
    commands->presentation_center_x =
        reach_motion_affine(&geometry.presentation.center_x, ctx.render_transform.scale_x,
                            ctx.render_transform.offset_x);
    commands->presentation_center_y =
        reach_motion_affine(&geometry.presentation.center_y, ctx.render_transform.scale_y,
                            ctx.render_transform.offset_y);

    int before = presentation_render_failures;
    if (definition->surface.popup_chrome)
    {
        reach_surface_runtime surface = {};
        surface.renderer.backend = reinterpret_cast<reach_render_backend *>(1);
        surface.renderer.ops.begin_frame = fake_begin_frame;
        surface.renderer.ops.end_frame = fake_end_frame;
        surface.renderer.ops.execute = validating_execute;
        result =
            reach_host_render_popup_surface(host, id, &surface, geometry.visible_bounds,
                                            geometry.notch_anchor_x, geometry.notch_side, commands);
    }
    else
    {
        result = validating_execute(nullptr, commands);
    }
    expect_true(result == REACH_OK && presentation_render_failures == before, label);
}

static void tick_presented_surface(reach_host *host, reach_surface_id id, double seconds)
{
    reach_test_motion_clock_advance(seconds);
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    reach_feature_tick_result tick = {};
    runtime->definition->capsule_ops->tick(runtime->capsule, seconds, &tick);
}

template <typename Open, typename Close>
static void render_presentation_phases(reach_host *host, reach_surface_id id,
                                       const reach_feature_surface_context &ctx, Open open,
                                       Close close, const char *label)
{
    open();
    render_presented_surface(host, id, ctx, label);
    tick_presented_surface(host, id, 0.05);
    render_presented_surface(host, id, ctx, label);
    close();
    tick_presented_surface(host, id, 0.04);
    render_presented_surface(host, id, ctx, label);
    open();
    render_presented_surface(host, id, ctx, label);
    tick_presented_surface(host, id, 0.6);
    render_presented_surface(host, id, ctx, label);
    close();
    render_presented_surface(host, id, ctx, label);
    tick_presented_surface(host, id, 0.6);
    tick_presented_surface(host, id, 0.6);
}

static size_t fake_switcher_window_count(const reach_window_manager *manager)
{
    (void)manager;
    return REACH_MAX_OPEN_WINDOWS;
}

static reach_result fake_switcher_window_at(const reach_window_manager *manager, size_t index,
                                            reach_window_snapshot *out_window)
{
    (void)manager;
    if (index >= REACH_MAX_OPEN_WINDOWS || out_window == nullptr)
        return REACH_ERROR;
    *out_window = {};
    out_window->id = 5000 + index;
    out_window->visible = 1;
    for (size_t at = 0; at < 48; ++at)
        out_window->title[at] = (uint16_t)('a' + (index + at) % 26);
    return REACH_OK;
}

struct presentation_contexts
{
    reach_feature_surface_context base;
    reach_feature_surface_context top;
    reach_feature_surface_context dock;
};

static presentation_contexts make_presentation_contexts(float dpi, reach_rect_f32 monitor)
{
    presentation_contexts contexts = {};
    reach_feature_surface_context &base = contexts.base;
    base.theme = reach_theme_default();
    base.monitor_bounds = monitor;
    base.available_bounds = monitor;
    base.dpi_scale = dpi;
    base.icon_size_px = (int32_t)(32.0f * dpi);
    base.text_measure = {reinterpret_cast<void *>(1), fake_stats_measure_text};
    base.transition_visible = 1;
    base.bounds_valid = 1;

    reach_feature_surface_context &top = contexts.top;
    top = base;
    top.anchor_valid = 1;
    top.anchor_button = {monitor.x + monitor.width - 48.0f * dpi, monitor.y + 4.0f * dpi,
                         32.0f * dpi, 24.0f * dpi};
    top.anchor_bar_edge_y = monitor.y + 32.0f * dpi;
    top.anchor_bar_height = 32.0f * dpi;
    top.anchor_direction = REACH_POPUP_DROP_DOWN;

    reach_feature_surface_context &dock = contexts.dock;
    dock = base;
    dock.anchor_valid = 1;
    dock.anchor_bounds = {monitor.x + monitor.width * 0.25f,
                          monitor.y + monitor.height - 64.0f * dpi, monitor.width * 0.5f,
                          56.0f * dpi};
    dock.anchor_button = {monitor.x + 8.0f * dpi, dock.anchor_bounds.y + 8.0f * dpi, 40.0f * dpi,
                          40.0f * dpi};
    dock.anchor_bar_edge_y = dock.anchor_bounds.y;
    dock.anchor_bar_height = 56.0f * dpi;
    dock.anchor_direction = REACH_POPUP_DROP_UP;
    return contexts;
}

static void test_shared_presentations_render_worst_case(void)
{
    reach_host *host = &presentation_render_host;
    presentation_render_count = 0;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered presentation consumers are available");
    reach_host_init_layout(host);

    reach_quick_settings *quick_settings =
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS);
    reach_audio_volume_session_list sessions = {};
    sessions.count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    for (size_t index = 0; index < sessions.count; ++index)
    {
        sessions.sessions[index].level = 0.5f;
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        sessions.sessions[index].icon_id = (uint64_t)(9000 + index);
        for (size_t at = 0; at < 40; ++at)
            sessions.sessions[index].label[at] = (uint16_t)('A' + (index + at) % 26);
        sessions.sessions[index].session_instance_id[0] = (uint16_t)('a' + index % 26);
        sessions.sessions[index].session_instance_id[1] = (uint16_t)('a' + index / 26);
    }
    reach_quick_settings_apply_sessions(quick_settings, &sessions);

    reach_battery *battery =
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY);
    reach_top_bar *top_bar = reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TRAY);
    reach_context_menu *menu =
        reach_host_feature_capsule<reach_context_menu>(host, REACH_SURFACE_ID_CONTEXT_MENU);
    reach_launcher *launcher =
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER);
    reach_clipboard_feature *clipboard =
        reach_host_feature_capsule<reach_clipboard_feature>(host, REACH_SURFACE_ID_CLIPBOARD);
    reach_system_hud *hud =
        reach_host_feature_capsule<reach_system_hud>(host, REACH_SURFACE_ID_SYSTEM_HUD);

    reach_window_manager_port switcher_port = {};
    switcher_port.ops.window_count = fake_switcher_window_count;
    switcher_port.ops.window_at = fake_switcher_window_at;
    switcher_port.ops.frame_bounds = fake_preview_frame_bounds;
    reach_window_tracking *switcher_windows = nullptr;
    expect_true(reach_window_tracking_create(switcher_port, &switcher_windows) == REACH_OK,
                "Switcher has the maximum window count");
    (void)reach_window_tracking_refresh(switcher_windows, nullptr);
    reach_switcher *switcher =
        reach_host_feature_capsule<reach_switcher>(host, REACH_SURFACE_ID_SWITCHER);
    reach_switcher_attach_services(switcher, nullptr, switcher_windows);
    const reach_feature_capsule_ops *switcher_ops =
        host->feature_runtimes[REACH_SURFACE_ID_SWITCHER].definition->capsule_ops;
    auto switch_event = [&](reach_ui_event_type type)
    {
        reach_ui_event event = {};
        event.type = type;
        reach_capsule_event_result result = {};
        switcher_ops->handle_event(switcher, &event, &result);
    };

    uint32_t power_commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    uint32_t power_icons[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    size_t power_count = 0;
    reach_context_menu_build_power_commands(power_commands, power_icons, &power_count);
    uint32_t item_commands[REACH_CONTEXT_MENU_MAX_ITEMS] = {};
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
        item_commands[index] = power_commands[index % (power_count > 0 ? power_count : 1)];
    reach_menu_request request = {};
    request.command_count = REACH_CONTEXT_MENU_MAX_ITEMS;
    static const uint16_t app_name[] = {'A', 'n', ' ', 'e', 'x', 't', 'r', 'e', 'm', 'e', 'l',
                                        'y', ' ', 'l', 'o', 'n', 'g', ' ', 'a', 'p', 'p', 0};

    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f};
    const reach_rect_f32 monitors[] = {{0.0f, 0.0f, 3840.0f, 2160.0f},
                                       {-1920.0f, 0.0f, 1920.0f, 1080.0f},
                                       {0.0f, 0.0f, 360.0f, 280.0f}};
    for (float dpi : dpis)
    {
        for (reach_rect_f32 monitor : monitors)
        {
            host->layout_dpi_scale = dpi;
            presentation_contexts contexts = make_presentation_contexts(dpi, monitor);
            const reach_feature_surface_context &base = contexts.base;
            const reach_feature_surface_context &top = contexts.top;
            const reach_feature_surface_context &dock = contexts.dock;

            for (const reach_feature_surface_context *anchor : {&top, &dock})
            {
                render_presentation_phases(
                    host, REACH_SURFACE_ID_QUICK_SETTINGS, *anchor,
                    [&] { (void)reach_quick_settings_set_open(quick_settings, 1); },
                    [&] { (void)reach_quick_settings_set_open(quick_settings, 0); },
                    "Quick Settings renders every presentation phase");
                render_presentation_phases(
                    host, REACH_SURFACE_ID_BATTERY, *anchor,
                    [&] { (void)reach_battery_set_open(battery, 1); },
                    [&] { (void)reach_battery_set_open(battery, 0); },
                    "Battery renders every presentation phase");
                render_presentation_phases(
                    host, REACH_SURFACE_ID_TRAY, *anchor,
                    [&] { (void)reach_top_bar_set_tray_popup_open(top_bar, 1); },
                    [&] { (void)reach_top_bar_set_tray_popup_open(top_bar, 0); },
                    "the tray popup renders every presentation phase");
                reach_context_menu_open_context open = {};
                open.theme = base.theme;
                open.monitor = monitor;
                open.dpi_scale = dpi;
                open.anchor_button = anchor->anchor_button;
                open.bar_edge_y = anchor->anchor_bar_edge_y;
                open.drop_direction = anchor->anchor_direction;
                open.anchored = 1;
                open.item_commands = item_commands;
                open.item_count = REACH_CONTEXT_MENU_MAX_ITEMS;
                open.request = &request;
                render_presentation_phases(
                    host, REACH_SURFACE_ID_CONTEXT_MENU, *anchor,
                    [&] { reach_context_menu_open_for_item(menu, 0, &open); },
                    [&] { (void)reach_context_menu_set_open(menu, 0); },
                    "the context menu renders every presentation phase");
            }
            render_presentation_phases(
                host, REACH_SURFACE_ID_LAUNCHER, base,
                [&] { (void)reach_launcher_set_open(launcher, 1); },
                [&] { (void)reach_launcher_set_open(launcher, 0); },
                "Launcher renders every presentation phase");
            render_presentation_phases(
                host, REACH_SURFACE_ID_CLIPBOARD, base,
                [&] { (void)reach_clipboard_set_open(clipboard, 1); },
                [&] { (void)reach_clipboard_set_open(clipboard, 0); },
                "Clipboard renders every presentation phase");
            render_presentation_phases(
                host, REACH_SURFACE_ID_SYSTEM_HUD, dock,
                [&] { reach_system_hud_show_app_removed(hud, app_name); },
                [&] { tick_presented_surface(host, REACH_SURFACE_ID_SYSTEM_HUD, 2.9); },
                "System HUD renders every presentation phase");
            render_presentation_phases(
                host, REACH_SURFACE_ID_SWITCHER, base,
                [&] { switch_event(REACH_UI_EVENT_APP_SWITCH_BEGIN); },
                [&] { switch_event(REACH_UI_EVENT_APP_SWITCH_CANCEL); },
                "Switcher renders every presentation phase");
        }
    }
    expect_true(presentation_render_count == 4 * 3 * 12 * 6,
                "every consumer renders every phase at every DPI and monitor width");
    reach_host_destroy_registered_features(host);
    reach_window_tracking_destroy(switcher_windows);
}

static reach_result fake_tray_refresh(reach_tray_provider *provider)
{
    (void)provider;
    return REACH_OK;
}

static int32_t fake_tray_needs_refresh(const reach_tray_provider *provider)
{
    (void)provider;
    return 0;
}

static size_t fake_tray_item_count(const reach_tray_provider *provider)
{
    (void)provider;
    return REACH_MAX_TRAY_ITEMS;
}

static reach_result fake_tray_item_at(const reach_tray_provider *provider, size_t index,
                                      reach_tray_item *out_item)
{
    (void)provider;
    if (index >= REACH_MAX_TRAY_ITEMS || out_item == nullptr)
        return REACH_ERROR;
    *out_item = {};
    out_item->id = (uint32_t)(100 + index);
    out_item->icon_id = index % 3 == 0 ? 0 : (uint64_t)(7000 + index);
    for (size_t at = 0; at < 100; ++at)
        out_item->title[at] = (uint16_t)('A' + (index + at) % 26);
    return REACH_OK;
}

static reach_result fake_tray_activate(reach_tray_provider *provider, uint32_t item_id,
                                       reach_tray_action action)
{
    (void)provider;
    (void)item_id;
    (void)action;
    return REACH_OK;
}

static reach_result fake_tray_cancel_menu(reach_tray_provider *provider)
{
    (void)provider;
    return REACH_OK;
}

static int32_t fake_tray_take_retired(reach_tray_provider *provider, uint64_t *out_icon_id)
{
    (void)provider;
    (void)out_icon_id;
    return 0;
}

static void fake_tray_release_retired(reach_tray_provider *provider, uint64_t icon_id)
{
    (void)provider;
    (void)icon_id;
}

static void fake_tray_destroy(reach_tray_provider *provider)
{
    (void)provider;
}

static reach_tray_provider_port fake_full_tray_port(void)
{
    reach_tray_provider_port port = {};
    port.provider = reinterpret_cast<reach_tray_provider *>(1);
    port.ops.refresh = fake_tray_refresh;
    port.ops.needs_refresh = fake_tray_needs_refresh;
    port.ops.item_count = fake_tray_item_count;
    port.ops.item_at = fake_tray_item_at;
    port.ops.activate = fake_tray_activate;
    port.ops.cancel_active_menu = fake_tray_cancel_menu;
    port.ops.take_retired_icon = fake_tray_take_retired;
    port.ops.release_retired_icon = fake_tray_release_retired;
    port.ops.destroy = fake_tray_destroy;
    return port;
}

static size_t fake_dock_window_count(const reach_window_manager *manager)
{
    (void)manager;
    return REACH_MAX_DOCK_RUNNING_APPS;
}

static reach_result fake_dock_window_at(const reach_window_manager *manager, size_t index,
                                        reach_window_snapshot *out_window)
{
    (void)manager;
    if (index >= REACH_MAX_DOCK_RUNNING_APPS || out_window == nullptr)
        return REACH_ERROR;
    *out_window = {};
    out_window->id = 9000 + index;
    out_window->visible = 1;
    char path[64] = {};
    snprintf(path, sizeof(path), "C:\\running\\app_%zu.exe", index);
    uint16_t runtime_path[260] = {};
    reach_copy_ascii_to_utf16(runtime_path, 260, path);
    reach_application_identity_add_runtime_path(&out_window->identity, runtime_path);
    reach_copy_ascii_to_utf16(out_window->title, 260, "window");
    return REACH_OK;
}

static void render_flat_surface(reach_host *host, reach_surface_id id,
                                reach_feature_surface_context ctx, const char *label)
{
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    const reach_feature_definition *definition = runtime->definition;
    (void)definition->surface_ops->arrange(runtime->capsule, &ctx);
    reach_feature_surface_geometry geometry = {};
    definition->capsule_ops->surface_geometry(runtime->capsule, &geometry);
    ctx.visible_bounds = geometry.visible_bounds;
    ctx.render_bounds = {0.0f, 0.0f, geometry.visible_bounds.width, geometry.visible_bounds.height};
    ctx.retained_supported = 1;

    auto storage = std::make_unique<reach_render_command_buffer>();
    reach_render_command_buffer *commands = storage.get();
    reach_render_command_buffer_clear(commands);
    reach_result result =
        definition->surface_ops->append_render_commands(runtime->capsule, &ctx, commands);
    expect_true(result == REACH_OK, label);
    expect_true(commands->group_count <= REACH_MAX_RENDER_GROUPS &&
                    commands->count <= REACH_MAX_RENDER_COMMANDS &&
                    reach_render_command_buffer_validate_groups(commands) == REACH_OK,
                label);
}

static reach_pointer_event make_pointer_event(reach_pointer_event_kind kind,
                                              reach_pointer_coordinate_space space, float x,
                                              float y)
{
    reach_pointer_event event = {};
    event.kind = kind;
    event.coordinate_space = space;
    event.x = (int32_t)x;
    event.y = (int32_t)y;
    event.button = REACH_POINTER_BUTTON_PRIMARY;
    event.surface_relation = REACH_POINTER_SURFACE_INSIDE;
    return event;
}

static void send_pointer(const reach_feature_capsule_ops *ops, void *capsule,
                         const reach_pointer_event &event)
{
    reach_capsule_pointer_result result = {};
    ops->handle_pointer(capsule, &event, &result);
}

static size_t find_press_targets(const reach_feature_capsule_ops *ops, void *capsule,
                                 reach_rect_f32 area, reach_pointer_coordinate_space space,
                                 float step, reach_pointer_event *out, size_t max)
{
    size_t found = 0;
    for (float y = ceilf(area.y); y <= area.y + area.height; y += step)
    {
        for (float x = ceilf(area.x); x <= area.x + area.width; x += step)
        {
            reach_pointer_event down = make_pointer_event(REACH_POINTER_EVENT_DOWN, space, x, y);
            send_pointer(ops, capsule, down);
            if (!ops->pointer_sequence_active(capsule))
                continue;
            send_pointer(ops, capsule, make_pointer_event(REACH_POINTER_EVENT_CANCEL, space, x, y));
            int32_t distinct = 1;
            for (size_t index = 0; index < found; ++index)
            {
                if (fabsf((float)out[index].x - x) < step * 4.0f &&
                    fabsf((float)out[index].y - y) < step * 4.0f)
                    distinct = 0;
            }
            if (distinct)
            {
                out[found++] = down;
                if (found == max)
                    return found;
            }
        }
    }
    return found;
}

template <typename Render, typename Tick>
static void render_press_cycles(const reach_feature_capsule_ops *ops, void *capsule,
                                reach_pointer_coordinate_space space,
                                const reach_pointer_event &down, Render render, Tick tick)
{
    reach_pointer_event away =
        make_pointer_event(REACH_POINTER_EVENT_MOVE, space, -100000.0f, -100000.0f);
    reach_pointer_event release = away;
    release.kind = REACH_POINTER_EVENT_UP;

    send_pointer(ops, capsule, down);
    render();
    tick(0.03);
    render();
    send_pointer(ops, capsule, away);
    render();
    for (double seconds : {0.02, 0.03, 0.05, 0.08, 0.15, 0.5})
    {
        tick(seconds);
        render();
    }
    send_pointer(ops, capsule, release);
    tick(0.6);

    send_pointer(ops, capsule, down);
    tick(0.05);
    render();
    send_pointer(ops, capsule, release);
    render();
    tick(0.04);
    render();
    send_pointer(ops, capsule, down);
    render();
    tick(0.02);
    render();
    send_pointer(ops, capsule, release);
    render();
    for (double seconds : {0.03, 0.06, 0.12, 0.5})
    {
        tick(seconds);
        render();
    }
}

template <typename Open, typename Render>
static void render_pressed_surface(reach_host *host, reach_surface_id id, Open open, Render render,
                                   float step, const char *label)
{
    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    const reach_feature_capsule_ops *ops = runtime->definition->capsule_ops;
    open();
    tick_presented_surface(host, id, 0.6);
    render();
    reach_feature_surface_geometry geometry = {};
    ops->surface_geometry(runtime->capsule, &geometry);
    int32_t local = runtime->definition->surface.cls == REACH_SURFACE_CLASS_POPUP;
    reach_pointer_coordinate_space space =
        local ? REACH_POINTER_COORDINATE_SURFACE_LOCAL : REACH_POINTER_COORDINATE_SCREEN;
    reach_rect_f32 area = local ? reach_rect_f32{0.0f, 0.0f, geometry.visible_bounds.width,
                                                 geometry.visible_bounds.height}
                                : geometry.visible_bounds;
    reach_pointer_event targets[3] = {};
    size_t count = find_press_targets(ops, runtime->capsule, area, space, step, targets, 3);
    expect_true(count > 0, label);
    for (size_t index = 0; index < count; ++index)
    {
        render_press_cycles(ops, runtime->capsule, space, targets[index], render,
                            [&](double seconds) { tick_presented_surface(host, id, seconds); });
    }
}

static void test_max_items_and_pressed_states_render_worst_case(void)
{
    reach_host *host = &presentation_render_host;
    presentation_render_count = 0;
    presentation_render_failures = 0;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered presentation consumers are available for the maximum-item scenario");
    reach_host_init_layout(host);

    reach_tray_service *tray_service = nullptr;
    expect_true(reach_tray_service_create(fake_full_tray_port(), &tray_service) == REACH_OK,
                "the maximum tray is available");
    reach_top_bar *top_bar = reach_host_feature_capsule<reach_top_bar>(host, REACH_SURFACE_ID_TRAY);
    reach_top_bar_attach_services(top_bar, nullptr, nullptr, nullptr, nullptr, nullptr,
                                  tray_service);
    expect_true(reach_top_bar_refresh_tray(top_bar) == REACH_OK &&
                    reach_top_bar_tray_item_count(top_bar) == REACH_MAX_TRAY_ITEMS,
                "the tray holds its maximum item count");

    reach_clipboard_feature *clipboard =
        reach_host_feature_capsule<reach_clipboard_feature>(host, REACH_SURFACE_ID_CLIPBOARD);
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
    {
        reach_clipboard_item item = {};
        item.id = (uint64_t)(index + 1);
        item.content_hash = (uint64_t)(index + 1) * 2654435761u;
        item.kind = index % 4 == 0 ? REACH_CLIPBOARD_ITEM_IMAGE : REACH_CLIPBOARD_ITEM_TEXT;
        item.thumbnail_id = item.kind == REACH_CLIPBOARD_ITEM_IMAGE ? 8000 + index : 0;
        item.image_width = 640;
        item.image_height = 480;
        for (size_t at = 0; at < 120; ++at)
            item.preview[at] = (uint16_t)('a' + (index + at) % 26);
        reach_clipboard_insert_outcome outcome = {};
        reach_clipboard_insert_captured(clipboard, item, &outcome);
    }
    expect_true(reach_clipboard_item_count(clipboard) == REACH_CLIPBOARD_MAX_ITEMS,
                "Clipboard holds its maximum item count");

    reach_launcher *launcher =
        reach_host_feature_capsule<reach_launcher>(host, REACH_SURFACE_ID_LAUNCHER);
    static reach_search_candidate candidates[REACH_SEARCH_MAX_RESULTS];
    for (size_t index = 0; index < REACH_SEARCH_MAX_RESULTS; ++index)
    {
        candidates[index] = {};
        for (size_t at = 0; at < REACH_SEARCH_RESULT_NAME_CAPACITY - 1; ++at)
            candidates[index].name[at] = (uint16_t)('a' + (index + at) % 26);
        for (size_t at = 0; at < REACH_SEARCH_RESULT_PATH_CAPACITY - 1; ++at)
            candidates[index].path[at] = (uint16_t)('a' + (index * 3 + at) % 26);
        candidates[index].kind = (reach_search_result_kind)(index % 7);
    }
    static const uint16_t query[] = {'a', 0};
    auto open_launcher = [&]
    {
        (void)reach_launcher_set_open(launcher, 1);
        (void)reach_launcher_set_query(launcher, query);
        (void)reach_launcher_set_results(launcher, candidates, REACH_SEARCH_MAX_RESULTS);
    };

    reach_dock *dock = reach_host_feature_capsule<reach_dock>(host, REACH_SURFACE_ID_DOCK);
    static reach_pinned_app_model pins[REACH_MAX_PINNED_APPS];
    for (size_t index = 0; index < REACH_MAX_PINNED_APPS; ++index)
    {
        char path[64] = {};
        snprintf(path, sizeof(path), "C:\\pins\\pin_%zu.exe", index);
        pins[index].id = (uint32_t)(index + 1);
        pins[index].application.launch.kind = REACH_APPLICATION_LAUNCH_EXECUTABLE;
        reach_copy_ascii_to_utf16(pins[index].application.launch.path, 260, path);
        uint16_t runtime_path[260] = {};
        reach_copy_ascii_to_utf16(runtime_path, 260, path);
        reach_application_identity_add_runtime_path(&pins[index].application.identity,
                                                    runtime_path);
    }
    reach_window_manager_port dock_port = {};
    dock_port.ops.window_count = fake_dock_window_count;
    dock_port.ops.window_at = fake_dock_window_at;
    dock_port.ops.frame_bounds = fake_preview_frame_bounds;
    reach_window_tracking *dock_windows = nullptr;
    expect_true(reach_window_tracking_create(dock_port, &dock_windows) == REACH_OK,
                "the Dock has the maximum running window count");
    (void)reach_window_tracking_refresh(dock_windows, nullptr);
    reach_dock_attach_services(dock, nullptr, dock_windows);
    reach_dock_apply_pinned_apps(dock, pins, REACH_MAX_PINNED_APPS);

    reach_quick_settings *quick_settings =
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS);
    reach_audio_volume_session_list sessions = {};
    sessions.count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    for (size_t index = 0; index < sessions.count; ++index)
    {
        sessions.sessions[index].level = 0.5f;
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        sessions.sessions[index].icon_id = (uint64_t)(9000 + index);
        for (size_t at = 0; at < 40; ++at)
            sessions.sessions[index].label[at] = (uint16_t)('A' + (index + at) % 26);
        sessions.sessions[index].session_instance_id[0] = (uint16_t)('a' + index % 26);
        sessions.sessions[index].session_instance_id[1] = (uint16_t)('a' + index / 26);
    }
    reach_quick_settings_apply_sessions(quick_settings, &sessions);
    reach_battery *battery =
        reach_host_feature_capsule<reach_battery>(host, REACH_SURFACE_ID_BATTERY);
    (void)reach_battery_set_power(battery, 50, 0);

    reach_feature_runtime *top_bar_runtime = &host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR];
    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f};
    const reach_rect_f32 monitors[] = {{0.0f, 0.0f, 3840.0f, 2160.0f},
                                       {-1920.0f, 0.0f, 1920.0f, 1080.0f},
                                       {0.0f, 0.0f, 360.0f, 280.0f}};
    for (float dpi : dpis)
    {
        for (reach_rect_f32 monitor : monitors)
        {
            host->layout_dpi_scale = dpi;
            presentation_contexts contexts = make_presentation_contexts(dpi, monitor);
            const reach_feature_surface_context &base = contexts.base;
            (void)top_bar_runtime->definition->surface_ops->arrange(top_bar_runtime->capsule,
                                                                    &base);
            expect_true(monitor.width < 1000.0f || reach_top_bar_tray_overflow_start(top_bar) ==
                                                       REACH_TOP_BAR_TRAY_INLINE_ICONS,
                        "the maximum tray overflows into the dropdown");

            for (const reach_feature_surface_context *anchor : {&contexts.top, &contexts.dock})
            {
                render_presentation_phases(
                    host, REACH_SURFACE_ID_TRAY, *anchor,
                    [&] { (void)reach_top_bar_set_tray_popup_open(top_bar, 1); },
                    [&] { (void)reach_top_bar_set_tray_popup_open(top_bar, 0); },
                    "the maximum tray renders every presentation phase");
                render_pressed_surface(
                    host, REACH_SURFACE_ID_TRAY,
                    [&] { (void)reach_top_bar_set_tray_popup_open(top_bar, 1); },
                    [&]
                    {
                        render_presented_surface(host, REACH_SURFACE_ID_TRAY, *anchor,
                                                 "the maximum tray renders while pressed");
                    },
                    6.0f, "the tray popup has pressable items");
                (void)reach_top_bar_set_tray_popup_open(top_bar, 0);
                tick_presented_surface(host, REACH_SURFACE_ID_TRAY, 0.6);

                render_pressed_surface(
                    host, REACH_SURFACE_ID_QUICK_SETTINGS,
                    [&] { (void)reach_quick_settings_set_open(quick_settings, 1); },
                    [&]
                    {
                        render_presented_surface(host, REACH_SURFACE_ID_QUICK_SETTINGS, *anchor,
                                                 "Quick Settings renders while pressed");
                    },
                    6.0f, "Quick Settings has pressable controls");
                (void)reach_quick_settings_set_open(quick_settings, 0);
                tick_presented_surface(host, REACH_SURFACE_ID_QUICK_SETTINGS, 0.6);

                render_pressed_surface(
                    host, REACH_SURFACE_ID_BATTERY,
                    [&] { (void)reach_battery_set_open(battery, 1); },
                    [&]
                    {
                        render_presented_surface(host, REACH_SURFACE_ID_BATTERY, *anchor,
                                                 "Battery renders while pressed");
                    },
                    6.0f, "Battery has a pressable saver row");
                (void)reach_battery_set_open(battery, 0);
                tick_presented_surface(host, REACH_SURFACE_ID_BATTERY, 0.6);
            }

            render_presentation_phases(
                host, REACH_SURFACE_ID_CLIPBOARD, base,
                [&] { (void)reach_clipboard_set_open(clipboard, 1); },
                [&] { (void)reach_clipboard_set_open(clipboard, 0); },
                "the full Clipboard renders every presentation phase");
            render_presentation_phases(
                host, REACH_SURFACE_ID_LAUNCHER, base, open_launcher,
                [&] { (void)reach_launcher_set_open(launcher, 0); },
                "the full Launcher renders every presentation phase");

            render_pressed_surface(
                host, REACH_SURFACE_ID_TOP_BAR, [] {},
                [&]
                {
                    render_flat_surface(host, REACH_SURFACE_ID_TOP_BAR, base,
                                        "the Top Bar renders while pressed");
                },
                6.0f, "the Top Bar has pressable controls");
            render_pressed_surface(
                host, REACH_SURFACE_ID_DOCK, [] {},
                [&]
                {
                    render_flat_surface(host, REACH_SURFACE_ID_DOCK, base,
                                        "the full Dock renders while pressed");
                },
                1.0f, "the Dock has pressable items");
        }
    }
    expect_true(presentation_render_failures == 0, "every worst-case render stays valid");
    expect_true(presentation_render_count > 0, "the maximum-item scenario rendered");
    reach_host_destroy_registered_features(host);
    reach_window_tracking_destroy(dock_windows);
    reach_tray_service_destroy(tray_service);
}

static void test_quick_settings_section_motion_renders_worst_case(void)
{
    reach_host *host = &presentation_render_host;
    presentation_render_count = 0;
    presentation_render_failures = 0;
    presentation_max_groups = 0;
    reach_host_init_feature_registry(host);
    expect_true(reach_host_create_registered_features(host) == REACH_OK,
                "registered features are available for Quick Settings section motion");
    reach_host_init_layout(host);

    reach_quick_settings *quick_settings =
        reach_host_feature_capsule<reach_quick_settings>(host, REACH_SURFACE_ID_QUICK_SETTINGS);
    reach_audio_volume_session_list sessions = {};
    sessions.count = REACH_AUDIO_VOLUME_MAX_SESSIONS;
    for (size_t index = 0; index < sessions.count; ++index)
    {
        sessions.sessions[index].level = (float)(index + 1) / (float)sessions.count;
        sessions.sessions[index].muted = (int32_t)(index % 2);
        sessions.sessions[index].process_id = (uint32_t)(1000 + index);
        sessions.sessions[index].icon_id = index % 3 == 0 ? 0 : (uint64_t)(9000 + index);
        for (size_t at = 0; at < 40; ++at)
            sessions.sessions[index].label[at] = (uint16_t)('A' + (index + at) % 26);
        sessions.sessions[index].session_instance_id[0] = (uint16_t)('a' + index % 26);
        sessions.sessions[index].session_instance_id[1] = (uint16_t)('a' + index / 26);
    }
    reach_audio_output_device_list devices = {};
    devices.count = REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
    for (size_t index = 0; index < devices.count; ++index)
    {
        reach_audio_output_device &device = devices.devices[index];
        device.is_default = index == 0;
        device.icon_id = index % 2 == 0 ? 0 : (uint64_t)(8000 + index);
        device.device_id[0] = (uint16_t)('a' + index);
        size_t at = 0;
        for (; at < 30; ++at)
            device.label[at] = (uint16_t)('A' + (index + at) % 26);
        device.label[at++] = ' ';
        device.label[at++] = '(';
        for (size_t tail = 0; tail < 30; ++tail)
            device.label[at++] = (uint16_t)('a' + (index + tail) % 26);
        device.label[at++] = ')';
    }
    reach_audio_volume_session_list shifted_sessions = sessions;
    reach_audio_output_device_list shifted_devices = devices;
    for (size_t index = 0; index < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++index)
    {
        shifted_sessions.sessions[index] = sessions.sessions[(index + 5) % sessions.count];
        shifted_sessions.sessions[index].session_instance_id[0] =
            index % 2 == 0 ? (uint16_t)('A' + index)
                           : sessions.sessions[index].session_instance_id[0];
        shifted_devices.devices[index] = devices.devices[(index + 7) % devices.count];
        shifted_devices.devices[index].device_id[0] =
            index % 3 == 0 ? (uint16_t)('A' + index) : devices.devices[index].device_id[0];
    }
    reach_network_state network = {};
    network.kind = REACH_NETWORK_KIND_WIFI;
    network.signal_strength = 3;
    for (size_t at = 0; at < 40; ++at)
        network.label[at] = (uint16_t)('a' + at % 26);
    reach_bluetooth_state bluetooth = {};
    bluetooth.available = 1;
    reach_brightness_state brightness = {};
    brightness.available = 1;
    brightness.level = 0.4f;
    auto set_system = [&](int32_t active)
    {
        network.connected = active;
        bluetooth.enabled = active;
        reach_quick_settings_system_apply_result applied = {};
        reach_quick_settings_apply_system_states(quick_settings, &network, &bluetooth, &brightness,
                                                 1, 0, &applied);
    };

    const float dpis[] = {1.0f, 1.25f, 1.5f, 2.0f};
    const reach_rect_f32 monitors[] = {{0.0f, 0.0f, 3840.0f, 2160.0f},
                                       {-1920.0f, 0.0f, 1920.0f, 1080.0f},
                                       {0.0f, 0.0f, 360.0f, 280.0f}};
    const reach_surface_id id = REACH_SURFACE_ID_QUICK_SETTINGS;
    const reach_feature_capsule_ops *ops = host->feature_runtimes[id].definition->capsule_ops;
    void *capsule = host->feature_runtimes[id].capsule;
    const char *label = "Quick Settings renders section motion within budget";
    for (float dpi : dpis)
    {
        for (reach_rect_f32 monitor : monitors)
        {
            host->layout_dpi_scale = dpi;
            presentation_contexts contexts = make_presentation_contexts(dpi, monitor);
            for (const reach_feature_surface_context *anchor : {&contexts.top, &contexts.dock})
            {
                auto render = [&] { render_presented_surface(host, id, *anchor, label); };
                auto step = [&](double seconds)
                {
                    tick_presented_surface(host, id, seconds);
                    uint64_t retired[2 * (REACH_AUDIO_VOLUME_MAX_SESSIONS +
                                          REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES)];
                    (void)reach_quick_settings_take_retired_render_icons(
                        quick_settings, retired, sizeof(retired) / sizeof(retired[0]));
                    render();
                };
                reach_quick_settings_apply_sessions(quick_settings, &sessions);
                reach_quick_settings_apply_output_devices(quick_settings, &devices);
                set_system(0);
                (void)reach_quick_settings_set_open(quick_settings, 1);
                render();
                step(0.6);
                (void)reach_quick_settings_toggle_expanded(quick_settings);
                set_system(1);
                render();
                step(0.06);
                (void)reach_quick_settings_toggle_output_devices(quick_settings);
                render();
                step(0.03);
                reach_quick_settings_apply_sessions(quick_settings, &shifted_sessions);
                reach_quick_settings_apply_output_devices(quick_settings, &shifted_devices);
                render();
                step(0.03);
                reach_quick_settings_apply_sessions(quick_settings, &sessions);
                reach_quick_settings_apply_output_devices(quick_settings, &devices);
                render();
                step(0.03);
                (void)reach_quick_settings_toggle_output_devices(quick_settings);
                set_system(0);
                render();
                step(0.02);
                (void)reach_quick_settings_toggle_expanded(quick_settings);
                render();
                for (size_t frame = 0; frame < 15; ++frame)
                    step(0.016);
                step(0.6);
                (void)reach_quick_settings_toggle_output_devices(quick_settings);
                for (size_t frame = 0; frame < 10; ++frame)
                    step(0.016);
                (void)reach_quick_settings_toggle_expanded(quick_settings);
                set_system(1);
                for (size_t frame = 0; frame < 10; ++frame)
                    step(0.016);
                (void)reach_quick_settings_set_open(quick_settings, 0);
                render();
                tick_presented_surface(host, id, 0.6);
                tick_presented_surface(host, id, 0.6);
                if (reach_quick_settings_state_ptr(quick_settings)->model.expanded)
                    (void)reach_quick_settings_toggle_expanded(quick_settings);
                if (reach_quick_settings_state_ptr(quick_settings)->model.output_devices_expanded)
                    (void)reach_quick_settings_toggle_output_devices(quick_settings);

                (void)reach_quick_settings_set_open(quick_settings, 1);
                (void)reach_quick_settings_toggle_expanded(quick_settings);
                render();
                step(0.6);
                reach_feature_surface_geometry geometry = {};
                ops->surface_geometry(capsule, &geometry);
                reach_pointer_event press = {};
                expect_true(find_press_targets(ops, capsule,
                                               {0.0f, 0.0f, geometry.visible_bounds.width,
                                                geometry.visible_bounds.height},
                                               REACH_POINTER_COORDINATE_SURFACE_LOCAL, 6.0f, &press,
                                               1) == 1,
                            "Quick Settings has a control to hold pressed during list churn");
                send_pointer(ops, capsule, press);
                reach_audio_volume_session_list replaced_sessions = sessions;
                for (size_t index = 0; index + 1 < sessions.count; ++index)
                    replaced_sessions.sessions[index] = sessions.sessions[index + 1];
                replaced_sessions.sessions[sessions.count - 1].session_instance_id[0] = 'Z';
                reach_quick_settings_apply_sessions(quick_settings, &replaced_sessions);
                (void)reach_quick_settings_toggle_output_devices(quick_settings);
                step(0.004);
                reach_audio_volume_session_list no_sessions = {};
                reach_quick_settings_apply_sessions(quick_settings, &no_sessions);
                step(0.004);
                for (size_t removed = 1; removed < devices.count; ++removed)
                {
                    reach_audio_output_device_list churned = devices;
                    for (size_t index = 0; index < churned.count; ++index)
                    {
                        size_t source = index + removed;
                        reach_audio_output_device &device = churned.devices[index];
                        device = devices.devices[source % devices.count];
                        if (source >= devices.count)
                            device.device_id[0] = (uint16_t)('A' + source - devices.count);
                        device.is_default = index == 0;
                    }
                    reach_quick_settings_apply_output_devices(quick_settings, &churned);
                    step(0.004);
                }
                for (size_t frame = 0; frame < 6; ++frame)
                    step(0.016);
                send_pointer(ops, capsule,
                             make_pointer_event(REACH_POINTER_EVENT_UP,
                                                REACH_POINTER_COORDINATE_SURFACE_LOCAL, -100000.0f,
                                                -100000.0f));
                step(0.6);
                (void)reach_quick_settings_set_open(quick_settings, 0);
                render();
                tick_presented_surface(host, id, 0.6);
                tick_presented_surface(host, id, 0.6);
                if (reach_quick_settings_state_ptr(quick_settings)->model.expanded)
                    (void)reach_quick_settings_toggle_expanded(quick_settings);
                if (reach_quick_settings_state_ptr(quick_settings)->model.output_devices_expanded)
                    (void)reach_quick_settings_toggle_output_devices(quick_settings);
            }
        }
    }
    expect_true(presentation_render_failures == 0,
                "every Quick Settings section-motion render stays valid");
    expect_true(presentation_render_count == 4 * 3 * 2 * 77,
                "Quick Settings renders every section-motion step at every DPI and monitor");
    const size_t largest_rows = (size_t)REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES * 5 +
                                (size_t)REACH_AUDIO_VOLUME_MAX_SESSIONS * 7;
    expect_true(presentation_max_groups >= largest_rows + 27,
                "Quick Settings rendered every row at its largest in both sections while pressed");
    reach_host_destroy_registered_features(host);
}

int main(void)
{
    test_close_handoff_readiness_waits_for_observed_restore();
    test_native_overlay_placement_follows_surface_presentation();
    test_order_invalidation_rechains_without_replaying_visibility();
    test_app_band_surface_does_not_invalidate_topmost_order();
    test_window_manipulation_relevance_survives_unavailable_pointer();
    test_window_manipulation_tracks_pointer_monitor_membership();
    test_scaled_presentation_keeps_native_envelope_stationary();
    test_presentation_extent_covers_spring_overshoot();
    test_window_state_commits_terminal_subpixel_position();
    test_popup_pointer_coordinates_are_surface_local();
    test_popup_activation_uses_owner_identity();
    test_power_popup_resolves_its_exact_top_bar_owner();
    test_window_list_region_crossing_uses_the_observed_point();
    test_captured_release_precedes_exclusive_popup();
    test_popup_trigger_press_bypasses_unrelated_open_popup();
    test_foreground_reconciles_after_window_discovery();
    test_focus_restore_follows_the_close_intent();
    test_every_dismissable_surface_reaches_the_shared_close_path();
    test_every_popup_names_the_control_that_holds_it_open();
    test_registered_feature_lifecycle();
    test_registered_surface_input_passthrough();
    test_frame_scheduling_does_not_present_a_closed_surface();
    test_popup_surface_retains_presentation();
    test_registered_surface_frame_uses_declared_anchor();
    test_registered_surface_frame_syncs_native_overlay();
    test_forced_bars_hold_through_a_closing_stage();
    test_bar_conditions_sync_once_from_host_state();
    test_dropped_bar_enter_reveals_after_manipulation_clears();
    test_dropped_bar_enter_is_discarded_by_leave_or_committed_hide();
    test_bar_visibility_request_reconciles_idle_top_bar();
    test_switcher_publishes_arranged_surface_geometry();
    test_system_stats_change_repaints_without_relayout();
    test_feature_tick_redraw_marks_only_its_own_surface();
    test_shared_presentations_render_worst_case();
    test_max_items_and_pressed_states_render_worst_case();
    test_quick_settings_section_motion_renders_worst_case();

    if (failures != 0)
    {
        fprintf(stderr, "%d host layout test failure(s)\n", failures);
        return 1;
    }
    printf("host layout tests passed\n");
    return 0;
}
