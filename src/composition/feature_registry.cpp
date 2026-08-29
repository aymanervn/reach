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
    reach_switcher_force_close(
        reach_host_feature_capsule<reach_switcher>(host, REACH_SURFACE_ID_SWITCHER));
    reach_host_surface_transition_set(host, &host->switcher_transition, 0);
    host->switcher.dirty_flags = 1;
}

#define REACH_HOST_LAYER_DOCK_EDGE_REVEAL 120
#define REACH_HOST_LAYER_BAR_ACTIVE 130
#define REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL 140
#define REACH_HOST_LAYER_STAGE_EDGE_REVEAL 150

static void reach_host_define_feature(reach_host *host, reach_surface_id id,
                                      reach_surface_class cls, reach_surface_runtime *surface,
                                      reach_host_surface_transition *transition,
                                      void (*force_close)(reach_host *),
                                      const reach_feature_capsule_ops *capsule_ops,
                                      uint32_t pointer_flags)
{
    reach_feature_definition *definition = &host->feature_definitions[id];
    *definition = {};
    definition->id = id;
    definition->capsule_ops = capsule_ops;
    definition->surface.cls = cls;
    definition->surface.pointer_flags = pointer_flags;
    definition->surface.has_transition = transition != nullptr;
    definition->layout.anchor = REACH_HOST_SURFACE_COUNT;
    definition->force_close = force_close;

    reach_feature_runtime *runtime = &host->feature_runtimes[id];
    *runtime = {};
    runtime->surface = surface;
    runtime->transition = transition;
    runtime->definition = definition;
}

static void reach_host_init_feature_definitions(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_define_feature(host, REACH_SURFACE_ID_DOCK, REACH_SURFACE_CLASS_PERSISTENT,
                              &host->dock, nullptr, nullptr, reach_dock_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED |
                                  REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE);
    reach_host_define_feature(host, REACH_SURFACE_ID_TOP_BAR, REACH_SURFACE_CLASS_PERSISTENT,
                              &host->top_bar, nullptr, nullptr, reach_top_bar_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_LAUNCHER, REACH_SURFACE_CLASS_TRANSIENT, &host->launcher,
        &host->launcher_transition, reach_host_surface_launcher_close, reach_launcher_capsule_ops(),
        REACH_SURFACE_POINTER_RELAYOUT_REDRAWS | REACH_SURFACE_POINTER_DOWN_CLOSES_ON_UNHANDLED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_CLIPBOARD, REACH_SURFACE_CLASS_TRANSIENT, &host->clipboard_surface,
        &host->clipboard_transition, reach_host_surface_clipboard_close,
        reach_clipboard_feature_capsule_ops(), REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(host, REACH_SURFACE_ID_TRAY, REACH_SURFACE_CLASS_POPUP, &host->tray,
                              &host->tray_transition, reach_host_surface_tray_close,
                              reach_top_bar_tray_capsule_ops(),
                              REACH_SURFACE_POINTER_DOWN_APPLIES_UNHANDLED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_QUICK_SETTINGS, REACH_SURFACE_CLASS_POPUP, &host->quick_settings,
        &host->quick_settings_transition, reach_host_surface_quick_settings_close,
        reach_quick_settings_capsule_ops(),
        REACH_SURFACE_POINTER_CAPTURE_CONSUMES_RELEASE | REACH_SURFACE_POINTER_CAPTURE_OWNS_MOVE);
    reach_host_define_feature(host, REACH_SURFACE_ID_BATTERY, REACH_SURFACE_CLASS_POPUP,
                              &host->battery, &host->battery_transition,
                              reach_host_surface_battery_close, reach_battery_capsule_ops(),
                              REACH_SURFACE_POINTER_NONE);
    reach_host_define_feature(host, REACH_SURFACE_ID_SYSTEM_HUD, REACH_SURFACE_CLASS_PERSISTENT,
                              &host->system_hud, nullptr, nullptr, reach_system_hud_capsule_ops(),
                              REACH_SURFACE_POINTER_SOURCE_GATED);
    reach_host_define_feature(
        host, REACH_SURFACE_ID_CONTEXT_MENU, REACH_SURFACE_CLASS_POPUP, &host->context_menu,
        &host->context_menu_transition, reach_host_surface_context_menu_close,
        reach_context_menu_capsule_ops(), REACH_SURFACE_POINTER_EXCLUSIVE_WHILE_OPEN);
    reach_host_define_feature(host, REACH_SURFACE_ID_SWITCHER, REACH_SURFACE_CLASS_OVERLAY,
                              &host->switcher, &host->switcher_transition,
                              reach_host_surface_switcher_close, reach_switcher_capsule_ops(),
                              REACH_SURFACE_POINTER_NONE);
    reach_host_define_feature(host, REACH_SURFACE_ID_STAGE, REACH_SURFACE_CLASS_TRANSIENT,
                              &host->stage, &host->stage_transition, reach_host_surface_stage_close,
                              reach_stage_capsule_ops(), REACH_SURFACE_POINTER_NONE);

    reach_feature_definition *definitions = host->feature_definitions;

    definitions[REACH_SURFACE_ID_DOCK].surface.shadow = REACH_SURFACE_SHADOW_BAR;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.shadow = REACH_SURFACE_SHADOW_BAR;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_TRAY].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_BATTERY].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.shadow = REACH_SURFACE_SHADOW_POPUP;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.shadow = REACH_SURFACE_SHADOW_POPUP;

    definitions[REACH_SURFACE_ID_STAGE].surface.layer = 50;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.layer = 100;
    definitions[REACH_SURFACE_ID_DOCK].surface.layer = 110;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.layer = 0;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.layer = 160;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.layer = 170;
    definitions[REACH_SURFACE_ID_BATTERY].surface.layer = 175;
    definitions[REACH_SURFACE_ID_TRAY].surface.layer = 180;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.layer = 190;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.layer = 200;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.layer = 220;

    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.role = REACH_SURFACE_CONTEXT_MENU;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface.pointer_priority = 10;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.role = REACH_SURFACE_CLIPBOARD;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface.pointer_priority = 20;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.role = REACH_SURFACE_LAUNCHER;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.pointer_priority = 30;
    definitions[REACH_SURFACE_ID_LAUNCHER].dismiss = reach_host_close_launcher;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_ACTIVATES | REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface.scale_in_envelope = 1;
    definitions[REACH_SURFACE_ID_TRAY].surface.role = REACH_SURFACE_TRAY_MENU;
    definitions[REACH_SURFACE_ID_TRAY].surface.pointer_priority = 40;
    definitions[REACH_SURFACE_ID_BATTERY].surface.role = REACH_SURFACE_BATTERY;
    definitions[REACH_SURFACE_ID_BATTERY].surface.pointer_priority = 55;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.role = REACH_SURFACE_QUICK_SETTINGS;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.pointer_priority = 50;
    definitions[REACH_SURFACE_ID_DOCK].surface.edge_reveal = {
        1, REACH_HOST_LAYER_DOCK_EDGE_REVEAL, {}, nullptr};
    definitions[REACH_SURFACE_ID_DOCK].surface.bar_reveal = {reach_dock_reveal_ops(), 0, 0.0f};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.edge_reveal = {
        1, REACH_HOST_LAYER_TOP_BAR_EDGE_REVEAL, {}, nullptr};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.bar_reveal = {reach_top_bar_reveal_ops(),
                                                                REACH_HOST_LAYER_BAR_ACTIVE, 4.0f};
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.role = REACH_SURFACE_TOP_BAR;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface.pointer_priority = 80;
    definitions[REACH_SURFACE_ID_DOCK].surface.role = REACH_SURFACE_DOCK;
    definitions[REACH_SURFACE_ID_DOCK].surface.pointer_priority = 90;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.role = REACH_SURFACE_SWITCHER;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.pointer_priority = 100;
    definitions[REACH_SURFACE_ID_SWITCHER].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_STAGE].surface.role = REACH_SURFACE_STAGE;
    definitions[REACH_SURFACE_ID_STAGE].surface.pointer_priority = 60;
    definitions[REACH_SURFACE_ID_STAGE].dismiss = reach_host_close_stage;
    definitions[REACH_SURFACE_ID_STAGE].surface.bar_shown_while_open = 1;
    definitions[REACH_SURFACE_ID_STAGE].surface.behavior_flags = REACH_SURFACE_BEHAVIOR_EXCLUSIVE;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.role = REACH_SURFACE_SYSTEM_HUD;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.pointer_priority = 0;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface.behavior_flags =
        REACH_SURFACE_BEHAVIOR_GAME_MODE_VISIBLE;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.anchor_slot =
        REACH_TOP_BAR_CONTROL_QUICK_SETTINGS;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface.popup_chrome = 1;
    definitions[REACH_SURFACE_ID_BATTERY].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_BATTERY].layout.anchor_slot = REACH_TOP_BAR_CONTROL_BATTERY;
    definitions[REACH_SURFACE_ID_BATTERY].surface.popup_chrome = 1;
    definitions[REACH_SURFACE_ID_TRAY].layout.anchor = REACH_SURFACE_ID_TOP_BAR;
    definitions[REACH_SURFACE_ID_TRAY].layout.anchor_slot = REACH_TOP_BAR_CONTROL_TRAY;
    definitions[REACH_SURFACE_ID_STAGE].surface.edge_reveal = {
        1,
        REACH_HOST_LAYER_STAGE_EDGE_REVEAL,
        {REACH_EDGE_REVEAL_ANCHOR_TOP_LEFT, 4.0f, 4.0f, 1},
        reach_host_on_stage_edge_reveal};

    definitions[REACH_SURFACE_ID_LAUNCHER].layout.priority = 10;
    definitions[REACH_SURFACE_ID_CLIPBOARD].layout.priority = 20;
    definitions[REACH_SURFACE_ID_DOCK].layout.priority = 30;
    definitions[REACH_SURFACE_ID_TOP_BAR].layout.priority = 35;
    definitions[REACH_SURFACE_ID_TRAY].layout.priority = 40;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].layout.priority = 50;
    definitions[REACH_SURFACE_ID_BATTERY].layout.priority = 55;
    definitions[REACH_SURFACE_ID_SWITCHER].layout.priority = 60;
    definitions[REACH_SURFACE_ID_STAGE].layout.priority = 65;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].layout.priority = 70;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].layout.priority = 80;

    definitions[REACH_SURFACE_ID_LAUNCHER].toggle_events = reach_launcher_activation_events(
        &definitions[REACH_SURFACE_ID_LAUNCHER].toggle_event_count);
    definitions[REACH_SURFACE_ID_LAUNCHER].toggle = reach_host_toggle_launcher;
    definitions[REACH_SURFACE_ID_CLIPBOARD].toggle_events = reach_clipboard_activation_events(
        &definitions[REACH_SURFACE_ID_CLIPBOARD].toggle_event_count);
    definitions[REACH_SURFACE_ID_CLIPBOARD].toggle = reach_host_toggle_clipboard;
    definitions[REACH_SURFACE_ID_SWITCHER].routed_events =
        reach_switcher_routed_events(&definitions[REACH_SURFACE_ID_SWITCHER].routed_event_count);
    definitions[REACH_SURFACE_ID_SWITCHER].handle_routed = reach_host_handle_switcher_event;
}

static int32_t reach_prearranged_surface_arrange(void *capsule,
                                                 const reach_feature_surface_context *ctx)
{
    (void)capsule;
    (void)ctx;
    return 0;
}

static reach_result reach_dock_surface_render(void *capsule,
                                              const reach_feature_surface_context *ctx,
                                              reach_render_command_buffer *out_commands)
{
    reach_dock_surface_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->render_bounds;
    render.icon_size_px = ctx->icon_size_px;
    render.dpi_scale = ctx->dpi_scale;
    return reach_dock_append_surface_render_commands(static_cast<reach_dock *>(capsule), &render,
                                                     out_commands);
}

static const reach_feature_surface_ops reach_dock_surface_ops = {
    reach_prearranged_surface_arrange,
    reach_dock_surface_render,
};

static int32_t reach_dock_resolve_anchor(const void *capsule, uint32_t slot, size_t index,
                                         reach_feature_anchor *out)
{
    if (slot != REACH_DOCK_CONTROL_ITEM || out == nullptr)
    {
        return 0;
    }
    reach_rect_f32 button = {};
    float bar_edge_y = 0.0f;
    if (!reach_dock_item_anchor(static_cast<const reach_dock *>(capsule), index, &button,
                                &bar_edge_y))
    {
        return 0;
    }
    out->button = button;
    out->bar_edge_y = bar_edge_y;
    out->direction = REACH_POPUP_DROP_UP;
    return 1;
}

static int32_t reach_top_bar_surface_arrange(void *capsule,
                                             const reach_feature_surface_context *ctx)
{
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_rect_f32 before = reach_top_bar_state_ptr(top_bar)->layout.bounds;
    reach_top_bar_build_context build = {};
    build.theme = ctx->theme;
    build.monitor_bounds = ctx->monitor_bounds;
    build.dpi_scale = ctx->dpi_scale;
    build.text_measure = ctx->text_measure;
    reach_top_bar_build_layout(top_bar, &build);
    return !reach_host_rect_equal(before, reach_top_bar_state_ptr(top_bar)->layout.bounds);
}

static reach_result reach_top_bar_surface_render(void *capsule,
                                                 const reach_feature_surface_context *ctx,
                                                 reach_render_command_buffer *out_commands)
{
    reach_top_bar_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    render.icon_size_px = ctx->icon_size_px;
    return reach_top_bar_append_render_commands(static_cast<reach_top_bar *>(capsule), &render,
                                                out_commands);
}

static const reach_feature_surface_ops reach_top_bar_surface_ops = {
    reach_top_bar_surface_arrange,
    reach_top_bar_surface_render,
};

static reach_result reach_launcher_surface_render(void *capsule,
                                                  const reach_feature_surface_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    return reach_launcher_append_surface_render_commands(static_cast<reach_launcher *>(capsule),
                                                         ctx->theme, ctx->dpi_scale, out_commands);
}

static void reach_launcher_surface_set_pointer_transform(void *capsule,
                                                         reach_transform_f32 transform)
{
    reach_launcher_set_pointer_transform(static_cast<reach_launcher *>(capsule), transform);
}

static const reach_feature_surface_ops reach_launcher_surface_ops = {
    reach_prearranged_surface_arrange,
    reach_launcher_surface_render,
    nullptr,
    reach_launcher_surface_set_pointer_transform,
};

static int32_t reach_context_menu_surface_layout_anchor(const void *capsule,
                                                        reach_feature_layout_anchor *out)
{
    const reach_context_menu_state *state =
        reach_context_menu_state_ptr(static_cast<const reach_context_menu *>(capsule));
    if (state == nullptr || out == nullptr || !state->open || !state->anchored)
    {
        return 0;
    }
    out->surface = state->power_open ? REACH_SURFACE_ID_TOP_BAR : REACH_SURFACE_ID_DOCK;
    out->slot = state->power_open ? REACH_TOP_BAR_CONTROL_POWER : REACH_DOCK_CONTROL_ITEM;
    out->index = state->target_index;
    return 1;
}

static int32_t reach_context_menu_surface_arrange(void *capsule,
                                                  const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_context_menu *menu = static_cast<reach_context_menu *>(capsule);
    reach_rect_f32 before = reach_context_menu_state_ptr(menu)->bounds;
    reach_context_menu_open_context arrange = {};
    arrange.theme = ctx->theme;
    arrange.monitor = ctx->monitor_bounds;
    arrange.dpi_scale = ctx->dpi_scale;
    arrange.anchor_button = ctx->anchor_button;
    arrange.bar_edge_y = ctx->anchor_bar_edge_y;
    arrange.drop_direction = ctx->anchor_direction;
    arrange.anchored = 1;
    arrange.text_measure = ctx->text_measure;
    reach_context_menu_reanchor(menu, &arrange);
    return !reach_host_rect_equal(before, reach_context_menu_state_ptr(menu)->bounds);
}

static reach_result reach_context_menu_surface_render(void *capsule,
                                                      const reach_feature_surface_context *ctx,
                                                      reach_render_command_buffer *out_commands)
{
    reach_context_menu_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_context_menu_append_render_commands(static_cast<reach_context_menu *>(capsule),
                                                     &render, out_commands);
}

static const reach_feature_surface_ops reach_context_menu_surface_ops = {
    reach_context_menu_surface_arrange,
    reach_context_menu_surface_render,
    reach_context_menu_surface_layout_anchor,
};

static reach_result reach_stage_surface_render(void *capsule,
                                               const reach_feature_surface_context *ctx,
                                               reach_render_command_buffer *out_commands)
{
    reach_stage_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.dpi_scale = ctx->dpi_scale;
    return reach_stage_append_render_commands(static_cast<reach_stage *>(capsule), &render,
                                              out_commands);
}

static size_t reach_stage_native_overlay_generation(const void *capsule)
{
    return reach_stage_tile_generation(static_cast<const reach_stage *>(capsule));
}

static size_t reach_stage_native_overlay_count(const void *capsule)
{
    return reach_stage_thumbnail_count(static_cast<const reach_stage *>(capsule));
}

static reach_result reach_stage_native_overlay_item(const void *capsule, size_t index,
                                                    reach_feature_native_overlay_item *out)
{
    if (out == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_stage_thumbnail_placement placement = {};
    reach_result result =
        reach_stage_thumbnail_at(static_cast<const reach_stage *>(capsule), index, &placement);
    if (result != REACH_OK)
    {
        return result;
    }
    out->source = placement.window;
    out->placement.destination = placement.destination;
    out->placement.source_screen = placement.source_screen;
    out->placement.opacity = placement.opacity;
    out->placement.visible = placement.visible;
    out->placement.source_screen_valid = placement.source_screen_valid;
    return REACH_OK;
}

static const reach_feature_native_overlay_ops reach_stage_native_overlay_ops = {
    reach_stage_native_overlay_generation,
    reach_stage_native_overlay_count,
    reach_stage_native_overlay_item,
};

static const reach_feature_surface_ops reach_stage_surface_ops = {
    reach_prearranged_surface_arrange, reach_stage_surface_render, nullptr, nullptr,
    &reach_stage_native_overlay_ops,
};

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

static int32_t reach_top_bar_resolve_anchor(const void *capsule, uint32_t slot, size_t index,
                                            reach_feature_anchor *out)
{
    (void)index;
    const reach_top_bar *top_bar = static_cast<const reach_top_bar *>(capsule);
    const reach_top_bar_state *state = reach_top_bar_state_ptr(top_bar);
    if (state == nullptr || out == nullptr)
    {
        return 0;
    }

    reach_rect_f32 button = {};
    if (slot == REACH_TOP_BAR_CONTROL_TRAY)
    {
        button = state->layout.tray_overflow_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_QUICK_SETTINGS)
    {
        button = state->layout.quick_settings_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_BATTERY)
    {
        button = state->layout.battery_button;
    }
    else if (slot == REACH_TOP_BAR_CONTROL_POWER)
    {
        button = state->layout.power_button;
    }
    else
    {
        return 0;
    }

    out->button = reach_top_bar_rect_to_screen(&state->layout, button);
    out->bar_edge_y = state->layout.bounds.y + state->layout.bounds.height;
    out->bar_height = state->layout.bounds.height;
    out->direction = REACH_POPUP_DROP_DOWN;
    return 1;
}

static int32_t reach_quick_settings_surface_arrange(void *capsule,
                                                    const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_quick_settings *quick_settings = static_cast<reach_quick_settings *>(capsule);
    reach_rect_f32 before = reach_quick_settings_state_ptr(quick_settings)->bounds;
    reach_quick_settings_layout_context layout = {};
    layout.theme = ctx->theme;
    layout.dpi_scale = ctx->dpi_scale;
    layout.anchor_button = ctx->anchor_button;
    layout.monitor = ctx->monitor_bounds;
    layout.bar_edge_y = ctx->anchor_bar_edge_y;
    layout.drop_direction = ctx->anchor_direction;
    reach_quick_settings_refresh_layout(quick_settings, &layout);
    int32_t animation_changed = reach_quick_settings_update_open_animation(quick_settings, &layout);
    return animation_changed ||
           !reach_host_rect_equal(before, reach_quick_settings_state_ptr(quick_settings)->bounds);
}

static reach_result reach_quick_settings_surface_render(void *capsule,
                                                        const reach_feature_surface_context *ctx,
                                                        reach_render_command_buffer *out_commands)
{
    return reach_quick_settings_append_render_commands(static_cast<reach_quick_settings *>(capsule),
                                                       ctx->theme, ctx->dpi_scale, out_commands);
}

static const reach_feature_surface_ops reach_quick_settings_surface_ops = {
    reach_quick_settings_surface_arrange,
    reach_quick_settings_surface_render,
};

static int32_t reach_battery_surface_arrange(void *capsule,
                                             const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_battery *battery = static_cast<reach_battery *>(capsule);
    reach_rect_f32 before = reach_battery_state_ptr(battery)->bounds;
    reach_battery_open_context layout = {};
    layout.theme = ctx->theme;
    layout.monitor = ctx->monitor_bounds;
    layout.anchor_button = ctx->anchor_button;
    layout.bar_edge_y = ctx->anchor_bar_edge_y;
    layout.dpi_scale = ctx->dpi_scale;
    layout.drop_direction = ctx->anchor_direction;
    reach_battery_relayout(battery, &layout);
    return !reach_host_rect_equal(before, reach_battery_state_ptr(battery)->bounds);
}

static reach_result reach_battery_surface_render(void *capsule,
                                                 const reach_feature_surface_context *ctx,
                                                 reach_render_command_buffer *out_commands)
{
    reach_battery_render_context render = {};
    render.theme = ctx->theme;
    render.dpi_scale = ctx->dpi_scale;
    return reach_battery_append_render_commands(static_cast<const reach_battery *>(capsule),
                                                &render, out_commands);
}

static const reach_feature_surface_ops reach_battery_surface_ops = {
    reach_battery_surface_arrange,
    reach_battery_surface_render,
};

static int32_t reach_tray_surface_arrange(void *capsule, const reach_feature_surface_context *ctx)
{
    if (!ctx->anchor_valid)
    {
        return 0;
    }
    reach_top_bar *top_bar = static_cast<reach_top_bar *>(capsule);
    reach_feature_surface_geometry before = {};
    reach_top_bar_tray_capsule_ops()->surface_geometry(top_bar, &before);
    reach_popup_anchor anchor = {};
    anchor.button = ctx->anchor_button;
    anchor.monitor = ctx->monitor_bounds;
    anchor.bar_edge_y = ctx->anchor_bar_edge_y;
    anchor.bar_height = ctx->anchor_bar_height;
    anchor.direction = ctx->anchor_direction;
    reach_rect_f32 bounds = {};
    reach_top_bar_layout_tray_popup(top_bar, ctx->theme, &anchor, ctx->dpi_scale, &bounds);
    return !reach_host_rect_equal(before.visible_bounds, bounds);
}

static reach_result reach_tray_surface_render(void *capsule,
                                              const reach_feature_surface_context *ctx,
                                              reach_render_command_buffer *out_commands)
{
    reach_top_bar_tray_render_context render = {};
    render.theme = ctx->theme;
    render.bounds = ctx->visible_bounds;
    render.dpi_scale = ctx->dpi_scale;
    return reach_top_bar_append_tray_render_commands(static_cast<reach_top_bar *>(capsule), &render,
                                                     out_commands);
}

static const reach_feature_surface_ops reach_tray_surface_ops = {
    reach_tray_surface_arrange,
    reach_tray_surface_render,
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

static void reach_host_bind_shared_feature_capsules(reach_host *host)
{
    reach_feature_runtime *runtimes = host->feature_runtimes;
    runtimes[REACH_SURFACE_ID_TRAY].capsule = runtimes[REACH_SURFACE_ID_TOP_BAR].capsule;
}

void reach_host_init_feature_registry(reach_host *host)
{
    if (host == nullptr)
    {
        return;
    }

    reach_host_init_feature_definitions(host);
    reach_feature_definition *definitions = host->feature_definitions;
    definitions[REACH_SURFACE_ID_DOCK].factory = {
        reach_feature_create<reach_dock, reach_dock_create>,
        reach_feature_destroy<reach_dock, reach_dock_destroy>};
    definitions[REACH_SURFACE_ID_TOP_BAR].factory = {
        reach_feature_create<reach_top_bar, reach_top_bar_create>,
        reach_feature_destroy<reach_top_bar, reach_top_bar_destroy>};
    definitions[REACH_SURFACE_ID_LAUNCHER].factory = {
        reach_feature_create<reach_launcher, reach_launcher_create>,
        reach_feature_destroy<reach_launcher, reach_launcher_destroy>};
    definitions[REACH_SURFACE_ID_CLIPBOARD].factory = {
        reach_feature_create<reach_clipboard_feature, reach_clipboard_feature_create>,
        reach_feature_destroy<reach_clipboard_feature, reach_clipboard_feature_destroy>};
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].factory = {
        reach_feature_create<reach_quick_settings, reach_quick_settings_create>,
        reach_feature_destroy<reach_quick_settings, reach_quick_settings_destroy>};
    definitions[REACH_SURFACE_ID_BATTERY].factory = {
        reach_feature_create<reach_battery, reach_battery_create>,
        reach_feature_destroy<reach_battery, reach_battery_destroy>};
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].factory = {
        reach_feature_create<reach_system_hud, reach_system_hud_create>,
        reach_feature_destroy<reach_system_hud, reach_system_hud_destroy>};
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].factory = {
        reach_feature_create<reach_context_menu, reach_context_menu_create>,
        reach_feature_destroy<reach_context_menu, reach_context_menu_destroy>};
    definitions[REACH_SURFACE_ID_SWITCHER].factory = {
        reach_feature_create<reach_switcher, reach_switcher_create>,
        reach_feature_destroy<reach_switcher, reach_switcher_destroy>};
    definitions[REACH_SURFACE_ID_STAGE].factory = {
        reach_feature_create<reach_stage, reach_stage_create>,
        reach_feature_destroy<reach_stage, reach_stage_destroy>};
    definitions[REACH_SURFACE_ID_DOCK].surface_ops = &reach_dock_surface_ops;
    definitions[REACH_SURFACE_ID_TOP_BAR].surface_ops = &reach_top_bar_surface_ops;
    definitions[REACH_SURFACE_ID_LAUNCHER].surface_ops = &reach_launcher_surface_ops;
    definitions[REACH_SURFACE_ID_CONTEXT_MENU].surface_ops = &reach_context_menu_surface_ops;
    definitions[REACH_SURFACE_ID_STAGE].surface_ops = &reach_stage_surface_ops;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].surface_ops = &reach_system_hud_surface_ops;
    definitions[REACH_SURFACE_ID_SYSTEM_HUD].layout.anchor = REACH_SURFACE_ID_DOCK;
    definitions[REACH_SURFACE_ID_SWITCHER].surface_ops = &reach_switcher_surface_ops;
    definitions[REACH_SURFACE_ID_CLIPBOARD].surface_ops = &reach_clipboard_surface_ops;
    definitions[REACH_SURFACE_ID_CLIPBOARD].layout.anchor = REACH_SURFACE_ID_LAUNCHER;
    definitions[REACH_SURFACE_ID_QUICK_SETTINGS].surface_ops = &reach_quick_settings_surface_ops;
    definitions[REACH_SURFACE_ID_BATTERY].surface_ops = &reach_battery_surface_ops;
    definitions[REACH_SURFACE_ID_TRAY].surface_ops = &reach_tray_surface_ops;
    definitions[REACH_SURFACE_ID_DOCK].resolve_anchor = reach_dock_resolve_anchor;
    definitions[REACH_SURFACE_ID_TOP_BAR].resolve_anchor = reach_top_bar_resolve_anchor;
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
        reach_feature_runtime *desc = &host->feature_runtimes[index];
        const reach_feature_definition *definition = desc->definition;
        if (definition == nullptr || definition->factory.create == nullptr)
        {
            continue;
        }
        reach_result create_result = definition->factory.create(&desc->capsule);
        if (create_result != REACH_OK)
        {
            result = create_result;
        }
    }
    reach_host_bind_shared_feature_capsules(host);
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
        reach_feature_runtime *desc = &host->feature_runtimes[index - 1];
        const reach_feature_definition *definition = desc->definition;
        if (definition != nullptr && definition->factory.destroy != nullptr &&
            desc->capsule != nullptr)
        {
            definition->factory.destroy(desc->capsule);
        }
        desc->capsule = nullptr;
    }
    reach_host_bind_shared_feature_capsules(host);
}
