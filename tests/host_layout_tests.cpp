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
static reach_window_manipulation observed_manipulation;
static reach_point_i32 observed_pointer;
static reach_monitor_info primary_monitor = {1, {0, 0, 1000, 800}, {}, 96, 96, 1, 60};

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
    reach_surface_runtime *surface = host->surface_descs[id].surface;
    surface->window.window = (reach_platform_window *)(uintptr_t)window_id;
    surface->window.ops.show = fake_show;
    surface->window.ops.hide = fake_hide;
    surface->window.ops.set_topmost = fake_set_topmost;
    surface->window.ops.place_behind = fake_place_behind;
    surface->window.ops.native_id = fake_native_id;

    reach_surface_desc *desc = &host->surface_descs[id];
    desc->id = id;
    desc->role = role;
    desc->surface = surface;
    desc->layer = layer;

    reach_layout_participant participant = 0;
    expect_true(reach_layout_register(&host->layout_manager, layer, &participant) == REACH_OK,
                "layout participant registers");
    host->surface_participants[id] = participant;
    host->layout_targets[participant].desc = desc;
}

static void initialize_host(reach_host *host)
{
    host->surface_descs[REACH_SURFACE_ID_TOP_BAR].surface = &host->top_bar;
    host->surface_descs[REACH_SURFACE_ID_DOCK].surface = &host->dock;
    host->surface_descs[REACH_SURFACE_ID_STAGE].surface = &host->stage;
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

int main(void)
{
    test_order_invalidation_rechains_without_replaying_visibility();
    test_app_band_surface_does_not_invalidate_topmost_order();
    test_window_manipulation_relevance_survives_unavailable_pointer();
    test_window_manipulation_tracks_pointer_monitor_membership();

    if (failures != 0)
    {
        fprintf(stderr, "%d host layout test failure(s)\n", failures);
        return 1;
    }
    printf("host layout tests passed\n");
    return 0;
}
