#include "reach/features/context_menu.h"

#include "context_menu_common.h"
#include "reach/core/pinned_app.h"
#include "reach/support/animation.h"

#include <new>

enum
{
    REACH_CONTEXT_MENU_ANIM_HOVER = 0,
    REACH_CONTEXT_MENU_ANIM_COUNT
};

void reach_context_menu_build_power_commands(uint32_t *out_commands, uint32_t *out_icon_ids,
                                             size_t *out_count)
{
    if (out_commands == nullptr || out_count == nullptr)
    {
        return;
    }

    out_commands[0] = REACH_CONTEXT_MENU_COMMAND_POWER_LOCK;
    out_commands[1] = REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP;
    out_commands[2] = REACH_CONTEXT_MENU_COMMAND_POWER_RESTART;
    out_commands[3] = REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN;
    out_commands[4] = REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT;
    out_commands[5] = REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS;
    if (out_icon_ids != nullptr)
    {
        out_icon_ids[0] = REACH_VECTOR_ICON_LOCK;
        out_icon_ids[1] = REACH_VECTOR_ICON_SLEEP;
        out_icon_ids[2] = REACH_VECTOR_ICON_RESTART;
        out_icon_ids[3] = REACH_VECTOR_ICON_SHUTDOWN;
        out_icon_ids[4] = REACH_VECTOR_ICON_SIGN_OUT;
        out_icon_ids[5] = REACH_VECTOR_ICON_SETTINGS;
    }
    *out_count = 6;
}

const uint16_t *reach_context_menu_command_text(uint32_t command)
{
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_NEW)
    {
        return (const uint16_t *)L"Open Another Instance";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN)
    {
        return (const uint16_t *)L"Open as admin";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_UNPIN)
    {
        return (const uint16_t *)L"Unpin app from dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_PIN)
    {
        return (const uint16_t *)L"Pin app to dock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE)
    {
        return (const uint16_t *)L"Close app";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_LOCK)
    {
        return (const uint16_t *)L"Lock";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SLEEP)
    {
        return (const uint16_t *)L"Sleep";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_RESTART)
    {
        return (const uint16_t *)L"Restart";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SHUTDOWN)
    {
        return (const uint16_t *)L"Shutdown";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SIGN_OUT)
    {
        return (const uint16_t *)L"Sign out";
    }
    if (command == REACH_CONTEXT_MENU_COMMAND_POWER_SETTINGS)
    {
        return (const uint16_t *)L"Settings";
    }
    return (const uint16_t *)L"";
}

struct reach_context_menu
{
    reach_context_menu_state state;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_CONTEXT_MENU_ANIM_COUNT];
};

const reach_context_menu_state *reach_context_menu_state_ptr(reach_context_menu *menu)
{
    return menu != nullptr ? &menu->state : nullptr;
}

int32_t reach_context_menu_is_open(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.open;
}

void reach_context_menu_force_close(reach_context_menu *menu)
{
    if (menu != nullptr)
    {
        menu->state.open = 0;
    }
}

void reach_context_menu_reset(reach_context_menu *menu)
{
    if (menu == nullptr)
    {
        return;
    }
    menu->state.open = 0;
    menu->state.power_open = 0;
    menu->state.window_list_open = 0;
    menu->state.target_index = REACH_MAX_PINNED_APPS;
    menu->state.hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    menu->state.item_count = 0;
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        menu->state.item_icon_ids[index] = 0;
        menu->state.item_windows[index] = 0;
        menu->state.item_titles[index][0] = 0;
    }
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
}

int32_t reach_context_menu_window_list_is_open(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.open && menu->state.window_list_open;
}

float reach_context_menu_hover_opacity(const reach_context_menu *menu)
{
    return menu != nullptr
               ? reach_animation_manager_value(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER)
               : 0.0f;
}

int32_t reach_context_menu_set_hovered(reach_context_menu *menu, size_t index)
{
    if (menu == nullptr || menu->state.hovered_index == index)
    {
        return 0;
    }
    int32_t was_hovered = menu->state.hovered_index < REACH_CONTEXT_MENU_MAX_ITEMS;
    int32_t is_hovered = index < REACH_CONTEXT_MENU_MAX_ITEMS;
    menu->state.hovered_index = index;
    if (is_hovered != was_hovered)
    {
        reach_animation_manager_animate_to(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER,
                                           is_hovered ? 1.0f : 0.0f, 0.18,
                                           REACH_EASING_EASE_IN_OUT);
    }
    return 1;
}

static void reach_context_menu_place(reach_context_menu_state *state,
                                     const reach_context_menu_open_context *ctx, float popup_width,
                                     float anchor_ratio)
{
    state->anchored = ctx->anchored;
    state->anchor_popup_width = popup_width;
    state->anchor_ratio = anchor_ratio;
    float scale = ctx->dpi_scale;
    float item_height = 34.0f * scale;
    float padding = 8.0f * scale;
    float notch_height = reach_popup_notch_height_scaled(scale);
    float popup_body_height = padding * 2.0f + item_height * (float)state->item_count;
    float popup_height = popup_body_height + notch_height;

    float popup_x;
    float popup_y;
    if (ctx->anchored)
    {
        popup_x = ctx->anchor_x - popup_width * anchor_ratio;
        popup_y = ctx->dock_top_y - popup_height - 8.0f * scale;
        if (popup_x < ctx->monitor.x + 8.0f * scale)
        {
            popup_x = ctx->monitor.x + 8.0f * scale;
        }
        float max_x = ctx->monitor.x + ctx->monitor.width - popup_width - 8.0f * scale;
        if (popup_x > max_x)
        {
            popup_x = max_x;
        }
        if (popup_y < ctx->monitor.y + 8.0f * scale)
        {
            popup_y = ctx->monitor.y + 8.0f * scale;
        }
    }
    else
    {
        popup_x = ctx->pointer_x - popup_width * anchor_ratio;
        popup_y = ctx->pointer_y - popup_height;
    }

    state->bounds = {popup_x, popup_y, popup_width, popup_height};
    for (size_t index = 0; index < state->item_count; ++index)
    {
        state->item_slots[index] = {popup_x + padding,
                                    popup_y + padding + item_height * (float)index,
                                    popup_width - padding * 2.0f, item_height};
    }
}

void reach_context_menu_open_power(reach_context_menu *menu,
                                   const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr)
    {
        return;
    }
    reach_context_menu_state *state = &menu->state;
    reach_context_menu_build_power_commands(state->item_commands, state->item_icon_ids,
                                            &state->item_count);
    reach_context_menu_place(state, ctx, 176.0f * ctx->dpi_scale, 0.72f);
    state->target_index = REACH_MAX_PINNED_APPS;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->power_open = 1;
    state->window_list_open = 0;
    state->open = 1;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
}

void reach_context_menu_open_for_item(reach_context_menu *menu, size_t target_index,
                                      const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr)
    {
        return;
    }
    reach_context_menu_state *state = &menu->state;
    state->item_count = ctx->item_count < REACH_CONTEXT_MENU_MAX_ITEMS
                            ? ctx->item_count
                            : REACH_CONTEXT_MENU_MAX_ITEMS;
    for (size_t index = 0; index < state->item_count; ++index)
    {
        state->item_commands[index] = ctx->item_commands[index];
    }
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        state->item_icon_ids[index] = 0;
    }
    reach_context_menu_place(state, ctx, 208.0f * ctx->dpi_scale, 0.30f);
    state->target_index = target_index;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->power_open = 0;
    state->window_list_open = 0;
    state->open = 1;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
}

void reach_context_menu_open_window_list(reach_context_menu *menu, size_t target_index,
                                         const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || ctx->window_entries == nullptr ||
        ctx->window_entry_count == 0)
    {
        return;
    }
    reach_context_menu_state *state = &menu->state;
    state->item_count = ctx->window_entry_count < REACH_CONTEXT_MENU_MAX_ITEMS
                            ? ctx->window_entry_count
                            : REACH_CONTEXT_MENU_MAX_ITEMS;
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        state->item_commands[index] = 0;
        state->item_icon_ids[index] = 0;
        state->item_windows[index] = 0;
        state->item_titles[index][0] = 0;
    }
    for (size_t index = 0; index < state->item_count; ++index)
    {
        state->item_windows[index] = ctx->window_entries[index].window;
        if (ctx->window_entries[index].title != nullptr)
        {
            (void)reach_copy_utf16(state->item_titles[index], 260,
                                   ctx->window_entries[index].title);
        }
    }
    reach_context_menu_place(state, ctx, 232.0f * ctx->dpi_scale, 0.5f);
    state->target_index = target_index;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->power_open = 0;
    state->window_list_open = 1;
    state->open = 1;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
}

int32_t reach_context_menu_hover_region_contains(reach_rect_f32 popup_bounds,
                                                 reach_rect_f32 anchor_slot, float dock_top_y,
                                                 float margin, float x, float y)
{
    if (x >= popup_bounds.x - margin && x <= popup_bounds.x + popup_bounds.width + margin &&
        y >= popup_bounds.y - margin && y <= popup_bounds.y + popup_bounds.height + margin)
    {
        return 1;
    }

    if (x >= anchor_slot.x - margin && x <= anchor_slot.x + anchor_slot.width + margin &&
        y >= anchor_slot.y - margin && y <= anchor_slot.y + anchor_slot.height + margin)
    {
        return 1;
    }

    float corridor_left = popup_bounds.x < anchor_slot.x ? popup_bounds.x : anchor_slot.x;
    float corridor_right = popup_bounds.x + popup_bounds.width > anchor_slot.x + anchor_slot.width
                               ? popup_bounds.x + popup_bounds.width
                               : anchor_slot.x + anchor_slot.width;
    float corridor_top = popup_bounds.y + popup_bounds.height;
    return x >= corridor_left - margin && x <= corridor_right + margin &&
           y >= corridor_top - margin && y <= dock_top_y + margin;
}

void reach_context_menu_reanchor(reach_context_menu *menu,
                                 const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || !ctx->anchored || !menu->state.open ||
        !menu->state.anchored)
    {
        return;
    }
    reach_context_menu_place(&menu->state, ctx, menu->state.anchor_popup_width,
                             menu->state.anchor_ratio);
}

static void reach_context_menu_capsule_reset(void *capsule)
{
    reach_context_menu_reset(static_cast<reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_capsule_is_open(const void *capsule)
{
    return reach_context_menu_is_open(static_cast<const reach_context_menu *>(capsule));
}

static void reach_context_menu_capsule_force_close(void *capsule)
{
    reach_context_menu_reset(static_cast<reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_capsule_wants_pointer_move(const void *capsule)
{
    return reach_context_menu_is_open(static_cast<const reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_point_in_bounds(const reach_context_menu_state *state, int32_t x,
                                                  int32_t y)
{
    return state != nullptr && (float)x >= state->bounds.x &&
           (float)x <= state->bounds.x + state->bounds.width && (float)y >= state->bounds.y &&
           (float)y <= state->bounds.y + state->bounds.height;
}

static void reach_context_menu_capsule_handle_pointer(void *capsule,
                                                      const reach_pointer_event *event,
                                                      reach_capsule_pointer_result *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};

    reach_context_menu *menu = static_cast<reach_context_menu *>(capsule);
    if (menu == nullptr || event == nullptr || !menu->state.open)
    {
        return;
    }

    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
        out->handled = reach_context_menu_point_in_bounds(&menu->state, event->x, event->y);
        break;

    case REACH_POINTER_EVENT_UP:
    {
        reach_context_menu_hit_result hit = reach_context_menu_hit_test_items(
            menu->state.item_slots, menu->state.item_count, event->x, event->y);
        out->handled = 1;
        if (menu->state.window_list_open)
        {
            if (hit.hit && menu->state.item_windows[hit.index] != 0)
            {
                out->action.kind = REACH_CONTEXT_MENU_POINTER_ACTION_FOCUS_WINDOW;
                out->action.window = menu->state.item_windows[hit.index];
            }
            else
            {
                out->action.kind = REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS;
            }
            break;
        }
        reach_context_menu_action action = reach_context_menu_action_for_hit(
            menu->state.item_commands, menu->state.item_count, hit);
        if (action.command != 0)
        {
            out->action.kind = REACH_CONTEXT_MENU_POINTER_ACTION_EXECUTE;
            out->action.id = action.command;
        }
        else
        {
            out->action.kind = REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS;
        }
        break;
    }

    case REACH_POINTER_EVENT_MOVE:
    {
        reach_context_menu_hit_result hit = reach_context_menu_hit_test_items(
            menu->state.item_slots, menu->state.item_count, event->x, event->y);
        size_t hovered = hit.hit ? hit.index : REACH_CONTEXT_MENU_MAX_ITEMS;
        out->handled = 1;
        out->redraw = reach_context_menu_set_hovered(menu, hovered);
        break;
    }

    case REACH_POINTER_EVENT_LEAVE:
    case REACH_POINTER_EVENT_CANCEL:
        out->handled = 1;
        out->redraw = reach_context_menu_set_hovered(menu, REACH_CONTEXT_MENU_MAX_ITEMS);
        break;

    case REACH_POINTER_EVENT_CONTEXT:
        out->handled = 1;
        out->action.kind = REACH_CONTEXT_MENU_POINTER_ACTION_DISMISS;
        break;

    case REACH_POINTER_EVENT_WHEEL:
    case REACH_POINTER_EVENT_MIDDLE:
    default:
        break;
    }
}

static void reach_context_menu_capsule_tick(void *capsule, double delta_seconds,
                                            reach_feature_tick_result *out)
{
    reach_context_menu *menu = static_cast<reach_context_menu *>(capsule);
    if (menu == nullptr || out == nullptr)
    {
        return;
    }
    if (reach_animation_manager_any_active(&menu->animations))
    {
        reach_animation_manager_tick(&menu->animations, delta_seconds);
        if (menu->state.open)
        {
            out->redraw = 1;
            out->request_update = 1;
        }
    }
}

static int32_t reach_context_menu_capsule_needs_frame(const void *capsule)
{
    const reach_context_menu *menu = static_cast<const reach_context_menu *>(capsule);
    return menu != nullptr && menu->state.open &&
           reach_animation_manager_any_active(&menu->animations);
}

const reach_feature_capsule_ops *reach_context_menu_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_context_menu_capsule_reset,
        reach_context_menu_capsule_tick,
        reach_context_menu_capsule_is_open,
        reach_context_menu_capsule_force_close,
        nullptr,
        reach_context_menu_capsule_needs_frame,
        reach_context_menu_capsule_wants_pointer_move,
        reach_context_menu_capsule_handle_pointer,
    };
    return &ops;
}

reach_result reach_context_menu_create(reach_context_menu **out_menu)
{
    if (out_menu == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_context_menu *menu = new (std::nothrow) reach_context_menu();
    if (menu == nullptr)
    {
        return REACH_ERROR;
    }
    reach_animation_manager_init(&menu->animations, menu->animation_tracks,
                                 REACH_CONTEXT_MENU_ANIM_COUNT);
    *out_menu = menu;
    return REACH_OK;
}

void reach_context_menu_destroy(reach_context_menu *menu)
{
    delete menu;
}
