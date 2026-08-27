#include "host_internal.h"

static void reach_host_surface_launcher_close(reach_host *host)
{
    reach_host_close_launcher_without_focus_restore(host);
}

static void reach_host_surface_clipboard_close(reach_host *host)
{
    reach_host_set_clipboard_open(host, 0);
}

static void reach_host_surface_tray_close(reach_host *host)
{
    reach_host_set_tray_popup_open(host, 0);
}

static void reach_host_surface_quick_settings_close(reach_host *host)
{
    reach_host_set_quick_settings_open(host, 0);
}

static void reach_host_surface_battery_close(reach_host *host)
{
    reach_host_set_battery_open(host, 0);
}

static void reach_host_surface_context_menu_close(reach_host *host)
{
    reach_host_close_context_menu(host);
}

static void reach_host_surface_stage_close(reach_host *host)
{
    reach_host_close_stage(host);
}

static void reach_host_surface_switcher_close(reach_host *host)
{
    reach_switcher_force_close(host->switcher_capsule);
    reach_host_surface_transition_set(host, &host->switcher_transition, 0);
    host->switcher.dirty_flags = 1;
}

#define REACH_HOST_LAYER_DOCK_EDGE_REVEAL 120
#define REACH_HOST_LAYER_BAR_ACTIVE 130
#define REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL 140
#define REACH_HOST_LAYER_STAGE_EDGE_REVEAL 150

static void reach_host_init_surface_descriptors(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_surface_desc *descs = host->surface_descs;

    descs[REACH_SURFACE_ID_DOCK] = {REACH_SURFACE_ID_DOCK,
                                    REACH_SURFACE_CLASS_PERSISTENT,
                                    &host->dock,
                                    nullptr,
                                    nullptr,
                                    host->dock_capsule,
                                    reach_dock_capsule_ops(),
                                    REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_TOP_BAR] = {REACH_SURFACE_ID_TOP_BAR,
                                       REACH_SURFACE_CLASS_PERSISTENT,
                                       &host->top_bar,
                                       nullptr,
                                       nullptr,
                                       host->top_bar_capsule,
                                       reach_top_bar_capsule_ops(),
                                       REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_LAUNCHER] = {REACH_SURFACE_ID_LAUNCHER,
                                        REACH_SURFACE_CLASS_TRANSIENT,
                                        &host->launcher,
                                        &host->launcher_transition,
                                        reach_host_surface_launcher_close,
                                        host->launcher_capsule,
                                        reach_launcher_capsule_ops(),
                                        REACH_SURFACE_POINTER_RELAYOUT_REDRAWS |
                                            REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED};
    descs[REACH_SURFACE_ID_CLIPBOARD] = {REACH_SURFACE_ID_CLIPBOARD,
                                         REACH_SURFACE_CLASS_TRANSIENT,
                                         &host->clipboard_surface,
                                         &host->clipboard_transition,
                                         reach_host_surface_clipboard_close,
                                         host->clipboard_capsule,
                                         reach_clipboard_feature_capsule_ops(),
                                         REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_TRAY] = {REACH_SURFACE_ID_TRAY,
                                    REACH_SURFACE_CLASS_POPUP,
                                    &host->tray,
                                    &host->tray_transition,
                                    reach_host_surface_tray_close,
                                    host->top_bar_capsule,
                                    reach_top_bar_tray_capsule_ops(),
                                    REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED};
    descs[REACH_SURFACE_ID_QUICK_SETTINGS] = {REACH_SURFACE_ID_QUICK_SETTINGS,
                                              REACH_SURFACE_CLASS_POPUP,
                                              &host->quick_settings,
                                              &host->quick_settings_transition,
                                              reach_host_surface_quick_settings_close,
                                              host->quick_settings_capsule,
                                              reach_quick_settings_capsule_ops(),
                                              REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_BATTERY] = {
        REACH_SURFACE_ID_BATTERY,    REACH_SURFACE_CLASS_POPUP,        &host->battery,
        &host->battery_transition,   reach_host_surface_battery_close, host->battery_capsule,
        reach_battery_capsule_ops(), REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_SYSTEM_HUD] = {REACH_SURFACE_ID_SYSTEM_HUD,
                                          REACH_SURFACE_CLASS_PERSISTENT,
                                          &host->system_hud,
                                          nullptr,
                                          nullptr,
                                          host->system_hud_capsule,
                                          reach_system_hud_capsule_ops(),
                                          REACH_SURFACE_POINTER_SOURCE_GATED};
    descs[REACH_SURFACE_ID_CONTEXT_MENU] = {REACH_SURFACE_ID_CONTEXT_MENU,
                                            REACH_SURFACE_CLASS_POPUP,
                                            &host->context_menu,
                                            &host->context_menu_transition,
                                            reach_host_surface_context_menu_close,
                                            host->context_menu_capsule,
                                            reach_context_menu_capsule_ops(),
                                            REACH_SURFACE_POINTER_NONE};
    descs[REACH_SURFACE_ID_SWITCHER] = {
        REACH_SURFACE_ID_SWITCHER,    REACH_SURFACE_CLASS_OVERLAY,       &host->switcher,
        &host->switcher_transition,   reach_host_surface_switcher_close, host->switcher_capsule,
        reach_switcher_capsule_ops(), REACH_SURFACE_POINTER_NONE};

    descs[REACH_SURFACE_ID_STAGE] = {
        REACH_SURFACE_ID_STAGE,    REACH_SURFACE_CLASS_TRANSIENT,  &host->stage,
        &host->stage_transition,   reach_host_surface_stage_close, host->stage_capsule,
        reach_stage_capsule_ops(), REACH_SURFACE_POINTER_NONE};

    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        descs[index].layout_anchor = REACH_HOST_SURFACE_COUNT;
    }

    descs[REACH_SURFACE_ID_DOCK].shadow = REACH_SURFACE_SHADOW_BAR;
    descs[REACH_SURFACE_ID_TOP_BAR].shadow = REACH_SURFACE_SHADOW_BAR;
    descs[REACH_SURFACE_ID_LAUNCHER].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_CLIPBOARD].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_SWITCHER].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_TRAY].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_BATTERY].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].shadow = REACH_SURFACE_SHADOW_POPUP;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].shadow = REACH_SURFACE_SHADOW_POPUP;

    descs[REACH_SURFACE_ID_STAGE].layer = 50;
    descs[REACH_SURFACE_ID_LAUNCHER].layer = 100;
    descs[REACH_SURFACE_ID_DOCK].layer = 110;
    descs[REACH_SURFACE_ID_TOP_BAR].layer = 0;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].layer = 160;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].layer = 170;
    descs[REACH_SURFACE_ID_BATTERY].layer = 175;
    descs[REACH_SURFACE_ID_TRAY].layer = 180;
    descs[REACH_SURFACE_ID_CLIPBOARD].layer = 190;
    descs[REACH_SURFACE_ID_SWITCHER].layer = 200;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].layer = 220;

    descs[REACH_SURFACE_ID_CONTEXT_MENU].role = REACH_SURFACE_CONTEXT_MENU;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].pointer_priority = 10;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].apply_pointer_action =
        reach_host_apply_context_menu_pointer_action;
    descs[REACH_SURFACE_ID_CLIPBOARD].role = REACH_SURFACE_CLIPBOARD;
    descs[REACH_SURFACE_ID_CLIPBOARD].pointer_priority = 20;
    descs[REACH_SURFACE_ID_CLIPBOARD].apply_pointer_action =
        reach_host_apply_clipboard_pointer_action;
    descs[REACH_SURFACE_ID_LAUNCHER].role = REACH_SURFACE_LAUNCHER;
    descs[REACH_SURFACE_ID_LAUNCHER].pointer_priority = 30;
    descs[REACH_SURFACE_ID_LAUNCHER].apply_pointer_action =
        reach_host_apply_launcher_pointer_action;
    descs[REACH_SURFACE_ID_LAUNCHER].dismiss = reach_host_close_launcher;
    descs[REACH_SURFACE_ID_LAUNCHER].behavior_flags =
        REACH_SURFACE_BEHAVIOR_ACTIVATES | REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_TRAY].role = REACH_SURFACE_TRAY_MENU;
    descs[REACH_SURFACE_ID_TRAY].pointer_priority = 40;
    descs[REACH_SURFACE_ID_TRAY].apply_pointer_action = reach_host_apply_tray_pointer_action;
    descs[REACH_SURFACE_ID_BATTERY].role = REACH_SURFACE_BATTERY;
    descs[REACH_SURFACE_ID_BATTERY].pointer_priority = 55;
    descs[REACH_SURFACE_ID_BATTERY].apply_pointer_action = reach_host_apply_battery_pointer_action;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].role = REACH_SURFACE_QUICK_SETTINGS;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].pointer_priority = 50;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].apply_pointer_action =
        reach_host_apply_quick_settings_pointer_action;
    descs[REACH_SURFACE_ID_DOCK].edge_reveal = {1, REACH_HOST_LAYER_DOCK_EDGE_REVEAL, {}, nullptr};
    descs[REACH_SURFACE_ID_DOCK].bar_reveal = {reach_dock_reveal_ops(), 0, 0.0f};
    descs[REACH_SURFACE_ID_TOP_BAR].edge_reveal = {
        1, REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL, {}, nullptr};
    descs[REACH_SURFACE_ID_TOP_BAR].bar_reveal = {reach_top_bar_reveal_ops(),
                                                  REACH_HOST_LAYER_BAR_ACTIVE, 4.0f};
    descs[REACH_SURFACE_ID_TOP_BAR].role = REACH_SURFACE_TOP_BAR;
    descs[REACH_SURFACE_ID_TOP_BAR].pointer_priority = 80;
    descs[REACH_SURFACE_ID_TOP_BAR].apply_pointer_action = reach_host_apply_top_bar_pointer_action;
    descs[REACH_SURFACE_ID_DOCK].role = REACH_SURFACE_DOCK;
    descs[REACH_SURFACE_ID_DOCK].pointer_priority = 90;
    descs[REACH_SURFACE_ID_DOCK].apply_pointer_action = reach_host_apply_dock_pointer_action;
    descs[REACH_SURFACE_ID_SWITCHER].role = REACH_SURFACE_SWITCHER;
    descs[REACH_SURFACE_ID_SWITCHER].pointer_priority = 100;
    descs[REACH_SURFACE_ID_SWITCHER].behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_STAGE].role = REACH_SURFACE_STAGE;
    descs[REACH_SURFACE_ID_STAGE].pointer_priority = 60;
    descs[REACH_SURFACE_ID_STAGE].apply_pointer_action = reach_host_apply_stage_pointer_action;
    descs[REACH_SURFACE_ID_STAGE].dismiss = reach_host_close_stage;
    descs[REACH_SURFACE_ID_STAGE].bar_shown_while_open = 1;
    descs[REACH_SURFACE_ID_STAGE].behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].role = REACH_SURFACE_SYSTEM_HUD;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].pointer_priority = 0;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].behavior_flags = REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE;
    descs[REACH_SURFACE_ID_STAGE].edge_reveal = {1,
                                                 REACH_HOST_LAYER_STAGE_EDGE_REVEAL,
                                                 {REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT, 4.0f, 4.0f, 1},
                                                 reach_host_on_stage_edge_reveal};

    descs[REACH_SURFACE_ID_LAUNCHER].frame = reach_host_frame_launcher;
    descs[REACH_SURFACE_ID_LAUNCHER].frame_priority = 10;
    descs[REACH_SURFACE_ID_CLIPBOARD].frame_priority = 20;
    descs[REACH_SURFACE_ID_DOCK].frame = reach_host_frame_dock;
    descs[REACH_SURFACE_ID_DOCK].frame_priority = 30;
    descs[REACH_SURFACE_ID_TOP_BAR].frame = reach_host_frame_top_bar;
    descs[REACH_SURFACE_ID_TOP_BAR].frame_priority = 35;
    descs[REACH_SURFACE_ID_TRAY].frame = reach_host_frame_tray;
    descs[REACH_SURFACE_ID_TRAY].frame_priority = 40;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame = reach_host_frame_quick_settings;
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].frame_priority = 50;
    descs[REACH_SURFACE_ID_BATTERY].frame = reach_host_frame_battery;
    descs[REACH_SURFACE_ID_BATTERY].frame_priority = 55;
    descs[REACH_SURFACE_ID_SWITCHER].frame_priority = 60;
    descs[REACH_SURFACE_ID_STAGE].frame = reach_host_frame_stage;
    descs[REACH_SURFACE_ID_STAGE].frame_priority = 65;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].frame = reach_host_frame_context_menu;
    descs[REACH_SURFACE_ID_CONTEXT_MENU].frame_priority = 70;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].frame_priority = 80;

    descs[REACH_SURFACE_ID_LAUNCHER].toggle_events =
        reach_launcher_activation_events(&descs[REACH_SURFACE_ID_LAUNCHER].toggle_event_count);
    descs[REACH_SURFACE_ID_LAUNCHER].toggle = reach_host_toggle_launcher;
    descs[REACH_SURFACE_ID_CLIPBOARD].toggle_events =
        reach_clipboard_activation_events(&descs[REACH_SURFACE_ID_CLIPBOARD].toggle_event_count);
    descs[REACH_SURFACE_ID_CLIPBOARD].toggle = reach_host_toggle_clipboard;
    descs[REACH_SURFACE_ID_SWITCHER].routed_events =
        reach_switcher_routed_events(&descs[REACH_SURFACE_ID_SWITCHER].routed_event_count);
    descs[REACH_SURFACE_ID_SWITCHER].handle_routed = reach_host_handle_switcher_event;
}

static int32_t reach_system_hud_surface_arrange(void *capsule,
                                                const reach_feature_surface_context *ctx)
{
    reach_system_hud_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.dock_shown_bounds = ctx->anchor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    return reach_system_hud_arrange(static_cast<reach_system_hud *>(capsule), &arrange);
}

static reach_result reach_system_hud_surface_render(void *capsule,
                                                    const reach_feature_surface_context *ctx,
                                                    reach_render_command_buffer *out_commands)
{
    reach_system_hud_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_system_hud_append_render_commands(static_cast<const reach_system_hud *>(capsule),
                                                   &render, out_commands);
}

static const reach_feature_surface_ops reach_system_hud_surface_ops = {
    reach_system_hud_surface_arrange,
    reach_system_hud_surface_render,
};

static int32_t reach_switcher_surface_arrange(void *capsule,
                                              const reach_feature_surface_context *ctx)
{
    reach_switcher_arrange_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor_bounds = ctx->monitor_bounds;
    arrange.last_bounds = ctx->last_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    arrange.transition_visible = ctx->transition_visible;
    arrange.bounds_valid = ctx->bounds_valid;
    return reach_switcher_arrange(static_cast<reach_switcher *>(capsule), &arrange);
}

static reach_result reach_switcher_surface_render(void *capsule,
                                                  const reach_feature_surface_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    reach_switcher_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->render_bounds;
    render.dpi_scale = ctx->dpi_scale;
    render.icon_size_px = ctx->icon_size_px;
    return reach_switcher_append_render_commands(static_cast<reach_switcher *>(capsule), &render,
                                                 out_commands);
}

static const reach_feature_surface_ops reach_switcher_surface_ops = {
    reach_switcher_surface_arrange,
    reach_switcher_surface_render,
};

static int32_t reach_clipboard_surface_arrange(void *capsule,
                                               const reach_feature_surface_context *ctx)
{
    float border = reach_theme_border_thickness(ctx->theme, ctx->dpi_scale);
    return reach_clipboard_feature_relayout(static_cast<reach_clipboard_feature *>(capsule),
                                            ctx->monitor_bounds, ctx->anchor_bounds, ctx->dpi_scale,
                                            border, nullptr);
}

static reach_result reach_clipboard_surface_render(void *capsule,
                                                   const reach_feature_surface_context *ctx,
                                                   reach_render_command_buffer *out_commands)
{
    return reach_clipboard_append_render_commands(static_cast<reach_clipboard_feature *>(capsule),
                                                  ctx->theme, ctx->dpi_scale, out_commands);
}

static const reach_feature_surface_ops reach_clipboard_surface_ops = {
    reach_clipboard_surface_arrange,
    reach_clipboard_surface_render,
};

template <typename Feature, reach_result (*Create)(Feature **)>
static reach_result reach_feature_create(void **out_capsule)
{
    if (out_capsule == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    Feature *capsule = nullptr;
    reach_result result = Create(&capsule);
    *out_capsule = capsule;
    return result;
}

template <typename Feature, void (*Destroy)(Feature *)>
static void reach_feature_destroy(void *capsule)
{
    Destroy(static_cast<Feature *>(capsule));
}

static void reach_host_bind_feature_capsules(reach_host *host)
{
    reach_surface_desc *descs = host->surface_descs;
    descs[REACH_SURFACE_ID_TRAY].capsule = descs[REACH_SURFACE_ID_TOP_BAR].capsule;

    host->dock_capsule = static_cast<reach_dock *>(descs[REACH_SURFACE_ID_DOCK].capsule);
    host->top_bar_capsule = static_cast<reach_top_bar *>(descs[REACH_SURFACE_ID_TOP_BAR].capsule);
    host->launcher_capsule =
        static_cast<reach_launcher *>(descs[REACH_SURFACE_ID_LAUNCHER].capsule);
    host->clipboard_capsule =
        static_cast<reach_clipboard_feature *>(descs[REACH_SURFACE_ID_CLIPBOARD].capsule);
    host->quick_settings_capsule =
        static_cast<reach_quick_settings *>(descs[REACH_SURFACE_ID_QUICK_SETTINGS].capsule);
    host->battery_capsule = static_cast<reach_battery *>(descs[REACH_SURFACE_ID_BATTERY].capsule);
    host->system_hud_capsule =
        static_cast<reach_system_hud *>(descs[REACH_SURFACE_ID_SYSTEM_HUD].capsule);
    host->context_menu_capsule =
        static_cast<reach_context_menu *>(descs[REACH_SURFACE_ID_CONTEXT_MENU].capsule);
    host->switcher_capsule =
        static_cast<reach_switcher *>(descs[REACH_SURFACE_ID_SWITCHER].capsule);
    host->stage_capsule = static_cast<reach_stage *>(descs[REACH_SURFACE_ID_STAGE].capsule);
}

void reach_host_init_feature_registry(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_init_surface_descriptors(host);
    reach_surface_desc *descs = host->surface_descs;
    descs[REACH_SURFACE_ID_DOCK].factory = {reach_feature_create<reach_dock, reach_dock_create>,
                                            reach_feature_destroy<reach_dock, reach_dock_destroy>};
    descs[REACH_SURFACE_ID_TOP_BAR].factory = {
        reach_feature_create<reach_top_bar, reach_top_bar_create>,
        reach_feature_destroy<reach_top_bar, reach_top_bar_destroy>};
    descs[REACH_SURFACE_ID_LAUNCHER].factory = {
        reach_feature_create<reach_launcher, reach_launcher_create>,
        reach_feature_destroy<reach_launcher, reach_launcher_destroy>};
    descs[REACH_SURFACE_ID_CLIPBOARD].factory = {
        reach_feature_create<reach_clipboard_feature, reach_clipboard_feature_create>,
        reach_feature_destroy<reach_clipboard_feature, reach_clipboard_feature_destroy>};
    descs[REACH_SURFACE_ID_QUICK_SETTINGS].factory = {
        reach_feature_create<reach_quick_settings, reach_quick_settings_create>,
        reach_feature_destroy<reach_quick_settings, reach_quick_settings_destroy>};
    descs[REACH_SURFACE_ID_BATTERY].factory = {
        reach_feature_create<reach_battery, reach_battery_create>,
        reach_feature_destroy<reach_battery, reach_battery_destroy>};
    descs[REACH_SURFACE_ID_SYSTEM_HUD].factory = {
        reach_feature_create<reach_system_hud, reach_system_hud_create>,
        reach_feature_destroy<reach_system_hud, reach_system_hud_destroy>};
    descs[REACH_SURFACE_ID_CONTEXT_MENU].factory = {
        reach_feature_create<reach_context_menu, reach_context_menu_create>,
        reach_feature_destroy<reach_context_menu, reach_context_menu_destroy>};
    descs[REACH_SURFACE_ID_SWITCHER].factory = {
        reach_feature_create<reach_switcher, reach_switcher_create>,
        reach_feature_destroy<reach_switcher, reach_switcher_destroy>};
    descs[REACH_SURFACE_ID_STAGE].factory = {
        reach_feature_create<reach_stage, reach_stage_create>,
        reach_feature_destroy<reach_stage, reach_stage_destroy>};
    descs[REACH_SURFACE_ID_SYSTEM_HUD].surface_ops = &reach_system_hud_surface_ops;
    descs[REACH_SURFACE_ID_SYSTEM_HUD].layout_anchor = REACH_SURFACE_ID_DOCK;
    descs[REACH_SURFACE_ID_SWITCHER].surface_ops = &reach_switcher_surface_ops;
    descs[REACH_SURFACE_ID_CLIPBOARD].surface_ops = &reach_clipboard_surface_ops;
    descs[REACH_SURFACE_ID_CLIPBOARD].layout_anchor = REACH_SURFACE_ID_LAUNCHER;
}

reach_result reach_host_create_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_result result = REACH_OK;
    for (size_t index = 0; index < REACH_HOST_SURFACE_COUNT; ++index)
    {
        reach_surface_desc *desc = &host->surface_descs[index];
        if (desc->factory.create == nullptr)
        {
            continue;
        }
        reach_result create_result = desc->factory.create(&desc->capsule);
        if (create_result != REACH_OK)
        {
            result = create_result;
        }
    }
    reach_host_bind_feature_capsules(host);
    return result;
}

void reach_host_destroy_registered_features(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    for (size_t index = REACH_HOST_SURFACE_COUNT; index > 0; --index)
    {
        reach_surface_desc *desc = &host->surface_descs[index - 1];
        if (desc->factory.destroy != nullptr && desc->capsule != nullptr)
        {
            desc->factory.destroy(desc->capsule);
        }
        desc->capsule = nullptr;
    }
    reach_host_bind_feature_capsules(host);
}
