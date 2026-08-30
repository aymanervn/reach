#include "context_menu_common.h"
#include "reach/core/limits.h"
#include "reach/features/common/pressable.h"
#include "reach/support/animation.h"

#include <new>

enum
{
    REACH_CONTEXT_MENU_ANIM_HOVER = 0,
    REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER,
    REACH_CONTEXT_MENU_ANIM_COUNT
};

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

#define REACH_CONTEXT_MENU_HOVER_MARGIN 12.0f

struct reach_context_menu
{
    reach_context_menu_state state;
    reach_animation_manager animations;
    reach_animation_track animation_tracks[REACH_CONTEXT_MENU_ANIM_COUNT];
    reach_pressable pressable;
    uint64_t pressable_identity;

    reach_menu_request request;
    uintptr_t request_windows[REACH_MENU_MAX_WINDOWS];
    float hover_margin;
};

enum
{
    REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND = 1,
    REACH_CONTEXT_MENU_PRESSABLE_ITEM = 2,
    REACH_CONTEXT_MENU_PRESSABLE_CLOSE = 3
};

const reach_context_menu_state *reach_context_menu_state_ptr(const reach_context_menu *menu)
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
        reach_pressable_reset(&menu->pressable, nullptr);
        menu->pressable_identity = 0;
        menu->state.open = 0;
    }
}

/* The window list stays up while the pointer is anywhere in the band that joins it to the
   control it hangs off, so it survives the gap the pointer crosses on the way over. */
int32_t reach_context_menu_window_list_holds_pointer(const reach_context_menu *menu, float screen_x,
                                                     float screen_y)
{
    if (menu == nullptr || !menu->state.window_list_open)
    {
        return 0;
    }
    if (!menu->request.anchored)
    {
        return 1;
    }
    return reach_context_menu_hover_region_contains(
        menu->state.bounds, menu->request.anchor_button, menu->request.bar_edge_y,
        menu->state.drop_direction, menu->hover_margin, screen_x, screen_y);
}

/* The bounding band of the hold region above, so the pointer can be watched globally while the
   pointer never enters a Reach window. */
int32_t reach_context_menu_window_list_hover_bounds(const reach_context_menu *menu,
                                                    reach_rect_f32 *out_bounds)
{
    if (menu == nullptr || out_bounds == nullptr || !menu->state.window_list_open ||
        !menu->request.anchored)
    {
        return 0;
    }

    const reach_rect_f32 popup = menu->state.bounds;
    const reach_rect_f32 slot = menu->request.anchor_button;
    const float margin = menu->hover_margin;
    float left = popup.x < slot.x ? popup.x : slot.x;
    float right = popup.x + popup.width > slot.x + slot.width ? popup.x + popup.width
                                                              : slot.x + slot.width;
    float top = popup.y < slot.y ? popup.y : slot.y;
    float bottom =
        popup.y + popup.height > slot.y + slot.height ? popup.y + popup.height : slot.y + slot.height;
    if (menu->request.bar_edge_y < top)
    {
        top = menu->request.bar_edge_y;
    }
    if (menu->request.bar_edge_y > bottom)
    {
        bottom = menu->request.bar_edge_y;
    }

    out_bounds->x = left - margin;
    out_bounds->y = top - margin;
    out_bounds->width = (right - left) + margin * 2.0f;
    out_bounds->height = (bottom - top) + margin * 2.0f;
    return 1;
}

size_t reach_context_menu_window_list_target(const reach_context_menu *menu)
{
    return menu != nullptr && menu->state.window_list_open ? menu->state.target_index
                                                           : REACH_CONTEXT_MENU_NO_TARGET;
}

int32_t reach_context_menu_set_open(reach_context_menu *menu, int32_t open)
{
    if (menu == nullptr || open || !reach_context_menu_is_open(menu))
    {
        return 0;
    }
    reach_context_menu_reset(menu);
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
    menu->state.hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    menu->state.close_hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    menu->state.item_count = 0;
    for (size_t index = 0; index < REACH_CONTEXT_MENU_MAX_ITEMS; ++index)
    {
        menu->state.item_icon_ids[index] = 0;
        menu->state.item_windows[index] = 0;
        menu->state.item_titles[index][0] = 0;
    }
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER, 0.0f);
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

float reach_context_menu_close_hover(const reach_context_menu *menu)
{
    return menu != nullptr ? reach_animation_manager_value(&menu->animations,
                                                           REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER)
                           : 0.0f;
}

int32_t reach_context_menu_set_close_hovered(reach_context_menu *menu, size_t index)
{
    if (menu == nullptr || menu->state.close_hovered_index == index)
    {
        return 0;
    }
    menu->state.close_hovered_index = index;
    reach_animation_manager_animate_to(&menu->animations, REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER,
                                       index < REACH_CONTEXT_MENU_MAX_ITEMS ? 1.0f : 0.0f,
                                       REACH_CONTEXT_MENU_CLOSE_HOVER_SECONDS,
                                       REACH_EASING_EASE_OUT);
    return 1;
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
                                     const reach_context_menu_open_context *ctx,
                                     float popup_content_width, float anchor_ratio)
{
    const reach_context_menu_metrics *metrics =
        reach_context_menu_metrics_for(state->window_list_open);
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

    float popup_x;
    float popup_y;
    if (ctx->anchored)
    {
        reach_popup_anchor anchor = {};
        anchor.button = ctx->anchor_button;
        anchor.monitor = ctx->monitor;
        anchor.bar_edge_y = ctx->bar_edge_y;
        anchor.direction = ctx->drop_direction;

        reach_popup_placement placement =
            reach_popup_place(&anchor, popup_width, popup_height, margin);
        popup_x = placement.bounds.x;
        popup_y = placement.bounds.y;
        state->notch_anchor_x = placement.notch_anchor_x;
    }
    else
    {
        popup_x = ctx->pointer_x - popup_width * anchor_ratio;
        popup_y = ctx->pointer_y - popup_height;
        state->notch_anchor_x = ctx->pointer_x;
    }

    state->bounds = {popup_x, popup_y, popup_width, popup_height};
    float items_y = border_thickness + padding +
                    (ctx->drop_direction == REACH_POPUP_DROP_DOWN ? notch_height : 0.0f);
    for (size_t index = 0; index < state->item_count; ++index)
    {
        state->item_slots[index] = {border_thickness + padding,
                                    items_y + item_height * (float)index,
                                    popup_content_width - padding * 2.0f, item_height};
    }
}

static float reach_context_menu_window_list_width(const reach_context_menu_state *state,
                                                  const reach_context_menu_open_context *ctx)
{
    const reach_context_menu_metrics &metrics = reach_context_menu_small_metrics;
    const float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    const float text_size = metrics.text_size * scale;
    float widest_title = 0.0f;
    for (size_t index = 0; index < state->item_count; ++index)
    {
        float width =
            reach_text_width_or_estimate(&ctx->text_measure, state->item_titles[index], text_size,
                                         REACH_TEXT_WEIGHT_NORMAL, metrics.glyph_advance_ratio);
        if (width > widest_title)
        {
            widest_title = width;
        }
    }

    const float chrome = (metrics.padding * 2.0f + metrics.text_leading_inset +
                          metrics.text_trailing_inset_with_close) *
                         scale;
    const float one_letter =
        reach_text_width_or_estimate(&ctx->text_measure, (const uint16_t *)L"W", text_size,
                                     REACH_TEXT_WEIGHT_NORMAL, metrics.glyph_advance_ratio);
    const float minimum = chrome + one_letter;
    float maximum = metrics.window_list_max_width * scale;
    const float border_thickness = reach_theme_border_thickness(ctx->theme, scale);
    const float monitor_width =
        ctx->monitor.width - metrics.screen_margin * scale * 2.0f - border_thickness * 2.0f;
    if (monitor_width > 0.0f && monitor_width < maximum)
    {
        maximum = monitor_width;
    }
    if (maximum < minimum)
    {
        return maximum > 0.0f ? maximum : minimum;
    }

    float width = chrome + widest_title;
    if (width < minimum)
    {
        width = minimum;
    }
    return width > maximum ? maximum : width;
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
    state->power_open = 1;
    state->window_list_open = 0;
    reach_context_menu_place(state, ctx, reach_context_menu_power_popup_width * ctx->dpi_scale,
                             reach_context_menu_power_anchor_ratio);
    state->target_index = REACH_MAX_DOCK_ITEMS;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->close_hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->open = 1;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
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
        out->target.kind = REACH_FEATURE_TARGET_PATH;
        out->target.path = request->path;
        out->target.arguments = request->arguments[0] != 0 ? request->arguments : nullptr;
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
    state->power_open = 0;
    state->window_list_open = 0;
    menu->request = ctx->request != nullptr ? *ctx->request : reach_menu_request{};
    for (size_t index = 0; index < menu->request.window_count && index < REACH_MENU_MAX_WINDOWS;
         ++index)
    {
        menu->request_windows[index] = menu->request.windows[index].window;
    }
    reach_context_menu_place(state, ctx, reach_context_menu_item_popup_width * ctx->dpi_scale,
                             reach_context_menu_item_anchor_ratio);
    state->target_index = target_index;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->close_hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
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
    menu->request = ctx->request != nullptr ? *ctx->request : reach_menu_request{};
    menu->hover_margin = REACH_CONTEXT_MENU_HOVER_MARGIN * ctx->dpi_scale;
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
    state->power_open = 0;
    state->window_list_open = 1;
    float popup_width = reach_context_menu_window_list_width(state, ctx);
    reach_context_menu_place(state, ctx, popup_width, reach_context_menu_window_list_anchor_ratio);
    state->target_index = target_index;
    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->close_hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->open = 1;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER, 0.0f);
}

size_t reach_context_menu_window_list_remove(reach_context_menu *menu, uintptr_t window)
{
    if (menu == nullptr || !menu->state.window_list_open || window == 0)
    {
        return menu != nullptr ? menu->state.item_count : 0;
    }

    reach_context_menu_state *state = &menu->state;
    size_t target = state->item_count;
    for (size_t index = 0; index < state->item_count; ++index)
    {
        if (state->item_windows[index] == window)
        {
            target = index;
            break;
        }
    }
    if (target >= state->item_count)
    {
        return state->item_count;
    }

    for (size_t index = target + 1; index < state->item_count; ++index)
    {
        state->item_windows[index - 1] = state->item_windows[index];
        (void)reach_copy_utf16(state->item_titles[index - 1], 260, state->item_titles[index]);
    }
    --state->item_count;
    state->item_windows[state->item_count] = 0;
    state->item_titles[state->item_count][0] = 0;

    state->hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    state->close_hovered_index = REACH_CONTEXT_MENU_MAX_ITEMS;
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_HOVER, 0.0f);
    reach_animation_manager_set(&menu->animations, REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER, 0.0f);
    return state->item_count;
}

int32_t reach_context_menu_hover_region_contains(reach_rect_f32 popup_bounds,
                                                 reach_rect_f32 anchor_slot, float bar_edge_y,
                                                 int32_t drop_direction, float margin, float x,
                                                 float y)
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
    float corridor_top =
        drop_direction == REACH_POPUP_DROP_DOWN ? bar_edge_y : popup_bounds.y + popup_bounds.height;
    float corridor_bottom = drop_direction == REACH_POPUP_DROP_DOWN ? popup_bounds.y : bar_edge_y;
    return x >= corridor_left - margin && x <= corridor_right + margin &&
           y >= corridor_top - margin && y <= corridor_bottom + margin;
}

void reach_context_menu_reanchor(reach_context_menu *menu,
                                 const reach_context_menu_open_context *ctx)
{
    if (menu == nullptr || ctx == nullptr || !ctx->anchored || !menu->state.open ||
        !menu->state.anchored)
    {
        return;
    }
    float popup_width = menu->state.window_list_open
                            ? reach_context_menu_window_list_width(&menu->state, ctx)
                            : menu->state.anchor_popup_width;
    reach_context_menu_place(&menu->state, ctx, popup_width, menu->state.anchor_ratio);
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

static int32_t reach_context_menu_point_in_bounds(const reach_context_menu_state *state, int32_t x,
                                                  int32_t y)
{
    return state != nullptr && x >= 0 && (float)x <= state->bounds.width && y >= 0 &&
           (float)y <= state->bounds.height;
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
        return reach_context_menu_point_in_bounds(&menu->state, x, y)
                   ? ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_BACKGROUND << 32)
                   : REACH_PRESSABLE_TARGET_NONE;
    }
    if (menu->state.window_list_open)
    {
        reach_context_menu_hit_result close_hit =
            reach_context_menu_hit_test_close_buttons(&menu->state, x, y);
        if (close_hit.hit)
        {
            return ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_CLOSE << 32) | close_hit.index;
        }
    }
    reach_context_menu_hit_result hit =
        reach_context_menu_hit_test_items(menu->state.item_slots, menu->state.item_count, x, y);
    if (hit.hit)
    {
        return ((uint64_t)REACH_CONTEXT_MENU_PRESSABLE_ITEM << 32) | hit.index;
    }
    return reach_context_menu_point_in_bounds(&menu->state, x, y)
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
        reach_context_menu_hit_result hit = reach_context_menu_hit_test_items(
            menu->state.item_slots, menu->state.item_count, event->x, event->y);
        size_t hovered = hit.hit ? hit.index : REACH_CONTEXT_MENU_MAX_ITEMS;
        reach_context_menu_hit_result close_hit =
            reach_context_menu_hit_test_close_buttons(&menu->state, event->x, event->y);
        out->handled = 1;
        out->redraw |= reach_context_menu_set_hovered(menu, hovered);
        out->redraw |= reach_context_menu_set_close_hovered(
            menu, close_hit.hit ? close_hit.index : REACH_CONTEXT_MENU_MAX_ITEMS);
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
        out->redraw |= reach_context_menu_set_hovered(menu, REACH_CONTEXT_MENU_MAX_ITEMS);
        out->redraw |= reach_context_menu_set_close_hovered(menu, REACH_CONTEXT_MENU_MAX_ITEMS);
        break;
    }

    case REACH_POINTER_EVENT_WHEEL:
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
        out->visible_bounds = menu->state.bounds;
        out->notch_anchor_x = menu->state.notch_anchor_x;
        out->notch_side = reach_popup_notch_side(menu->state.drop_direction);
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
    reach_pressable_init(&menu->pressable);
    *out_menu = menu;
    return REACH_OK;
}

void reach_context_menu_destroy(reach_context_menu *menu)
{
    delete menu;
}
