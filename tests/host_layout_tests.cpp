#include "host_internal.h"

#include <stdint.h>
#include <stdio.h>

enum fake_window_call_kind
{
    FAKE_WINDOW_CALL_SHOW = 1,
    FAKE_WINDOW_CALL_HIDE = 2,
    FAKE_WINDOW_CALL_SET_TOPMOST = 3,
    FAKE_WINDOW_CALL_PLACE_BEHIND = 4
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
static reach_host transition_host;
static reach_host transition_frame_host;
static reach_host registry_host;
static reach_host generic_frame_host;
static reach_host native_overlay_host;
static reach_host closing_stage_host;
static reach_host bar_conditions_host;
static reach_window_manipulation observed_manipulation;
static reach_point_i32 observed_pointer;
static reach_monitor_info primary_monitor = {1, {0, 0, 1000, 800}, {}, 96, 96, 1, 60};
static reach_rect_f32 observed_bounds;
static size_t render_count;
static size_t thumbnail_create_count;
static size_t thumbnail_place_count;
static size_t thumbnail_destroy_count;

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

static reach_result fake_thumbnail_set_target(reach_window_thumbnails *thumbnails,
                                              reach_window_id target)
{
    (void)thumbnails;
    return target != 0 ? REACH_OK : REACH_ERROR;
}

static reach_result fake_thumbnail_create(reach_window_thumbnails *thumbnails,
                                          reach_window_id source, reach_window_thumbnail_id *out_id)
{
    (void)thumbnails;
    if (source == 0 || out_id == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_id = ++thumbnail_create_count;
    return REACH_OK;
}

static reach_result fake_thumbnail_set_placement(reach_window_thumbnails *thumbnails,
                                                 reach_window_thumbnail_id id,
                                                 const reach_window_thumbnail_placement *placement)
{
    (void)thumbnails;
    if (id == REACH_WINDOW_THUMBNAIL_NONE || placement == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    thumbnail_place_count++;
    return REACH_OK;
}

static reach_result fake_thumbnail_destroy_all(reach_window_thumbnails *thumbnails)
{
    (void)thumbnails;
    thumbnail_destroy_count++;
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

static void test_registered_transition_completion(void)
{
    reach_host *host = &transition_host;
    reach_animation_track tracks[REACH_HOST_ANIMATION_COUNT] = {};
    reach_animation_manager_init(&host->animations, tracks, REACH_HOST_ANIMATION_COUNT);

    reach_host_surface_transition transition = {};
    transition.visible = 1;
    transition.y_track = 0;
    transition.opacity_track = 1;
    transition.scale_track = REACH_HOST_ANIMATION_COUNT;
    host->feature_runtimes[REACH_SURFACE_ID_SYSTEM_HUD].transition = &transition;

    reach_host_finish_surface_transitions(host);

    expect_true(!transition.visible,
                "a transition registered only in the feature runtime completes");
    expect_true(host->dirty.update_requested,
                "finishing a registered transition schedules reconciliation");
}

static void test_scaled_transition_keeps_native_envelope_stationary(void)
{
    reach_host *host = &transition_frame_host;
    reach_animation_track tracks[REACH_HOST_ANIMATION_COUNT] = {};
    reach_animation_manager_init(&host->animations, tracks, REACH_HOST_ANIMATION_COUNT);
    host->layout_dpi_scale = 1.25f;

    reach_host_surface_transition transition = {};
    transition.visible = 1;
    transition.y_track = 0;
    transition.opacity_track = 1;
    transition.scale_track = 2;
    transition.start_scale = REACH_HOST_LAUNCHER_TRANSITION_SCALE;

    reach_animation_manager_set(&host->animations, transition.scale_track, 1.04f);
    reach_animation_manager_set(&host->animations, transition.y_track, 8.0f);
    reach_host_surface_transition_frame offset_frame =
        reach_host_surface_transition_frame_compute_in_envelope(
            host, &transition, {710.0f, 900.0f, 500.0f, 72.0f}, {710.0f, 900.0f, 500.0f, 300.0f},
            {});

    reach_animation_manager_set(&host->animations, transition.y_track, 0.0f);
    reach_host_surface_transition_frame settled_frame =
        reach_host_surface_transition_frame_compute_in_envelope(
            host, &transition, {710.0f, 900.0f, 500.0f, 72.0f}, {710.0f, 900.0f, 500.0f, 300.0f},
            {});

    expect_true(
        reach_host_scalar_equal(offset_frame.window_bounds.y, settled_frame.window_bounds.y),
        "a scaled transition keeps its native window envelope stationary");
    expect_true(reach_host_scalar_equal(offset_frame.render_transform.offset_y -
                                            settled_frame.render_transform.offset_y,
                                        10.0f),
                "a scaled transition applies vertical motion in the render transform");
    expect_true(reach_host_scalar_equal(offset_frame.pointer_transform.offset_y -
                                            settled_frame.pointer_transform.offset_y,
                                        10.0f),
                "a scaled transition keeps pointer motion aligned with rendered content");
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
    expect_true(launcher->surface.scale_in_envelope &&
                    launcher->surface_ops->set_pointer_transform != nullptr,
                "Launcher declares its envelope transform contract");
    expect_true(host->feature_runtimes[REACH_SURFACE_ID_CONTEXT_MENU]
                        .definition->surface_ops->layout_anchor != nullptr,
                "Context Menu declares runtime-selected layout anchoring");
    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_STAGE].definition->surface_ops->native_overlay !=
            nullptr,
        "Stage declares its native overlay contract");
    expect_true(
        host->feature_runtimes[REACH_SURFACE_ID_DOCK].definition->resolve_anchor != nullptr &&
            host->feature_runtimes[REACH_SURFACE_ID_TOP_BAR].definition->resolve_anchor != nullptr,
        "dynamic anchor owners publish generic anchor resolvers");

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
                    reach_host_rect_equal(hud->resolved_bounds, state->layout.bounds),
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
    reach_stage_open_window window = {};
    window.window = 42;
    window.label = label;
    window.frame = {100.0f, 100.0f, 800.0f, 600.0f};
    reach_rect_f32 monitor = {0.0f, 0.0f, 1920.0f, 1080.0f};
    expect_true(
        reach_stage_open(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE),
                         monitor, 1.0f, &window, 1) == REACH_OK,
        "Stage opens for native-overlay frame testing");

    reach_feature_runtime *stage = &host->feature_runtimes[REACH_SURFACE_ID_STAGE];
    stage->transition = nullptr;
    stage->surface->window.window = reinterpret_cast<reach_platform_window *>(9);
    stage->surface->window.ops.set_bounds = fake_set_bounds;
    stage->surface->window.ops.native_id = fake_window_id;
    stage->surface->renderer.backend = reinterpret_cast<reach_render_backend *>(1);
    stage->surface->renderer.ops.begin_frame = fake_begin_frame;
    stage->surface->renderer.ops.end_frame = fake_end_frame;
    stage->surface->renderer.ops.execute = fake_execute;
    stage->surface->dirty_flags = 1;
    host->window_thumbnails.thumbnails = reinterpret_cast<reach_window_thumbnails *>(1);
    host->window_thumbnails.ops.set_target = fake_thumbnail_set_target;
    host->window_thumbnails.ops.create = fake_thumbnail_create;
    host->window_thumbnails.ops.set_placement = fake_thumbnail_set_placement;
    host->window_thumbnails.ops.destroy_all = fake_thumbnail_destroy_all;

    thumbnail_create_count = 0;
    thumbnail_place_count = 0;
    thumbnail_destroy_count = 0;
    reach_host_frame_context frame = {};
    frame.monitor_bounds = monitor;
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "generic frame renders a native-overlay surface");
    expect_true(thumbnail_create_count == 1 && thumbnail_place_count == 1,
                "generic frame registers and places the Stage thumbnail");

    reach_stage_force_close(reach_host_feature_capsule<reach_stage>(host, REACH_SURFACE_ID_STAGE));
    expect_true(reach_host_frame_registered_surface(host, stage, &frame) == REACH_OK,
                "generic frame handles native-overlay closure");
    expect_true(thumbnail_destroy_count == 1,
                "generic frame releases native overlays when the capsule closes");

    reach_host_destroy_registered_features(host);
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
    stage->definition->capsule_ops->tick(stage->capsule, 5.0, &tick);
    expect_true(!reach_host_surface_presented(stage),
                "a fully closed Stage stops forcing the bars shown");

    reach_host_destroy_registered_features(host);
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
    reach_host_sync_bar_layout_conditions(host);
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
    reach_host_sync_bar_layout_conditions(host);
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

int main(void)
{
    test_order_invalidation_rechains_without_replaying_visibility();
    test_app_band_surface_does_not_invalidate_topmost_order();
    test_window_manipulation_relevance_survives_unavailable_pointer();
    test_window_manipulation_tracks_pointer_monitor_membership();
    test_registered_transition_completion();
    test_scaled_transition_keeps_native_envelope_stationary();
    test_popup_pointer_coordinates_are_surface_local();
    test_registered_feature_lifecycle();
    test_registered_surface_frame_uses_declared_anchor();
    test_registered_surface_frame_syncs_native_overlay();
    test_forced_bars_hold_through_a_closing_stage();
    test_bar_conditions_sync_once_from_host_state();
    test_switcher_publishes_arranged_surface_geometry();

    if (failures != 0)
    {
        fprintf(stderr, "%d host layout test failure(s)\n", failures);
        return 1;
    }
    printf("host layout tests passed\n");
    return 0;
}
