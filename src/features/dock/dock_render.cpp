#include "reach/features/dock.h"

#include "dock_common_state.h"

#include "dock_common.h"
#include "dock_metrics.h"

static reach_color reach_dock_black(float alpha)
{
    reach_color color = {};
    color.a = alpha;
    return color;
}

static reach_rect_f32 reach_dock_center_square(reach_rect_f32 outer, float size)
{
    return reach_dock_rect(outer.x + (outer.width - size) * 0.5f,
                           outer.y + (outer.height - size) * 0.5f, size, size);
}

static reach_rect_f32 reach_dock_union_rect(reach_rect_f32 a, reach_rect_f32 b)
{
    float left = a.x < b.x ? a.x : b.x;
    float top = a.y < b.y ? a.y : b.y;
    float right = a.x + a.width;
    float other_right = b.x + b.width;
    float bottom = a.y + a.height;
    float other_bottom = b.y + b.height;
    if (other_right > right)
    {
        right = other_right;
    }
    if (other_bottom > bottom)
    {
        bottom = other_bottom;
    }
    return reach_dock_rect(left, top, right - left, bottom - top);
}

static void reach_dock_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                 reach_color color, float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_push_vector_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                        uint32_t icon_id, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = color;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_push_icon(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                 uint64_t icon_id, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = color;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                 const uint16_t *text, float text_size, int32_t text_weight,
                                 int32_t text_alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = text_size;
    command.text_weight = text_weight;
    command.text_alignment = text_alignment;
    command.text_ellipsis = 1;
    command.color = color;
    reach_copy_utf16(command.text, 260, text != nullptr ? text : (const uint16_t *)L"");
    reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_push_click_feedback(reach_render_command_buffer *commands,
                                           reach_rect_f32 rect, float radius, float opacity)
{
    if (opacity <= reach_dock_metrics_values.click_feedback_min_opacity)
    {
        return;
    }
    reach_dock_push_rect(commands, rect, reach_dock_black(opacity), radius);
}

static void reach_dock_push_item_feedback(reach_render_command_buffer *commands,
                                          reach_rect_f32 rect, float radius, reach_icon_handle icon,
                                          float opacity)
{
    if (opacity <= reach_dock_metrics_values.click_feedback_min_opacity)
    {
        return;
    }

    if (icon.id != 0)
    {
        reach_render_command command = {};
        command.type = REACH_RENDER_COMMAND_ICON_TINT;
        command.rect = rect;
        command.icon_id = icon.id;
        command.color = reach_dock_black(opacity);
        reach_render_command_buffer_push(commands, &command);
        return;
    }

    reach_dock_push_click_feedback(commands, rect, radius, opacity);
}

static void reach_dock_push_running_indicator(const reach_dock_render_input *input,
                                              reach_render_command_buffer *commands, size_t index,
                                              reach_rect_f32 icon_box)
{
    if (input->model == nullptr || index >= input->model->item_count ||
        input->model->items[index].window == 0)
    {
        return;
    }

    const reach_dock_metrics &metrics = reach_dock_metrics_values;
    int32_t focused = input->model->items[index].window == input->focused_window;
    float indicator_y = icon_box.y + icon_box.height + metrics.running_indicator_gap;
    float max_indicator_y = input->layout->bounds.height - metrics.running_indicator_size -
                            metrics.running_indicator_bottom_inset;
    if (indicator_y > max_indicator_y)
    {
        indicator_y = max_indicator_y;
    }

    reach_color color = {};
    color.r = 1.0f;
    color.g = 1.0f;
    color.b = 1.0f;
    color.a = focused ? metrics.running_indicator_focused_alpha
                      : metrics.running_indicator_unfocused_alpha;

    reach_dock_push_rect(
        commands,
        reach_dock_rect(icon_box.x + (icon_box.width - metrics.running_indicator_size) * 0.5f,
                        indicator_y, metrics.running_indicator_size,
                        metrics.running_indicator_size),
        color, metrics.running_indicator_size * 0.5f);
}

static void reach_dock_push_item(const reach_dock_render_input *input,
                                 reach_render_command_buffer *commands, size_t index,
                                 float override_box_x, int32_t use_override, float icon_box_size,
                                 float icon_box_radius)
{
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;
    float reveal = 1.0f;
    if (input->item_reveal != nullptr && index < input->item_reveal_count)
    {
        reveal = input->item_reveal[index];
    }
    if (reveal <= 0.0f)
    {

        return;
    }
    uint16_t fallback_initial = '?';
    reach_icon_handle icon = {};
    if (input->render_items != nullptr && index < input->render_item_count)
    {
        const reach_dock_render_item *item = &input->render_items[index];
        icon = item->icon;
        fallback_initial = item->fallback_initial != 0 ? item->fallback_initial : '?';
    }
    reach_rect_f32 icon_box = reach_dock_icon_box_for_slot(layout->app_slots[index], icon_box_size);
    if (use_override)
    {
        icon_box.x = override_box_x;
    }
    else if (index < input->item_box_x_count)
    {
        icon_box.x = input->item_box_x[index];
    }
    if (reveal < 1.0f)
    {

        const float inset = icon_box.width * (1.0f - reveal) * 0.5f;
        icon_box.x += inset;
        icon_box.y += inset;
        icon_box.width -= inset * 2.0f;
        icon_box.height -= inset * 2.0f;
    }

    if (icon.id != 0)
    {
        reach_color color = {};
        color.a = reveal;
        reach_dock_push_icon(commands, icon_box, icon.id, color);
    }
    else
    {
        reach_color fallback_background = theme->icon_box_background;
        fallback_background.a *= reach_dock_metrics_values.fallback_icon_background_alpha * reveal;
        reach_dock_push_rect(commands, icon_box, fallback_background, icon_box_radius);

        reach_render_command command = {};
        command.type = REACH_RENDER_COMMAND_TEXT;
        command.rect = icon_box;
        command.color = theme->fallback_icon_text;
        command.text_weight = REACH_TEXT_WEIGHT_BOLD;
        command.color.a *= reveal;
        command.text_alignment = input->text_alignment_center;
        command.text[0] = fallback_initial;
        command.text[1] = 0;
        reach_render_command_buffer_push(commands, &command);
    }

    if (reveal >= 1.0f)
    {
        reach_dock_push_running_indicator(input, commands, index, icon_box);
    }

    if (input->click_feedback_index == index)
    {
        reach_dock_push_item_feedback(commands, icon_box, icon_box_radius, icon,
                                      input->click_feedback_opacity);
    }
}

static void reach_dock_push_background(const reach_theme *theme, const reach_dock_layout *layout,
                                       reach_render_command_buffer *commands, float dock_radius)
{
    reach_dock_push_rect(commands,
                         reach_dock_rect(0.0f, 0.0f, layout->bounds.width, layout->bounds.height),
                         theme->light_background, dock_radius);

    if (theme->border_thickness <= 0.0f || theme->light_border.a <= 0.0f)
    {
        return;
    }

    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    command.rect = reach_dock_rect(theme->border_thickness * 0.5f, theme->border_thickness * 0.5f,
                                   layout->bounds.width - theme->border_thickness,
                                   layout->bounds.height - theme->border_thickness);
    command.color = theme->light_border;
    command.radius = dock_radius;
    command.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_dock_push_system_buttons(const reach_dock_render_input *input,
                                           reach_render_command_buffer *commands,
                                           float icon_box_size, float icon_box_radius)
{
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;
    float system_icon_size = icon_box_size * reach_dock_metrics_values.system_icon_box_scale;

    reach_rect_f32 tray_box = reach_dock_icon_box_for_slot(layout->tray_button, icon_box_size);
    reach_rect_f32 quick_settings_box =
        reach_dock_icon_box_for_slot(layout->quick_settings_button, icon_box_size);

    reach_dock_push_rect(commands, reach_dock_union_rect(tray_box, quick_settings_box),
                         theme->dock_button_background, icon_box_radius);
    reach_dock_push_vector_icon(commands, reach_dock_center_square(tray_box, system_icon_size),
                                REACH_VECTOR_ICON_ARROW_UP, theme->icon_backplate_background);
    reach_dock_push_vector_icon(commands,
                                reach_dock_center_square(quick_settings_box, system_icon_size),
                                REACH_VECTOR_ICON_QUICK_SETTINGS, theme->icon_backplate_background);

    if (input->click_feedback_index == input->tray_feedback_index)
    {
        reach_dock_push_click_feedback(commands, tray_box, icon_box_radius,
                                       input->click_feedback_opacity);
    }
    if (input->click_feedback_index == input->quick_settings_feedback_index)
    {
        reach_dock_push_click_feedback(commands, quick_settings_box, icon_box_radius,
                                       input->click_feedback_opacity);
    }
}

static void reach_dock_push_clock(const reach_dock_render_input *input,
                                  reach_render_command_buffer *commands)
{
    const reach_dock_metrics &metrics = reach_dock_metrics_values;
    reach_rect_f32 clock = input->layout->clock;

    reach_dock_push_text(commands,
                         reach_dock_rect(clock.x, clock.y + metrics.clock_time_top_offset,
                                         clock.width,
                                         clock.height * metrics.clock_time_height_ratio),
                         input->time_text, metrics.clock_time_text_size,
                         metrics.clock_time_text_weight, 0, input->theme->dock_clock_time);

    reach_dock_push_text(
        commands,
        reach_dock_rect(clock.x, clock.y + clock.height * metrics.clock_date_top_ratio, clock.width,
                        clock.height * metrics.clock_date_height_ratio),
        input->date_text, metrics.clock_date_text_size, metrics.clock_date_text_weight, 0,
        input->theme->dock_clock_date);
}

static void reach_dock_push_power_button(const reach_dock_render_input *input,
                                         reach_render_command_buffer *commands, float icon_box_size)
{
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;
    float system_icon_size = icon_box_size * reach_dock_metrics_values.system_icon_box_scale;
    reach_rect_f32 power_box = reach_dock_icon_box_for_slot(layout->power_button, icon_box_size);

    reach_dock_push_rect(commands, power_box, theme->dock_button_background,
                         theme->dock_power_button_corner_radius);
    reach_dock_push_vector_icon(commands, reach_dock_center_square(power_box, system_icon_size),
                                REACH_VECTOR_ICON_POWER, theme->dock_power_glyph);

    if (input->click_feedback_index == input->power_feedback_index)
    {
        reach_dock_push_click_feedback(commands, power_box, theme->dock_power_button_corner_radius,
                                       input->click_feedback_opacity);
    }
}

reach_result reach_dock_build_render_commands(const reach_dock_render_input *input,
                                              reach_render_command_buffer *out_commands)
{
    if (input == nullptr || input->theme == nullptr || input->layout == nullptr ||
        input->model == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;

    float dock_radius = reach_theme_dock_corner_radius(theme, layout->bounds.height);
    float icon_box_size = reach_theme_icon_box_size(theme, layout->bounds.height);
    float icon_box_radius = reach_theme_icon_box_corner_radius(theme, icon_box_size);

    reach_dock_push_background(theme, layout, out_commands, dock_radius);

    for (size_t index = 0; index < layout->app_slot_count; ++index)
    {
        if (index != input->dragged_render_index)
        {
            reach_dock_push_item(input, out_commands, index, 0.0f, 0, icon_box_size,
                                 icon_box_radius);
        }
    }
    if (input->dragged_render_index < layout->app_slot_count)
    {
        reach_dock_push_item(input, out_commands, input->dragged_render_index, input->dragged_box_x,
                             1, icon_box_size, icon_box_radius);
    }

    reach_dock_push_system_buttons(input, out_commands, icon_box_size, icon_box_radius);

    reach_rect_f32 separator = layout->system_separator;
    reach_dock_push_rect(out_commands, separator, theme->dock_system_separator,
                         separator.width * 0.5f);

    reach_dock_push_clock(input, out_commands);
    reach_dock_push_power_button(input, out_commands, icon_box_size);

    return REACH_OK;
}

float reach_dock_item_current_x(reach_dock *dock, const reach_theme *theme,
                                const reach_dock_layout *layout, size_t index)
{
    if (dock == nullptr || theme == nullptr || layout == nullptr)
    {
        return 0.0f;
    }

    reach_dock_state *state = reach_dock_state_mut(dock);
    reach_animation_manager *manager = reach_dock_manager(dock);

    if (index >= state->model.item_count || index >= layout->app_slot_count)
    {
        return 0.0f;
    }

    reach_dock_order_key drag_key = {state->drag.pinned, state->drag.pin_id, state->drag.window};
    if ((state->drag.active ||
         reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP)) &&
        reach_dock_feature_model_item_matches_key(&state->model, index, drag_key))
    {
        return reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP)
                   ? reach_animation_manager_value(manager, REACH_DOCK_ANIM_DRAG_SNAP)
                   : state->drag.x;
    }

    const float slot_x = reach_dock_slot_box_x(theme, layout, index);
    reach_dock_order_key item_x_key = {state->item_x_pinned[index], state->item_x_pin_ids[index],
                                       state->item_x_windows[index]};
    reach_dock_order_key item_key = {state->model.items[index].pinned,
                                     reach_dock_feature_model_item_pin_id(&state->model, index),
                                     state->model.items[index].window};
    if (state->item_x_valid[index] && reach_dock_key_equal(&item_x_key, &item_key))
    {
        return slot_x + reach_animation_manager_value(manager, reach_dock_item_animation_id(index));
    }

    return slot_x;
}

reach_result reach_dock_append_render_commands(reach_dock *dock,
                                               const reach_dock_render_context *ctx,
                                               reach_render_command_buffer *out_commands)
{
    if (dock == nullptr || ctx == nullptr || out_commands == nullptr || ctx->theme == nullptr ||
        ctx->layout == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_dock_state *state = reach_dock_state_mut(dock);
    reach_animation_manager *manager = reach_dock_manager(dock);

    float item_box_x[REACH_MAX_PINNED_APPS] = {};
    float item_reveal[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < ctx->layout->app_slot_count && index < REACH_MAX_PINNED_APPS;
         ++index)
    {
        item_box_x[index] = reach_dock_item_current_x(dock, ctx->theme, ctx->layout, index);
        item_reveal[index] = reach_dock_item_reveal(dock, index);
    }

    reach_dock_order_key drag_key = {state->drag.pinned, state->drag.pin_id, state->drag.window};
    size_t dragged_render_index =
        (state->drag.active || reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP))
            ? reach_dock_feature_model_find_item_key(&state->model, drag_key)
            : REACH_MAX_PINNED_APPS;
    float dragged_x = reach_animation_manager_active(manager, REACH_DOCK_ANIM_DRAG_SNAP)
                          ? reach_animation_manager_value(manager, REACH_DOCK_ANIM_DRAG_SNAP)
                          : state->drag.x;

    reach_dock_render_item render_items[REACH_MAX_PINNED_APPS] = {};
    for (size_t index = 0; index < state->model.item_count && index < REACH_MAX_PINNED_APPS;
         ++index)
    {
        const reach_dock_item_model *item = &state->model.items[index];
        const uint16_t *icon_path = nullptr;
        uint16_t initial = '?';
        if (item->pinned)
        {
            if (ctx->pinned_apps != nullptr && item->pinned_index < ctx->pinned_app_count)
            {
                const reach_pinned_app_model *app = &ctx->pinned_apps[item->pinned_index];
                icon_path = app->icon_ref[0] != 0 ? app->icon_ref : app->path;
                initial = app->title[0] != 0 ? app->title[0] : '?';
            }
        }
        else
        {
            const reach_window_snapshot *window =
                reach_window_tracking_window_by_id(reach_dock_windows(dock), item->window);
            if (window != nullptr)
            {
                icon_path = window->icon_ref[0] != 0 ? window->icon_ref : window->path;
                initial = window->title[0] != 0 ? window->title[0] : '?';
            }
        }
        render_items[index].fallback_initial = initial;
        if (icon_path != nullptr && icon_path[0] != 0)
        {
            render_items[index].icon.id =
                reach_icon_service_get(reach_dock_icons(dock), icon_path, ctx->icon_size_px);
        }
    }

    reach_dock_render_input input = {};
    input.theme = ctx->theme;
    input.layout = ctx->layout;
    input.model = &state->model;
    input.render_items = render_items;
    input.render_item_count = REACH_MAX_PINNED_APPS;
    input.item_box_x = item_box_x;
    input.item_box_x_count = REACH_MAX_PINNED_APPS;
    input.item_reveal = item_reveal;
    input.item_reveal_count = REACH_MAX_PINNED_APPS;
    input.focused_window = ctx->focused_window;
    input.dragged_render_index = dragged_render_index;
    input.dragged_box_x = dragged_x;
    input.click_feedback_index = state->feedback_index;
    input.click_feedback_opacity =
        reach_animation_manager_value(manager, REACH_DOCK_ANIM_FEEDBACK_OPACITY);
    input.tray_feedback_index = REACH_DOCK_FEEDBACK_TRAY_BUTTON;
    input.quick_settings_feedback_index = REACH_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON;
    input.power_feedback_index = REACH_DOCK_FEEDBACK_POWER_BUTTON;
    input.time_text = state->clock_time_text;
    input.date_text = state->clock_date_text;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;

    reach_result result = reach_dock_build_render_commands(&input, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_dock_now_playing_render_context now_playing = {};
    now_playing.theme = ctx->theme;
    now_playing.dpi_scale = ctx->dpi_scale;
    now_playing.reveal_width =
        reach_dock_now_playing_reveal_width(dock, ctx->dock_gap * ctx->dpi_scale);
    return reach_dock_now_playing_append_render_commands(reach_dock_now_playing_subfeature(dock),
                                                         &now_playing, out_commands);
}
