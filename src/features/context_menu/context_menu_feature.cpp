#include "context_menu_common.h"
#include "reach/core/limits.h"

#include <new>

static const double REACH_CONTEXT_MENU_CLOSE_HOVER_SECONDS = 0.14;

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
    if (out_icon_ids != nullptr)
    {
        out_icon_ids[0] = REACH_VECTOR_ICON_LOCK;
        out_icon_ids[1] = REACH_VECTOR_ICON_SLEEP;
        out_icon_ids[2] = REACH_VECTOR_ICON_RESTART;
        out_icon_ids[3] = REACH_VECTOR_ICON_SHUTDOWN;
        out_icon_ids[4] = REACH_VECTOR_ICON_SIGN_OUT;
    }
    *out_count = 5;
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
    if (command == REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL)
    {
        return (const uint16_t *)L"Close all";
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
    return (const uint16_t *)L"";
}

enum
{
    REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND = 1,
    REACH_CONTEXT_MENU_PRESSABLE_ITEM = 2,
    REACH_CONTEXT_MENU_PRESSABLE_CLOSE = 3,
    REACH_CONTEXT_MENU_PRESSABLE_PREVIOUS = 4,
    REACH_CONTEXT_MENU_PRESSABLE_NEXT = 5
};

const reach_context_menu_state *reach_context_menu_state_ptr(const reach_context_menu *menu)
{
    if (menu != nullptr && menu->state.window_list_open && menu->layout_initialized)
    {
        auto *state = &const_cast<reach_context_menu *>(menu)->state;
        double now = reach_motion_now_seconds();
        state->bounds = reach_rect_motion_value_at(&menu->previews.body, now);
        state->bounds.x += menu->previews.envelope.x;
        state->bounds.y += menu->previews.envelope.y;
        state->item_viewport =
            reach_context_menu_preview_bounds_at(menu, &menu->previews.viewport, now);
        for (size_t index = 0; index < REACH_MENU_MAX_WINDOWS; ++index)
            state->item_slots[index] = {};
        for (size_t visible = 0; visible < menu->previews.count; ++visible)
        {
            auto card = reach_context_menu_preview_card_at(menu, visible, now);
            state->item_slots[card.index] = {card.header.x, card.header.y, card.header.width,
                                             state->item_height};
        }
    }
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
        reach_pressable_reset(&menu->pressable, nullptr);
        menu->pressable_identity = 0;
        menu->state.open = 0;
        menu->state.close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
        reach_motion_set(&menu->hover, 0.0f);
        reach_motion_set(&menu->close_hover, 0.0f);
        reach_context_menu_stop_preview_reflow(menu);
        reach_context_menu_reset_preview_layout(menu);
        reach_feature_transition_reset(&menu->popup_transition);
    }
}

int32_t reach_context_menu_window_list_hold_shape(const reach_context_menu *menu,
                                                  reach_rect_union *out_shape)
{
    if (menu == nullptr || out_shape == nullptr || !menu->state.open ||
        !menu->state.window_list_open || !menu->request.anchored)
    {
        return 0;
    }
    *out_shape = reach_context_menu_hover_shape(
        menu->previews.envelope, menu->request.anchor_button, menu->request.bar_edge_y,
        menu->state.drop_direction, menu->hover_margin);
    return 1;
}

int32_t reach_context_menu_window_list_holds_pointer(const reach_context_menu *menu, float screen_x,
                                                     float screen_y)
{
    if (menu == nullptr || !menu->state.open || !menu->state.window_list_open)
    {
        return 0;
    }
    if (!menu->request.anchored)
    {
        return 1;
    }
    reach_rect_union shape = {};
    return reach_context_menu_window_list_hold_shape(menu, &shape) &&
           reach_rect_union_contains(&shape, screen_x, screen_y);
}

size_t reach_context_menu_window_list_target(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.open && menu->state.window_list_open
               ? menu->state.target_index
               : REACH_CONTEXT_MENU_NO_TARGET;
}

int32_t reach_context_menu_set_open(reach_context_menu *menu, int32_t open)
{
    if (menu == nullptr || open || !reach_context_menu_is_open(menu))
    {
        return 0;
    }
    menu->state.open = 0;
    if (menu->state.window_list_open)
    {
        (void)reach_context_menu_state_ptr(menu);
        reach_context_menu_stop_preview_reflow(menu);
    }
    reach_pressable_reset(&menu->pressable, nullptr);
    menu->pressable_identity = 0;
    (void)reach_feature_transition_set_open(&menu->popup_transition, 0);
    return 1;
}

void reach_context_menu_reset(reach_context_menu *menu)
{
    if (menu == nullptr)
    {
        return;
    }
    menu->state.open = 0;
    reach_pressable_reset(&menu->pressable, nullptr);
    menu->pressable_identity = 0;
    menu->state.power_open = 0;
    menu->state.window_list_open = 0;
    menu->state.target_index = REACH_MAX_DOCK_ITEMS;
    menu->state.hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    menu->state.close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    menu->state.item_count = 0;
    menu->state.visible_start = 0;
    menu->state.visible_count = 0;
    menu->previews = {};
    ++menu->preview_generation;
    menu->state.scroll_offset = 0.0f;
    menu->state.scroll_max = 0.0f;
    for (size_t index = 0; index < REACH_MENU_MAX_WINDOWS; ++index)
    {
        menu->state.item_windows[index] = 0;
        menu->state.item_titles[index][0] = 0;
    }
    reach_motion_set(&menu->hover, 0.0f);
    reach_motion_set(&menu->close_hover, 0.0f);
    reach_context_menu_reset_preview_layout(menu);
    reach_feature_transition_reset(&menu->popup_transition);
}

static void reach_context_menu_present(reach_context_menu *menu,
                                       const reach_context_menu_open_context *ctx)
{
    reach_feature_transition_configure(&menu->popup_transition, ctx->theme, ctx->dpi_scale,
                                       ctx->drop_direction);
    menu->state.open = 1;
    (void)reach_feature_transition_set_open(&menu->popup_transition, 1);
}

int32_t reach_context_menu_window_list_is_open(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.open && menu->state.window_list_open;
}

int32_t reach_context_menu_set_close_hovered(reach_context_menu *menu, size_t index)
{
    if (menu == nullptr || menu->state.close_hovered_index == index)
    {
        return 0;
    }
    menu->state.close_hovered_index = index;
    reach_motion_animate_to(&menu->close_hover, index < menu->state.item_count ? 1.0f : 0.0f,
                            REACH_CONTEXT_MENU_CLOSE_HOVER_SECONDS, REACH_EASING_EASE_OUT);
    return 1;
}

int32_t reach_context_menu_set_hovered(reach_context_menu *menu, size_t index)
{
    if (menu == nullptr || menu->state.hovered_index == index)
    {
        return 0;
    }
    int32_t was_hovered = menu->state.hovered_index < menu->state.item_count;
    int32_t is_hovered = index < menu->state.item_count;
    menu->state.hovered_index = index;
    if (is_hovered != was_hovered)
    {
        reach_motion_animate_to(&menu->hover, is_hovered ? 1.0f : 0.0f, 0.18,
                                REACH_EASING_EASE_IN_OUT);
    }
    return 1;
}

static void reach_context_menu_place(reach_context_menu_state *state,
                                     const reach_context_menu_open_context *ctx,
                                     float popup_content_width, float anchor_ratio)
{
    const reach_context_menu_metrics *metrics = &reach_context_menu_popup_metrics;
    state->anchored = ctx->anchored;
    state->drop_direction = ctx->drop_direction;
    state->anchor_popup_width = popup_content_width;
    state->anchor_ratio = anchor_ratio;
    state->dpi_scale = ctx->dpi_scale;
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    float border_thickness = reach_theme_border_thickness(ctx->theme, scale);
    float popup_width = popup_content_width + border_thickness * 2.0f;
    float item_height = metrics->item_height * scale;
    float padding = metrics->padding * scale;
    float margin = metrics->screen_margin * scale;
    float notch_height = reach_popup_notch_height_scaled(scale);
    float popup_body_height = padding * 2.0f + item_height * (float)state->item_count;
    float popup_height = popup_body_height + notch_height + border_thickness * 2.0f;

    reach_rect_f32 placed = {};
    if (ctx->anchored)
    {
        reach_popup_anchor anchor = {};
        anchor.button = ctx->anchor_button;
        anchor.monitor = ctx->monitor;
        anchor.bar_edge_y = ctx->bar_edge_y;
        anchor.direction = ctx->drop_direction;

        reach_popup_placement placement =
            reach_popup_place(&anchor, popup_width, popup_height, margin);
        placed = placement.bounds;
        state->notch_anchor_x = placement.notch_anchor_x;
    }
    else
    {
        placed = {ctx->pointer_x - popup_width * anchor_ratio, ctx->pointer_y - popup_height,
                  popup_width, popup_height};
        placed = reach_popup_constrain_bounds(placed, ctx->monitor, margin);
        state->notch_anchor_x = ctx->pointer_x;
    }

    state->bounds = placed;
    float items_y = border_thickness + padding +
                    (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    float items_bottom = state->bounds.height - border_thickness - padding -
                         (ctx->drop_direction == REACH_POPUP_DROP_UP ? notch_height : 0.0f);
    state->item_viewport = {border_thickness + padding, items_y,
                            state->bounds.width - border_thickness * 2.0f - padding * 2.0f,
                            items_bottom - items_y};
    if (state->item_viewport.width < 0.0f)
    {
        state->item_viewport.width = 0.0f;
    }
    if (state->item_viewport.height < 0.0f)
    {
        state->item_viewport.height = 0.0f;
    }
    state->item_height = item_height;
    state->scroll_max = item_height * (float)state->item_count - state->item_viewport.height;
    if (state->scroll_max < 0.0f)
    {
        state->scroll_max = 0.0f;
    }
    if (state->scroll_offset > state->scroll_max)
    {
        state->scroll_offset = state->scroll_max;
    }
    for (size_t index = 0; index < state->item_count; ++index)
    {
        state->item_slots[index] = {border_thickness + padding,
                                    items_y + item_height * (float)index - state->scroll_offset,
                                    state->item_viewport.width, item_height};
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
    state->scroll_offset = 0.0f;
    reach_context_menu_build_power_commands(state->item_commands, state->item_icon_ids,
                                            &state->item_count);
    state->power_open = 1;
    state->window_list_open = 0;
    reach_context_menu_reset_preview_layout(menu);
    menu->previews = {};
    ++menu->preview_generation;
    reach_context_menu_place(state, ctx, reach_context_menu_power_popup_width * ctx->dpi_scale,
                             reach_context_menu_power_anchor_ratio);
    state->target_index = REACH_MAX_DOCK_ITEMS;
    state->hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    state->close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    reach_context_menu_present(menu, ctx);
    reach_motion_set(&menu->hover, 0.0f);
}

/* The menu answers with the whole request it was opened for, so composition never asks the
   feature that owns the item what the command meant. */
void reach_context_menu_command_action(const reach_menu_request *request,
                                       const uintptr_t *request_windows, uint32_t command,
                                       reach_capsule_action *out)
{
    if (request == nullptr || out == nullptr)
    {
        return;
    }

    out->flags |= REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST;

    if (command >= REACH_CONTEXT_MENU_COMMAND_POWER_LOCK)
    {
        out->kind = REACH_FEATURE_ACTION_EXECUTE_MENU_COMMAND;
        out->id = command;
        return;
    }

    if (!reach_menu_request_allows(request, command))
    {
        out->kind = REACH_FEATURE_ACTION_NONE;
        return;
    }

    switch (command)
    {
    case REACH_CONTEXT_MENU_COMMAND_OPEN_NEW:
    case REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN:
        if (request->path[0] == 0)
        {
            out->kind = REACH_FEATURE_ACTION_NONE;
            return;
        }
        out->kind = REACH_FEATURE_ACTION_OPEN_TARGET;
        out->target.kind = request->launch_kind != REACH_APPLICATION_LAUNCH_NONE ||
                                   request->app_user_model_id[0] != 0
                               ? REACH_FEATURE_TARGET_APP
                               : REACH_FEATURE_TARGET_PATH;
        out->target.path = request->path;
        out->target.arguments = request->arguments[0] != 0 ? request->arguments : nullptr;
        out->target.app_user_model_id = request->app_user_model_id;
        out->target.launch_kind = request->launch_kind;
        out->flags |= REACH_FEATURE_ACTION_FLAG_NEW_INSTANCE;
        if (command == REACH_CONTEXT_MENU_COMMAND_OPEN_AS_ADMIN)
        {
            out->flags |= REACH_FEATURE_ACTION_FLAG_RUN_AS_ADMIN;
        }
        return;

    case REACH_CONTEXT_MENU_COMMAND_UNPIN:
        out->kind = REACH_FEATURE_ACTION_UNPIN_APP;
        out->id = request->pin_id;
        return;

    case REACH_CONTEXT_MENU_COMMAND_PIN:
        out->kind = REACH_FEATURE_ACTION_PIN_APP;
        out->window = request->window;
        out->target.kind = REACH_FEATURE_TARGET_APP;
        out->target.path = request->path;
        out->target.app_user_model_id = request->app_user_model_id;
        out->target.icon_ref = request->icon_ref;
        out->target.launch_kind = request->launch_kind;
        return;

    case REACH_CONTEXT_MENU_COMMAND_CLOSE:
        out->kind = REACH_FEATURE_ACTION_CLOSE_WINDOW;
        out->window = request->window;
        return;

    case REACH_CONTEXT_MENU_COMMAND_CLOSE_ALL:
        if (request->window_count == 0)
        {
            out->kind = REACH_FEATURE_ACTION_CLOSE_WINDOW;
            out->window = request->window;
            return;
        }
        out->kind = REACH_FEATURE_ACTION_CLOSE_WINDOWS;
        out->windows = request_windows;
        out->window_count = request->window_count;
        return;

    default:
        out->kind = REACH_FEATURE_ACTION_NONE;
        return;
    }
}

uint64_t reach_context_menu_owner_identity(const reach_context_menu *menu)
{
    return menu != nullptr ? menu->request.owner_identity : 0;
}

void reach_context_menu_open_for_item(reach_context_menu *menu, size_t target_index,
                                      const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr)
    {
        return;
    }
    reach_context_menu_state *state = &menu->state;
    state->scroll_offset = 0.0f;
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
    state->power_open = 0;
    state->window_list_open = 0;
    reach_context_menu_reset_preview_layout(menu);
    menu->previews = {};
    ++menu->preview_generation;
    menu->request = ctx->request != nullptr ? *ctx->request : reach_menu_request{};
    for (size_t index = 0; index < menu->request.window_count && index < REACH_MENU_MAX_WINDOWS;
         ++index)
    {
        menu->request_windows[index] = menu->request.windows[index].window;
    }
    reach_context_menu_place(state, ctx, reach_context_menu_item_popup_width * ctx->dpi_scale,
                             reach_context_menu_item_anchor_ratio);
    state->target_index = target_index;
    state->hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    state->close_hovered_index = REACH_CONTEXT_MENU_NO_ITEM;
    reach_context_menu_present(menu, ctx);
    reach_motion_set(&menu->hover, 0.0f);
}

static reach_rect_f32 reach_context_menu_grow(reach_rect_f32 rect, float margin)
{
    return {rect.x - margin, rect.y - margin, rect.width + margin * 2.0f,
            rect.height + margin * 2.0f};
}

reach_rect_union reach_context_menu_hover_shape(reach_rect_f32 popup_bounds,
                                                reach_rect_f32 anchor_slot, float bar_edge_y,
                                                int32_t drop_direction, float margin)
{
    float corridor_left = fminf(popup_bounds.x, anchor_slot.x);
    float corridor_right =
        fmaxf(popup_bounds.x + popup_bounds.width, anchor_slot.x + anchor_slot.width);
    float corridor_top =
        drop_direction == REACH_POPUP_DROP_DOWN ? bar_edge_y : popup_bounds.y + popup_bounds.height;
    float corridor_bottom = drop_direction == REACH_POPUP_DROP_DOWN ? popup_bounds.y : bar_edge_y;
    reach_rect_union shape = {};
    shape.rects[shape.count++] = reach_context_menu_grow(popup_bounds, margin);
    reach_rect_f32 slot = reach_context_menu_grow(anchor_slot, margin);
    if (drop_direction == REACH_POPUP_DROP_DOWN)
    {
        slot.height = fmaxf(slot.height, bar_edge_y - slot.y);
    }
    else if (bar_edge_y < slot.y)
    {
        slot.height += slot.y - bar_edge_y;
        slot.y = bar_edge_y;
    }
    shape.rects[shape.count++] = slot;
    shape.rects[shape.count++] = {corridor_left - margin, corridor_top,
                                  corridor_right - corridor_left + margin * 2.0f,
                                  corridor_bottom - corridor_top};
    return shape;
}

int32_t reach_context_menu_reanchor(reach_context_menu *menu,
                                    const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || !ctx->anchored || !menu->state.open ||
        !menu->state.anchored)
    {
        return 0;
    }
    reach_rect_f32 before = menu->state.bounds;
    int32_t changed = 0;
    if (menu->state.window_list_open)
    {
        changed = reach_context_menu_arrange_previews(menu, ctx);
    }
    else
    {
        reach_context_menu_place(&menu->state, ctx, menu->state.anchor_popup_width,
                                 menu->state.anchor_ratio);
    }
    reach_feature_transition_configure(&menu->popup_transition, ctx->theme, menu->state.dpi_scale,
                                       ctx->drop_direction);
    return changed || !reach_rect_equal(before, menu->state.bounds);
}

static void reach_context_menu_capsule_reset(void *capsule)
{
    reach_context_menu_reset(static_cast<reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_capsule_is_open(const void *capsule)
{
    return reach_context_menu_is_open(static_cast<const reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_capsule_wants_pointer_move(const void *capsule)
{
    return reach_context_menu_is_open(static_cast<const reach_context_menu *>(capsule));
}

static int32_t reach_context_menu_point_in_bounds(const reach_context_menu *menu, int32_t x,
                                                  int32_t y)
{
    if (menu == nullptr)
        return 0;
    reach_rect_f32 bounds = {0, 0, menu->state.bounds.width, menu->state.bounds.height};
    if (menu->state.window_list_open && menu->layout_initialized)
        bounds = reach_rect_motion_value_at(&menu->previews.body, reach_motion_now_seconds());
    return x >= bounds.x && x <= bounds.x + bounds.width && y >= bounds.y &&
           y <= bounds.y + bounds.height;
}

static int32_t reach_context_menu_point_in_item_viewport(const reach_context_menu_state *state,
                                                         int32_t x, int32_t y)
{
    return state != nullptr && (float)x >= state->item_viewport.x &&
           (float)x <= state->item_viewport.x + state->item_viewport.width &&
           (float)y >= state->item_viewport.y &&
           (float)y <= state->item_viewport.y + state->item_viewport.height;
}

static uint64_t reach_context_menu_pressable_target(const reach_context_menu *menu, int32_t x,
                                                    int32_t y, reach_pointer_button button)
{
    if (menu == nullptr)
    {
        return REACH_PRESSABLE_TARGET_NONE;
    }
    if (button == REACH_POINTER_BUTTON_SECONDARY)
    {
        return reach_context_menu_point_in_bounds(menu, x, y)
                   ? ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND << 32)
                   : REACH_PRESSABLE_TARGET_NONE;
    }
    if (menu->state.window_list_open)
    {
        double now = reach_motion_now_seconds();
        reach_rect_f32 previous =
            reach_context_menu_preview_bounds_at(menu, &menu->previews.previous, now);
        reach_rect_f32 next = reach_context_menu_preview_bounds_at(menu, &menu->previews.next, now);
        if (previous.width > 0.0f && reach_context_menu_hit_test_items(&previous, 1, x, y).hit)
        {
            return (uint64_t)REACH_CONTEXT_MENU_PRESSABLE_PREVIOUS << 32;
        }
        if (next.width > 0.0f && reach_context_menu_hit_test_items(&next, 1, x, y).hit)
        {
            return (uint64_t)REACH_CONTEXT_MENU_PRESSABLE_NEXT << 32;
        }
    }
    if (menu->state.window_list_open &&
        reach_context_menu_point_in_item_viewport(&menu->state, x, y))
    {
        reach_context_menu_hit_result close_hit =
            reach_context_menu_hit_test_close_buttons(menu, x, y);
        if (close_hit.hit)
        {
            return ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_CLOSE << 32) | close_hit.index;
        }
    }
    reach_context_menu_hit_result hit = {};
    if (reach_context_menu_point_in_item_viewport(&menu->state, x, y))
    {
        hit = reach_context_menu_hit_test_menu_items(menu, x, y);
    }
    if (hit.hit)
    {
        return ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_ITEM << 32) | hit.index;
    }
    return reach_context_menu_point_in_bounds(menu, x, y)
               ? ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND << 32)
               : REACH_PRESSABLE_TARGET_NONE;
}

static uint64_t reach_context_menu_pressable_identity(const reach_context_menu *menu,
                                                      uint64_t target)
{
    if (menu == nullptr || target == REACH_PRESSABLE_TARGET_NONE)
    {
        return 0;
    }
    size_t index = (size_t)(target & UINT32_MAX);
    uint64_t kind = target >> 32;
    if (kind == REACH_CONTEXT_MENU_PRESSABLE_PREVIOUS || kind == REACH_CONTEXT_MENU_PRESSABLE_NEXT)
    {
        return ((uint64_t)menu->state.visible_start << 32) | kind;
    }
    if (kind == REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND || index >= menu->state.item_count)
    {
        return 0;
    }
    return menu->state.window_list_open ? (uint64_t)menu->state.item_windows[index]
                                        : (uint64_t)menu->state.item_commands[index];
}

static void reach_context_menu_apply_pressable_result(const reach_pressable_result *result,
                                                      reach_capsule_pointer_result *out)
{
    if (result == nullptr || out == nullptr)
    {
        return;
    }
    out->redraw |= result->redraw;
    out->capture = result->capture;
    out->sync_pointer_subscriptions = result->sync_pointer_subscriptions;
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
    (void)reach_context_menu_state_ptr(menu);
    if (menu == nullptr || event == nullptr || !menu->state.open ||
        event->coordinate_space != REACH_POINTER_COORDINATE_SURFACE_LOCAL)
    {
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN && event->owner_trigger)
    {
        int32_t keep_open =
            (menu->state.power_open && event->button == REACH_POINTER_BUTTON_PRIMARY) ||
            (!menu->state.power_open && !menu->state.window_list_open &&
             event->button == REACH_POINTER_BUTTON_SECONDARY);
        out->handled = 1;
        out->continue_source_sequence = 1;
        if (!keep_open)
        {
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        }
        return;
    }

    if (event->kind == REACH_POINTER_EVENT_DOWN &&
        event->surface_relation == REACH_POINTER_SURFACE_OUTSIDE)
    {
        out->handled = 1;
        out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
        out->cancel_source_sequence = event->button == REACH_POINTER_BUTTON_PRIMARY;
        out->continue_source_sequence = event->button != REACH_POINTER_BUTTON_PRIMARY;
        return;
    }
    if ((event->kind == REACH_POINTER_EVENT_DOWN || event->kind == REACH_POINTER_EVENT_UP) &&
        event->button == REACH_POINTER_BUTTON_MIDDLE)
    {
        return;
    }

    switch (event->kind)
    {
    case REACH_POINTER_EVENT_DOWN:
    {
        uint64_t target =
            reach_context_menu_pressable_target(menu, event->x, event->y, event->button);
        reach_pressable_result result = {};
        reach_pressable_press(&menu->pressable, event->button, target,
                              REACH_PRESSABLE_FEEDBACK_NONE, nullptr, &result);
        reach_context_menu_apply_pressable_result(&result, out);
        if (result.capture == 1)
        {
            menu->pressable_identity = event->button == REACH_POINTER_BUTTON_PRIMARY
                                           ? reach_context_menu_pressable_identity(menu, target)
                                           : 0;
        }
        out->handled = target != REACH_PRESSABLE_TARGET_NONE;
        break;
    }

    case REACH_POINTER_EVENT_UP:
    {
        int32_t was_tracking = reach_pressable_tracking(&menu->pressable);
        uint64_t identity = menu->pressable_identity;
        reach_pressable_result result = {};
        reach_pressable_release(
            &menu->pressable, event->button,
            reach_context_menu_pressable_target(menu, event->x, event->y, event->button), nullptr,
            &result);
        reach_context_menu_apply_pressable_result(&result, out);
        menu->pressable_identity = 0;
        out->handled = was_tracking;
        if (!result.activated)
        {
            break;
        }
        if (event->button == REACH_POINTER_BUTTON_SECONDARY)
        {
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            break;
        }
        uint64_t kind = result.activated_target >> 32;
        size_t index = (size_t)(result.activated_target & UINT32_MAX);
        if (kind == REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND)
        {
            out->action.kind = REACH_FEATURE_ACTION_CLOSE_SELF;
            break;
        }
        if (kind == REACH_CONTEXT_MENU_PRESSABLE_PREVIOUS ||
            kind == REACH_CONTEXT_MENU_PRESSABLE_NEXT)
        {
            if (identity == reach_context_menu_pressable_identity(menu, result.activated_target))
            {
                out->redraw = reach_context_menu_window_list_page(
                    menu, kind == REACH_CONTEXT_MENU_PRESSABLE_NEXT ? 1 : -1);
            }
            break;
        }
        if (index >= menu->state.item_count ||
            identity != reach_context_menu_pressable_identity(menu, result.activated_target))
        {
            break;
        }
        if (menu->state.window_list_open)
        {
            out->action.window = menu->state.item_windows[index];
            if (kind == REACH_CONTEXT_MENU_PRESSABLE_CLOSE)
            {
                out->action.kind = REACH_FEATURE_ACTION_CLOSE_WINDOW;
                if (reach_context_menu_window_list_remove(menu, out->action.window) == 0)
                {
                    out->action.flags |= REACH_FEATURE_ACTION_FLAG_CLOSE_SELF_FIRST;
                }
                else
                {
                    out->redraw = 1;
                }
            }
            else
            {
                out->action.kind = REACH_FEATURE_ACTION_FOCUS_WINDOW;
            }
        }
        else
        {
            reach_context_menu_command_action(&menu->request, menu->request_windows,
                                              menu->state.item_commands[index], &out->action);
        }
        break;
    }

    case REACH_POINTER_EVENT_MOVE:
    {
        reach_pressable_result pressable_result = {};
        reach_pressable_update(
            &menu->pressable,
            reach_context_menu_pressable_target(menu, event->x, event->y,
                                                reach_pressable_button(&menu->pressable)),
            &pressable_result);
        reach_context_menu_apply_pressable_result(&pressable_result, out);
        reach_context_menu_hit_result hit = {};
        if (reach_context_menu_point_in_item_viewport(&menu->state, event->x, event->y))
        {
            hit = reach_context_menu_hit_test_menu_items(menu, event->x, event->y);
        }
        size_t hovered = hit.hit ? hit.index : REACH_CONTEXT_MENU_NO_ITEM;
        reach_context_menu_hit_result close_hit = {};
        if (reach_context_menu_point_in_item_viewport(&menu->state, event->x, event->y))
        {
            close_hit = reach_context_menu_hit_test_close_buttons(menu, event->x, event->y);
        }
        out->handled = 1;
        out->redraw |= reach_context_menu_set_hovered(menu, hovered);
        out->redraw |= reach_context_menu_set_close_hovered(
            menu, close_hit.hit ? close_hit.index : REACH_CONTEXT_MENU_NO_ITEM);
        break;
    }

    case REACH_POINTER_EVENT_LEAVE:
    case REACH_POINTER_EVENT_CANCEL:
    {
        reach_pressable_result result = {};
        if (event->kind == REACH_POINTER_EVENT_LEAVE)
        {
            reach_pressable_update(&menu->pressable, REACH_PRESSABLE_TARGET_NONE, &result);
        }
        else
        {
            reach_pressable_cancel(&menu->pressable, nullptr, &result);
            menu->pressable_identity = 0;
        }
        reach_context_menu_apply_pressable_result(&result, out);
        out->handled = 1;
        out->redraw |= reach_context_menu_set_hovered(menu, REACH_CONTEXT_MENU_NO_ITEM);
        out->redraw |= reach_context_menu_set_close_hovered(menu, REACH_CONTEXT_MENU_NO_ITEM);
        break;
    }

    case REACH_POINTER_EVENT_WHEEL:
    {
        if (menu->state.window_list_open)
        {
            out->handled = 1;
            out->redraw = reach_context_menu_window_list_page(menu, -event->wheel_delta);
            break;
        }
        if (menu->state.scroll_max <= 0.0f || event->wheel_delta == 0)
        {
            break;
        }
        float previous = menu->state.scroll_offset;
        float delta = menu->state.item_height * 3.0f;
        menu->state.scroll_offset += event->wheel_delta > 0 ? -delta : delta;
        if (menu->state.scroll_offset < 0.0f)
        {
            menu->state.scroll_offset = 0.0f;
        }
        if (menu->state.scroll_offset > menu->state.scroll_max)
        {
            menu->state.scroll_offset = menu->state.scroll_max;
        }
        float moved = menu->state.scroll_offset - previous;
        if (moved != 0.0f)
        {
            for (size_t index = 0; index < menu->state.item_count; ++index)
            {
                menu->state.item_slots[index].y -= moved;
            }
            out->redraw = 1;
        }
        out->handled = 1;
        break;
    }
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
    if (menu->layout_cleanup_pending && !reach_motion_active(&menu->reflow))
    {
        reach_context_menu_stop_preview_reflow(menu);
        reach_context_menu_settle_previews(menu);
        out->redraw = out->request_update = 1;
    }
    int32_t popup_was_visible = reach_feature_transition_visible(&menu->popup_transition);
    if (reach_feature_transition_tick(&menu->popup_transition, delta_seconds))
    {
        out->redraw = 1;
        out->request_update = reach_feature_transition_needs_update(&menu->popup_transition);
    }
    if (popup_was_visible && !reach_feature_transition_visible(&menu->popup_transition) &&
        !menu->state.open)
    {
        reach_context_menu_reset(menu);
    }
}

static int32_t reach_context_menu_capsule_needs_frame(const void *capsule)
{
    const reach_context_menu *menu = static_cast<const reach_context_menu *>(capsule);
    return menu != nullptr && reach_feature_transition_needs_update(&menu->popup_transition);
}

static int32_t reach_context_menu_capsule_presentation_visible(const void *capsule)
{
    const reach_context_menu *menu = static_cast<const reach_context_menu *>(capsule);
    return menu != nullptr && reach_feature_transition_visible(&menu->popup_transition);
}

static int32_t reach_context_menu_capsule_pointer_sequence_active(const void *capsule)
{
    const reach_context_menu *menu = static_cast<const reach_context_menu *>(capsule);
    return menu != nullptr && reach_pressable_tracking(&menu->pressable);
}

static void reach_context_menu_capsule_surface_geometry(const void *capsule,
                                                        reach_feature_surface_geometry *out)
{
    if (out == nullptr)
    {
        return;
    }
    *out = {};
    const reach_context_menu *menu = static_cast<const reach_context_menu *>(capsule);
    if (menu != nullptr)
    {
        out->visible_bounds = menu->state.window_list_open && menu->layout_initialized
                                  ? menu->previews.envelope
                                  : menu->state.bounds;
        out->envelope_bounds = out->visible_bounds;
        out->has_input_bounds = menu->state.window_list_open && menu->layout_initialized;
        out->input_bounds = menu->previews.body;
        out->notch_anchor_x = menu->state.notch_anchor_x;
        out->notch_side = reach_popup_notch_side(menu->state.drop_direction);
        reach_feature_transition_presentation(&menu->popup_transition, out);
        if (menu->layout_cleanup_pending)
            out->deadline_seconds =
                fmax(out->deadline_seconds, reach_motion_deadline(&menu->reflow));
    }
}

const reach_feature_capsule_ops *reach_context_menu_capsule_ops(void)
{
    static const reach_feature_capsule_ops ops = {
        reach_context_menu_capsule_reset,
        reach_context_menu_capsule_tick,
        reach_context_menu_capsule_is_open,
        nullptr,
        reach_context_menu_capsule_needs_frame,
        reach_context_menu_capsule_wants_pointer_move,
        reach_context_menu_capsule_handle_pointer,
        reach_context_menu_capsule_pointer_sequence_active,
        nullptr,
        reach_context_menu_capsule_surface_geometry,
        nullptr,
        nullptr,
        nullptr,
        reach_context_menu_capsule_presentation_visible,
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
    reach_feature_transition_init(&menu->popup_transition, REACH_FEATURE_TRANSITION_FROM_BELOW);
    reach_pressable_init(&menu->pressable);
    reach_context_menu_reset(menu);
    *out_menu = menu;
    return REACH_OK;
}

void reach_context_menu_destroy(reach_context_menu *menu)
{
    delete menu;
}
