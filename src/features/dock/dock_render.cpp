#include "reach/features/dock.h"

#include "dock_common_state.h"

#include "dock_common.h"
#include "dock_metrics.h"
#include "reach/features/common/icon_feedback.h"
#include "reach/core/typography.h"

static uint16_t reach_dock_item_fallback_initial(const reach_dock_item_model *item)
{
    if (item == nullptr)
    {
        return '?';
    }
    const uint16_t *path =
        reach_application_identity_primary_runtime_path(&item->application.identity);
    if (path == nullptr)
    {
        path = item->application.launch.path;
    }
    if (path[0] == 0)
    {
        return '?';
    }
    size_t stem = 0;
    for (size_t index = 0; path[index] != 0; ++index)
    {
        if (path[index] == '\\' || path[index] == '/')
        {
            stem = index + 1;
        }
    }
    return path[stem] != 0 ? path[stem] : '?';
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
    const float content_scale =
        input->layout->content_scale > 0.0f ? input->layout->content_scale : 1.0f;
    const float indicator_size = metrics.running_indicator_size * content_scale;
    const float indicator_gap = metrics.running_indicator_gap * content_scale;
    const float indicator_bottom_inset = metrics.running_indicator_bottom_inset * content_scale;
    int32_t focused = input->model->items[index].window == input->focused_window;
    float indicator_y = icon_box.y + icon_box.height + indicator_gap;
    float max_indicator_y = input->layout->bounds.height - indicator_size - indicator_bottom_inset;
    if (indicator_y > max_indicator_y)
    {
        indicator_y = max_indicator_y;
    }

    reach_color color = reach_theme_color_alpha(
        input->theme->dock_running_indicator, focused ? metrics.running_indicator_focused_alpha
                                                      : metrics.running_indicator_unfocused_alpha);

    reach_dock_push_rect(commands,
                         reach_dock_rect(icon_box.x + (icon_box.width - indicator_size) * 0.5f,
                                         indicator_y, indicator_size, indicator_size),
                         color, indicator_size * 0.5f);
}

static void reach_dock_push_item(const reach_dock_render_input *input,
                                 reach_render_command_buffer *commands, size_t index,
                                 float icon_box_size, float icon_box_radius)
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
    if (input->item_motion != nullptr)
    {
        reach_render_group group = {};
        group.id = input->model->items[index].key;
        group.bounds = icon_box;
        group.bounds.height = layout->bounds.height - icon_box.y;
        group.x = input->item_motion[index];
        reach_render_command_buffer_begin_group(commands, &group);
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
        command.text_size = REACH_TEXT_SIZE_HEADING * input->dpi_scale;
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
        reach_push_icon_press_feedback(commands, icon_box, icon_box_radius, icon.id,
                                       theme->dock_background, input->click_feedback_opacity,
                                       reach_dock_metrics_values.click_feedback_min_opacity);
    }
    if (input->item_motion != nullptr)
        reach_render_command_buffer_end_group(commands);
}

static reach_result reach_dock_push_background(const reach_dock_render_input *input,
                                               reach_render_command_buffer *commands,
                                               float dock_radius)
{
    const reach_theme *theme = input->theme;
    const reach_dock_layout *layout = input->layout;

    reach_render_command shape = {};
    shape.type = REACH_RENDER_COMMAND_RECT;
    shape.rect = reach_dock_rect(0.0f, 0.0f, layout->bounds.width, layout->bounds.height);
    shape.radius = dock_radius;
    return reach_render_push_bordered_background(
        commands, &shape, theme->dock_background, theme->bar_border,
        reach_theme_border_thickness(theme, input->dpi_scale) * layout->content_scale,
        &theme->bar_shadow, input->dpi_scale);
}

static void reach_dock_push_trigger_button(const reach_dock_render_input *input,
                                           reach_render_command_buffer *commands,
                                           float icon_box_size)
{
    const reach_theme *theme = input->theme;
    reach_rect_f32 trigger_box =
        reach_dock_icon_box_for_slot(input->layout->trigger_button, icon_box_size);

    reach_color glyph = theme->system_glyph;
    if (input->click_feedback_index == input->trigger_feedback_index &&
        input->click_feedback_opacity > reach_dock_metrics_values.click_feedback_min_opacity)
    {
        glyph =
            reach_theme_color_mix(glyph, theme->bar_click_feedback, input->click_feedback_opacity);
    }

    reach_dock_push_vector_icon(commands, trigger_box, REACH_VECTOR_ICON_MENU, glyph);
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

    reach_result result = reach_dock_push_background(input, out_commands, dock_radius);
    if (result != REACH_OK)
    {
        return result;
    }
    reach_dock_push_trigger_button(input, out_commands, icon_box_size);

    for (size_t index = 0; index < layout->app_slot_count; ++index)
    {
        if (index != input->dragged_render_index)
        {
            reach_dock_push_item(input, out_commands, index, icon_box_size, icon_box_radius);
        }
    }
    if (input->dragged_render_index < layout->app_slot_count)
    {
        reach_dock_push_item(input, out_commands, input->dragged_render_index, icon_box_size,
                             icon_box_radius);
    }

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

    if (index >= state->model.item_count || index >= layout->app_slot_count)
    {
        return 0.0f;
    }

    if ((reach_draggable_tracking(&state->drag.gesture) ||
         reach_motion_active(&state->drag.snap)) &&
        state->drag.key != 0 && reach_dock_item_key_at(&state->model, index) == state->drag.key)
    {
        return reach_motion_active(&state->drag.snap) ? reach_motion_value(&state->drag.snap)
                                                      : state->drag.x;
    }

    const float slot_x = reach_dock_slot_box_x(theme, layout, index);
    uint32_t item_key = reach_dock_item_key_at(&state->model, index);
    if (state->item_x_valid[index] && item_key != 0 && state->item_x_keys[index] == item_key)
    {
        return slot_x + reach_motion_value(&state->item_x[index]);
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

    reach_motion item_motion[REACH_MAX_DOCK_ITEMS] = {};
    float item_reveal[REACH_MAX_DOCK_ITEMS] = {};
    for (size_t index = 0; index < ctx->layout->app_slot_count && index < REACH_MAX_DOCK_ITEMS;
         ++index)
    {
        item_motion[index] = state->item_x[index];
        if (reach_dock_item_key_at(&state->model, index) == state->drag.key &&
            (reach_draggable_tracking(&state->drag.gesture) ||
             reach_motion_active(&state->drag.snap)))
        {
            item_motion[index] = state->drag.snap;
            if (reach_draggable_tracking(&state->drag.gesture))
            {
                item_motion[index].from = item_motion[index].to = state->drag.x;
                item_motion[index].duration_seconds = 0.0;
            }
            float slot_x = reach_dock_slot_box_x(ctx->theme, ctx->layout, index);
            item_motion[index].from -= slot_x;
            item_motion[index].to -= slot_x;
        }
        item_reveal[index] = reach_dock_item_reveal(dock, index);
    }

    size_t dragged_render_index =
        (reach_draggable_tracking(&state->drag.gesture) || reach_motion_active(&state->drag.snap))
            ? reach_dock_feature_model_find_item_key(&state->model, state->drag.key)
            : REACH_MAX_DOCK_ITEMS;
    reach_dock_render_item render_items[REACH_MAX_DOCK_ITEMS] = {};
    for (size_t index = 0; index < state->model.item_count && index < REACH_MAX_DOCK_ITEMS; ++index)
    {
        const reach_dock_item_model *item = &state->model.items[index];
        const uint16_t *icon_path = item->application.icon_ref;
        uint16_t initial = reach_dock_item_fallback_initial(item);
        const reach_window_snapshot *window =
            item->window != 0
                ? reach_window_tracking_window_by_id(reach_dock_windows(dock), item->window)
                : nullptr;
        if (window != nullptr && window->title[0] != 0)
        {
            initial = window->title[0];
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
    input.render_item_count = REACH_MAX_DOCK_ITEMS;
    input.item_motion = item_motion;
    input.item_reveal = item_reveal;
    input.item_reveal_count = REACH_MAX_DOCK_ITEMS;
    input.focused_window = ctx->focused_window;
    input.dragged_render_index = dragged_render_index;
    input.click_feedback_index = reach_pressable_feedback_index(&state->pressable);
    reach_pressable_feedback_style feedback = {};
    feedback.animations = manager;
    feedback.track = REACH_DOCK_ANIM_FEEDBACK_OPACITY;
    input.click_feedback_opacity = reach_pressable_feedback_value(&state->pressable, &feedback);
    input.trigger_feedback_index = REACH_DOCK_FEEDBACK_TRIGGER;
    input.text_alignment_center = REACH_TEXT_ALIGNMENT_CENTER;
    input.dpi_scale = ctx->dpi_scale;

    return reach_dock_build_render_commands(&input, out_commands);
}
