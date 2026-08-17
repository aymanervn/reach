#include "reach/features/top_bar.h"

#include "top_bar_common.h"
#include "top_bar_metrics.h"
#include "top_bar_now_playing.h"

typedef struct reach_top_bar_render_input
{
    const reach_theme *theme;
    const reach_top_bar_layout *layout;
    const uint16_t *time_text;
    const uint16_t *date_text;
    float dpi_scale;
    uint64_t current_app_icon_id;
    const uint16_t *current_app_name;
    const reach_top_bar_tray_item *tray_items;
    size_t tray_item_count;
    int32_t tray_overflow;
    int32_t tray_popup_open;
    const uint16_t *language_code;
    const uint16_t *network_name;
    uint32_t network_icon_id;
    uint32_t bluetooth_icon_id;
    const uint16_t *stats_cpu_text;
    const uint16_t *stats_memory_text;
    const uint16_t *stats_download_text;
    const uint16_t *stats_upload_text;
    int32_t battery_valid;
    int32_t battery_percent;
    float power_hover;
    size_t click_feedback_index;
    float click_feedback_opacity;
} reach_top_bar_render_input;

static reach_rect_f32 reach_top_bar_center_square(reach_rect_f32 outer, float size)
{
    return reach_top_bar_rect(outer.x + (outer.width - size) * 0.5f,
                              outer.y + (outer.height - size) * 0.5f, size, size);
}

static void reach_top_bar_push_rect(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                    reach_color color, float radius)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.color = color;
    command.radius = radius;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_top_bar_push_vector_icon(reach_render_command_buffer *commands,
                                           reach_rect_f32 rect, uint32_t icon_id,
                                           reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    command.rect = rect;
    command.icon_id = icon_id;
    command.color = color;
    reach_render_command_buffer_push(commands, &command);
}

static void reach_top_bar_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
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

static reach_rect_f32 reach_top_bar_rect_union(reach_rect_f32 left, reach_rect_f32 right)
{
    return reach_top_bar_rect(left.x, left.y, right.x + right.width - left.x, left.height);
}

static void reach_top_bar_push_pill(const reach_theme *theme,
                                    reach_render_command_buffer *commands, reach_rect_f32 pill)
{
    float radius = pill.height * 0.5f;

    reach_top_bar_push_rect(commands, pill, theme->bar_background, radius);

    if (theme->border_thickness <= 0.0f || theme->bar_border.a <= 0.0f)
    {
        return;
    }

    reach_render_command border = {};
    border.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    border.rect = reach_top_bar_rect(pill.x + theme->border_thickness * 0.5f,
                                     pill.y + theme->border_thickness * 0.5f,
                                     pill.width - theme->border_thickness,
                                     pill.height - theme->border_thickness);
    border.color = theme->bar_border;
    border.radius = radius;
    border.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &border);
}

static void reach_top_bar_push_power_button(const reach_top_bar_render_input *input,
                                            reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const reach_theme *theme = input->theme;
    reach_rect_f32 power_box = input->layout->power_button;
    if (power_box.width <= 0.0f)
    {
        return;
    }

    reach_color background = reach_theme_color_mix(
        theme->bar_button_background, theme->bar_power_hover_background, input->power_hover);

    reach_top_bar_push_rect(commands, power_box, background, power_box.height * 0.5f);
    reach_top_bar_push_vector_icon(
        commands, reach_top_bar_center_square(power_box, power_box.height * metrics.power_glyph_scale),
        REACH_VECTOR_ICON_POWER, theme->system_glyph);

    if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_POWER_BUTTON &&
        input->click_feedback_opacity > metrics.click_feedback_min_opacity)
    {
        reach_top_bar_push_rect(
            commands, power_box,
            reach_theme_color_alpha(theme->bar_click_feedback, input->click_feedback_opacity),
            power_box.height * 0.5f);
    }
}

static void reach_top_bar_push_clock(const reach_top_bar_render_input *input,
                                     reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const reach_top_bar_layout *layout = input->layout;

    reach_top_bar_push_text(commands, layout->clock_time, input->time_text,
                            metrics.clock_time_text_size * input->dpi_scale,
                            metrics.clock_time_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                            input->theme->bar_text_primary);
    reach_top_bar_push_text(commands, layout->clock_date, input->date_text,
                            metrics.clock_date_text_size * input->dpi_scale,
                            metrics.clock_date_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                            input->theme->bar_text_secondary);
}

static void reach_top_bar_push_button_feedback(const reach_top_bar_render_input *input,
                                               reach_render_command_buffer *commands,
                                               reach_rect_f32 button, size_t feedback_slot)
{
    if (input->click_feedback_index != feedback_slot ||
        input->click_feedback_opacity <= reach_top_bar_metrics_values.click_feedback_min_opacity)
    {
        return;
    }
    reach_top_bar_push_rect(
        commands, button,
        reach_theme_color_alpha(input->theme->bar_click_feedback, input->click_feedback_opacity),
        button.height * 0.5f);
}

static void reach_top_bar_push_separator_dot(const reach_top_bar_render_input *input,
                                             reach_render_command_buffer *commands,
                                             reach_rect_f32 dot)
{
    if (dot.width <= 0.0f)
    {
        return;
    }
    reach_top_bar_push_rect(commands, dot, input->theme->bar_separator_dot, dot.height * 0.5f);
}

static void reach_top_bar_push_current_app(const reach_top_bar_render_input *input,
                                           reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    reach_rect_f32 text = input->layout->current_app_text;
    if (text.width <= 0.0f)
    {
        return;
    }

    if (input->current_app_icon_id != 0 && input->layout->current_app_icon.width > 0.0f)
    {
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.rect = input->layout->current_app_icon;
        icon.icon_id = input->current_app_icon_id;
        icon.color.a = 1.0f;
        reach_render_command_buffer_push(commands, &icon);
    }

    reach_top_bar_push_text(commands, text, input->current_app_name,
                            metrics.current_app_name_text_size * input->dpi_scale,
                            metrics.current_app_name_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                            input->theme->bar_text_primary);
}

static void reach_top_bar_push_tray(const reach_top_bar_render_input *input,
                                    reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const reach_top_bar_layout *layout = input->layout;

    for (size_t index = 0; index < layout->tray_icon_count && index < input->tray_item_count;
         ++index)
    {
        reach_rect_f32 slot = layout->tray_icons[index];
        if (input->tray_items[index].icon_id != 0)
        {
            reach_render_command icon = {};
            icon.type = REACH_RENDER_COMMAND_ICON;
            icon.rect = slot;
            icon.icon_id = input->tray_items[index].icon_id;
            icon.color.a = 1.0f;
            reach_render_command_buffer_push(commands, &icon);
        }

        if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_TRAY_BASE + index &&
            input->click_feedback_opacity > metrics.click_feedback_min_opacity)
        {
            reach_top_bar_push_rect(
                commands, slot,
                reach_theme_color_alpha(input->theme->tray_click_feedback,
                                        input->click_feedback_opacity),
                slot.height * 0.5f);
        }
    }

    if (!input->tray_overflow || layout->tray_overflow_button.width <= 0.0f)
    {
        return;
    }

    reach_rect_f32 overflow = layout->tray_overflow_button;
    reach_top_bar_push_rect(commands, overflow, input->theme->bar_button_background,
                            overflow.height * 0.5f);
    reach_top_bar_push_vector_icon(
        commands,
        reach_top_bar_center_square(overflow, overflow.height * metrics.tray_overflow_glyph_scale),
        input->tray_popup_open ? REACH_VECTOR_ICON_ARROW_UP : REACH_VECTOR_ICON_ARROW_DOWN,
        input->theme->system_glyph);
    reach_top_bar_push_button_feedback(input, commands, overflow,
                                       REACH_TOP_BAR_FEEDBACK_TRAY_OVERFLOW);
}

static void reach_top_bar_push_stat(const reach_top_bar_render_input *input,
                                    reach_render_command_buffer *commands, reach_rect_f32 slot,
                                    const uint16_t *text, reach_color color)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    if (slot.width <= 0.0f)
    {
        return;
    }

    reach_top_bar_push_text(commands, slot, text, metrics.stats_text_size * input->dpi_scale,
                            metrics.stats_text_weight, REACH_TEXT_ALIGNMENT_LEADING, color);
}

static void reach_top_bar_push_stats(const reach_top_bar_render_input *input,
                                     reach_render_command_buffer *commands)
{
    const reach_top_bar_layout *layout = input->layout;
    const reach_theme *theme = input->theme;

    reach_top_bar_push_stat(input, commands, layout->stats_cpu, input->stats_cpu_text,
                            theme->bar_stats_cpu);
    reach_top_bar_push_stat(input, commands, layout->stats_memory, input->stats_memory_text,
                            theme->bar_stats_memory);
    reach_top_bar_push_stat(input, commands, layout->stats_download, input->stats_download_text,
                            theme->bar_stats_download);
    reach_top_bar_push_stat(input, commands, layout->stats_upload, input->stats_upload_text,
                            theme->bar_stats_upload);
}

static void reach_top_bar_push_language(const reach_top_bar_render_input *input,
                                        reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    reach_rect_f32 button = input->layout->language_button;
    if (button.width <= 0.0f)
    {
        return;
    }

    reach_top_bar_push_rect(commands, button, input->theme->bar_button_background,
                            button.height * 0.5f);
    reach_top_bar_push_text(commands, button, input->language_code,
                            metrics.language_text_size * input->dpi_scale,
                            metrics.language_text_weight, REACH_TEXT_ALIGNMENT_CENTER,
                            input->theme->bar_text_primary);
    reach_top_bar_push_button_feedback(input, commands, button,
                                       REACH_TOP_BAR_FEEDBACK_LANGUAGE_BUTTON);
}

static void reach_top_bar_push_battery(const reach_top_bar_render_input *input,
                                       reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const reach_theme *theme = input->theme;
    reach_rect_f32 shell = input->layout->battery_shell;
    if (!input->battery_valid || shell.width <= 0.0f)
    {
        return;
    }

    int32_t percent = input->battery_percent;
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    float shell_radius = shell.height * 0.35f;
    reach_top_bar_push_rect(commands, shell, theme->bar_battery_shell, shell_radius);

    reach_rect_f32 cap = input->layout->battery_cap;
    reach_top_bar_push_rect(commands, cap, theme->bar_battery_shell, cap.height * 0.35f);

    float inset = metrics.battery_fill_inset * input->dpi_scale;
    float track_width = shell.width - inset * 2.0f;
    float fill_height = shell.height - inset * 2.0f;
    if (track_width <= 0.0f || fill_height <= 0.0f)
    {
        return;
    }

    float fill_width = track_width * (float)percent / 100.0f;
    if (fill_width < fill_height)
    {
        fill_width = fill_height;
    }

    reach_color fill_color = (float)percent <= metrics.battery_low_percent
                                 ? theme->bar_battery_low
                                 : theme->bar_battery_fill;
    reach_top_bar_push_rect(commands,
                            reach_top_bar_rect(shell.x + inset, shell.y + inset, fill_width,
                                               fill_height),
                            fill_color, fill_height * 0.5f);
}

static void reach_top_bar_push_glyph_button(const reach_top_bar_render_input *input,
                                            reach_render_command_buffer *commands,
                                            reach_rect_f32 button, uint32_t icon_id,
                                            size_t feedback_slot)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    if (button.width <= 0.0f)
    {
        return;
    }

    reach_top_bar_push_rect(commands, button, input->theme->bar_button_background,
                            button.height * 0.5f);
    reach_top_bar_push_vector_icon(
        commands,
        reach_top_bar_center_square(button, button.height * metrics.bar_button_glyph_scale),
        icon_id, input->theme->system_glyph);
    reach_top_bar_push_button_feedback(input, commands, button, feedback_slot);
}

static void reach_top_bar_push_quick_settings(const reach_top_bar_render_input *input,
                                              reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    const reach_top_bar_layout *layout = input->layout;
    reach_rect_f32 button = layout->quick_settings_button;
    if (button.width <= 0.0f)
    {
        return;
    }

    reach_top_bar_push_rect(commands, button, input->theme->bar_button_background,
                            button.height * 0.5f);
    reach_top_bar_push_vector_icon(commands, layout->network_icon, input->network_icon_id,
                                   input->theme->system_glyph);
    if (layout->network_label.width > 0.0f)
    {
        reach_top_bar_push_text(commands, layout->network_label, input->network_name,
                                metrics.network_name_text_size * input->dpi_scale,
                                metrics.network_name_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                                input->theme->bar_text_primary);
    }
    if (input->bluetooth_icon_id != REACH_VECTOR_ICON_NONE)
    {
        reach_top_bar_push_vector_icon(commands, layout->bluetooth_icon, input->bluetooth_icon_id,
                                       input->theme->system_glyph);
    }
    reach_top_bar_push_button_feedback(input, commands, button,
                                       REACH_TOP_BAR_FEEDBACK_QUICK_SETTINGS_BUTTON);
}

reach_result reach_top_bar_append_render_commands(reach_top_bar *top_bar,
                                                  const reach_top_bar_render_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    if (top_bar == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(out_commands);

    const reach_top_bar_state *state = &top_bar->state;
    const reach_top_bar_layout *layout = &state->layout;

    for (size_t index = 0; index < REACH_TOP_BAR_PILL_COUNT; ++index)
    {
        if (!layout->pill_visible[index] || index == REACH_TOP_BAR_PILL_TRAY)
        {
            continue;
        }
        reach_rect_f32 pill = layout->pills[index];
        if (index == REACH_TOP_BAR_PILL_QUICK_SETTINGS &&
            layout->pill_visible[REACH_TOP_BAR_PILL_TRAY])
        {
            pill = reach_top_bar_rect_union(layout->pills[REACH_TOP_BAR_PILL_TRAY], pill);
        }
        reach_top_bar_push_pill(ctx->theme, out_commands, pill);
    }

    reach_top_bar_render_input input = {};
    input.theme = ctx->theme;
    input.layout = layout;
    input.time_text = state->clock_time_text;
    input.date_text = state->clock_date_text;
    input.dpi_scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;
    input.battery_valid = state->battery_valid;
    input.battery_percent = state->battery_percent;
    input.power_hover =
        reach_animation_manager_value(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER);
    input.click_feedback_index = state->feedback_index;
    input.click_feedback_opacity =
        reach_animation_manager_value(&top_bar->manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY);
    input.current_app_name = state->current_app_name;
    input.tray_items = state->tray_items;
    input.tray_item_count = state->tray_item_count;
    input.tray_overflow = state->tray_overflow;
    input.tray_popup_open = state->tray_popup_open;
    input.language_code = state->language_code;
    input.network_name = state->network_name;
    input.network_icon_id = state->network_icon_id;
    input.bluetooth_icon_id = state->bluetooth_icon_id;
    input.stats_cpu_text = state->stats_cpu_text;
    input.stats_memory_text = state->stats_memory_text;
    input.stats_download_text = state->stats_download_text;
    input.stats_upload_text = state->stats_upload_text;
    if (state->current_app_icon_ref[0] != 0)
    {
        input.current_app_icon_id = reach_icon_service_get(reach_top_bar_icons(top_bar),
                                                           state->current_app_icon_ref,
                                                           ctx->icon_size_px);
    }

    reach_top_bar_push_power_button(&input, out_commands);
    reach_top_bar_push_clock(&input, out_commands);
    reach_top_bar_push_separator_dot(&input, out_commands, layout->now_playing_separator);
    reach_top_bar_push_separator_dot(&input, out_commands, layout->tray_separator);
    reach_top_bar_push_current_app(&input, out_commands);
    reach_top_bar_push_tray(&input, out_commands);
    reach_top_bar_push_stats(&input, out_commands);
    reach_top_bar_push_language(&input, out_commands);
    reach_top_bar_push_battery(&input, out_commands);
    reach_top_bar_push_quick_settings(&input, out_commands);
    reach_top_bar_push_glyph_button(&input, out_commands, layout->settings_button,
                                    REACH_VECTOR_ICON_SETTINGS,
                                    REACH_TOP_BAR_FEEDBACK_SETTINGS_BUTTON);

    reach_top_bar_now_playing_render_context now_playing = {};
    now_playing.theme = ctx->theme;
    now_playing.dpi_scale = ctx->dpi_scale;
    return reach_top_bar_now_playing_append_render_commands(
        reach_top_bar_now_playing_subfeature(top_bar), &now_playing, out_commands);
}
