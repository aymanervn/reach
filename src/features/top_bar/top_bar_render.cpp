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
    const uint16_t *current_app_title;
    const reach_top_bar_tray_item *tray_items;
    size_t tray_item_count;
    int32_t tray_overflow;
    int32_t tray_popup_open;
    const uint16_t *language_code;
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

static void reach_top_bar_push_pill(const reach_theme *theme,
                                    reach_render_command_buffer *commands, reach_rect_f32 pill)
{
    float radius = pill.height * 0.5f;

    reach_top_bar_push_rect(commands, pill, theme->dock_background, radius);

    if (theme->border_thickness <= 0.0f || theme->dock_border.a <= 0.0f)
    {
        return;
    }

    reach_render_command border = {};
    border.type = REACH_RENDER_COMMAND_ROUNDED_RECT_STROKE;
    border.rect = reach_top_bar_rect(pill.x + theme->border_thickness * 0.5f,
                                     pill.y + theme->border_thickness * 0.5f,
                                     pill.width - theme->border_thickness,
                                     pill.height - theme->border_thickness);
    border.color = theme->dock_border;
    border.radius = radius;
    border.stroke_width = theme->border_thickness;
    reach_render_command_buffer_push(commands, &border);
}

static int32_t reach_top_bar_battery_percent_clamped(const reach_top_bar_render_input *input)
{
    int32_t percent = input->battery_percent;
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }
    return percent;
}

static reach_color reach_top_bar_battery_accent(const reach_top_bar_render_input *input,
                                                int32_t percent)
{
    return percent <= 15 ? input->theme->dock_battery_low : input->theme->system_glyph;
}

static void reach_top_bar_push_battery_ring(const reach_top_bar_render_input *input,
                                            reach_render_command_buffer *commands,
                                            reach_rect_f32 power_box, int32_t percent)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;

    float inset = metrics.power_ring_inset + metrics.power_ring_stroke_width * 0.5f;
    reach_rect_f32 ring_box =
        reach_top_bar_rect(power_box.x + inset, power_box.y + inset, power_box.width - inset * 2.0f,
                           power_box.height - inset * 2.0f);

    reach_render_command track = {};
    track.type = REACH_RENDER_COMMAND_ARC_STROKE;
    track.rect = ring_box;
    track.color = input->theme->system_glyph;
    track.color.a *= metrics.power_ring_track_alpha;
    track.stroke_width = metrics.power_ring_stroke_width;
    track.arc_sweep = 1.0f;
    reach_render_command_buffer_push(commands, &track);

    reach_render_command arc = {};
    arc.type = REACH_RENDER_COMMAND_ARC_STROKE;
    arc.rect = ring_box;
    arc.color = reach_top_bar_battery_accent(input, percent);
    arc.stroke_width = metrics.power_ring_stroke_width;
    arc.arc_sweep = (float)percent / 100.0f;
    reach_render_command_buffer_push(commands, &arc);
}

static void reach_top_bar_push_battery_percent(const reach_top_bar_render_input *input,
                                               reach_render_command_buffer *commands,
                                               reach_rect_f32 power_box, int32_t percent)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    float hover = input->power_hover;
    if (hover <= 0.001f)
    {
        return;
    }

    uint16_t percent_text[8] = {};
    size_t length = 0;
    if (percent >= 100)
    {
        percent_text[length++] = '1';
        percent_text[length++] = '0';
        percent_text[length++] = '0';
    }
    else
    {
        if (percent >= 10)
        {
            percent_text[length++] = (uint16_t)('0' + percent / 10);
        }
        percent_text[length++] = (uint16_t)('0' + percent % 10);
    }
    percent_text[length++] = '%';
    percent_text[length] = 0;

    reach_color text_color = reach_top_bar_battery_accent(input, percent);
    text_color.a *= hover;

    reach_top_bar_push_text(commands, power_box, percent_text, metrics.power_percent_text_size,
                            metrics.power_percent_text_weight, REACH_TEXT_ALIGNMENT_CENTER,
                            text_color);
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

    int32_t percent = reach_top_bar_battery_percent_clamped(input);

    reach_color glyph_color = theme->system_glyph;
    reach_color background = theme->dock_button_background;
    if (input->battery_valid)
    {
        glyph_color.a *= 1.0f - input->power_hover;
        background = reach_theme_color_mix(background, theme->dock_power_hover_background,
                                           input->power_hover);
    }

    reach_top_bar_push_rect(commands, power_box, background, power_box.height * 0.5f);
    reach_top_bar_push_vector_icon(
        commands, reach_top_bar_center_square(power_box, power_box.height * metrics.power_glyph_scale),
        REACH_VECTOR_ICON_POWER, glyph_color);

    if (input->battery_valid)
    {
        reach_top_bar_push_battery_percent(input, commands, power_box, percent);
        reach_top_bar_push_battery_ring(input, commands, power_box, percent);
    }

    if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_POWER_BUTTON &&
        input->click_feedback_opacity > metrics.click_feedback_min_opacity)
    {
        reach_top_bar_push_rect(
            commands, power_box,
            reach_theme_color_alpha(theme->dock_click_feedback, input->click_feedback_opacity),
            power_box.height * 0.5f);
    }
}

static void reach_top_bar_push_clock(const reach_top_bar_render_input *input,
                                     reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    reach_rect_f32 clock = input->layout->clock;
    if (clock.width <= 0.0f)
    {
        return;
    }

    float time_height = clock.height * metrics.clock_time_height_ratio;
    reach_top_bar_push_text(commands, reach_top_bar_rect(clock.x, clock.y, clock.width, time_height),
                            input->time_text, metrics.clock_time_text_size,
                            metrics.clock_time_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                            input->theme->dock_clock_time);

    reach_top_bar_push_text(
        commands,
        reach_top_bar_rect(clock.x, clock.y + time_height, clock.width, clock.height - time_height),
        input->date_text, metrics.clock_date_text_size, metrics.clock_date_text_weight,
        REACH_TEXT_ALIGNMENT_LEADING, input->theme->dock_clock_date);
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

    int32_t has_title = input->current_app_title != nullptr && input->current_app_title[0] != 0;
    if (!has_title)
    {
        reach_top_bar_push_text(commands, text, input->current_app_name,
                                metrics.current_app_name_text_size,
                                metrics.current_app_name_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                                input->theme->dock_clock_time);
        return;
    }

    float name_height = text.height * metrics.current_app_name_height_ratio;
    reach_top_bar_push_text(commands,
                            reach_top_bar_rect(text.x, text.y, text.width, name_height),
                            input->current_app_name, metrics.current_app_name_text_size,
                            metrics.current_app_name_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
                            input->theme->dock_clock_time);
    reach_top_bar_push_text(
        commands,
        reach_top_bar_rect(text.x, text.y + name_height, text.width, text.height - name_height),
        input->current_app_title, metrics.current_app_title_text_size,
        metrics.current_app_title_text_weight, REACH_TEXT_ALIGNMENT_LEADING,
        input->theme->dock_clock_date);
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
    reach_top_bar_push_rect(commands, overflow, input->theme->dock_button_background,
                            overflow.height * 0.5f);
    reach_top_bar_push_vector_icon(
        commands, reach_top_bar_center_square(overflow, overflow.height * 0.50f),
        input->tray_popup_open ? REACH_VECTOR_ICON_ARROW_UP : REACH_VECTOR_ICON_ARROW_DOWN,
        input->theme->system_glyph);

    if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_TRAY_OVERFLOW &&
        input->click_feedback_opacity > metrics.click_feedback_min_opacity)
    {
        reach_top_bar_push_rect(commands, overflow,
                                reach_theme_color_alpha(input->theme->dock_click_feedback,
                                                        input->click_feedback_opacity),
                                overflow.height * 0.5f);
    }
}

static void reach_top_bar_push_stats_column(const reach_top_bar_render_input *input,
                                            reach_render_command_buffer *commands,
                                            reach_rect_f32 column, const uint16_t *top_text,
                                            const uint16_t *bottom_text)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    if (column.width <= 0.0f)
    {
        return;
    }

    float line = column.height * metrics.stats_line_height_ratio;
    reach_top_bar_push_text(commands, reach_top_bar_rect(column.x, column.y, column.width, line),
                            top_text, metrics.stats_text_size, metrics.stats_text_weight,
                            REACH_TEXT_ALIGNMENT_LEADING, input->theme->dock_clock_time);
    reach_top_bar_push_text(
        commands, reach_top_bar_rect(column.x, column.y + line, column.width, column.height - line),
        bottom_text, metrics.stats_text_size, metrics.stats_text_weight,
        REACH_TEXT_ALIGNMENT_LEADING, input->theme->dock_clock_date);
}

static void reach_top_bar_push_stats(const reach_top_bar_render_input *input,
                                     reach_render_command_buffer *commands)
{
    reach_top_bar_push_stats_column(input, commands, input->layout->stats_usage,
                                    input->stats_cpu_text, input->stats_memory_text);
    reach_top_bar_push_stats_column(input, commands, input->layout->stats_network,
                                    input->stats_download_text, input->stats_upload_text);
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

    reach_top_bar_push_rect(commands, button, input->theme->dock_button_background,
                            button.height * 0.5f);
    reach_top_bar_push_text(commands, button, input->language_code, metrics.language_text_size,
                            metrics.language_text_weight, REACH_TEXT_ALIGNMENT_CENTER,
                            input->theme->dock_clock_time);

    if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_LANGUAGE_BUTTON &&
        input->click_feedback_opacity > metrics.click_feedback_min_opacity)
    {
        reach_top_bar_push_rect(commands, button,
                                reach_theme_color_alpha(input->theme->dock_click_feedback,
                                                        input->click_feedback_opacity),
                                button.height * 0.5f);
    }
}

static void reach_top_bar_push_quick_settings(const reach_top_bar_render_input *input,
                                              reach_render_command_buffer *commands)
{
    const reach_top_bar_metrics &metrics = reach_top_bar_metrics_values;
    reach_rect_f32 button = input->layout->quick_settings_button;
    if (button.width <= 0.0f)
    {
        return;
    }

    reach_top_bar_push_rect(commands, button, input->theme->dock_button_background,
                            button.height * 0.5f);
    reach_top_bar_push_vector_icon(commands,
                                   reach_top_bar_center_square(button, button.height * 0.55f),
                                   REACH_VECTOR_ICON_QUICK_SETTINGS, input->theme->system_glyph);

    if (input->click_feedback_index == REACH_TOP_BAR_FEEDBACK_QUICK_SETTINGS_BUTTON &&
        input->click_feedback_opacity > metrics.click_feedback_min_opacity)
    {
        reach_top_bar_push_rect(commands, button,
                                reach_theme_color_alpha(input->theme->dock_click_feedback,
                                                        input->click_feedback_opacity),
                                button.height * 0.5f);
    }
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
        if (!layout->pill_visible[index] || index == REACH_TOP_BAR_PILL_NOW_PLAYING)
        {
            continue;
        }
        reach_top_bar_push_pill(ctx->theme, out_commands, layout->pills[index]);
    }

    reach_top_bar_render_input input = {};
    input.theme = ctx->theme;
    input.layout = layout;
    input.time_text = state->clock_time_text;
    input.date_text = state->clock_date_text;
    input.dpi_scale = ctx->dpi_scale;
    input.battery_valid = ctx->battery_valid;
    input.battery_percent = ctx->battery_percent;
    input.power_hover =
        reach_animation_manager_value(&top_bar->manager, REACH_TOP_BAR_ANIM_POWER_HOVER);
    input.click_feedback_index = state->feedback_index;
    input.click_feedback_opacity =
        reach_animation_manager_value(&top_bar->manager, REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY);
    input.current_app_name = state->current_app_name;
    input.current_app_title = state->current_app_title;
    input.tray_items = state->tray_items;
    input.tray_item_count = state->tray_item_count;
    input.tray_overflow = state->tray_overflow;
    input.tray_popup_open = state->tray_popup_open;
    input.language_code = state->language_code;
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
    reach_top_bar_push_current_app(&input, out_commands);
    reach_top_bar_push_tray(&input, out_commands);
    reach_top_bar_push_stats(&input, out_commands);
    reach_top_bar_push_language(&input, out_commands);
    reach_top_bar_push_quick_settings(&input, out_commands);

    reach_top_bar_now_playing_render_context now_playing = {};
    now_playing.theme = ctx->theme;
    now_playing.dpi_scale = ctx->dpi_scale;
    return reach_top_bar_now_playing_append_render_commands(
        reach_top_bar_now_playing_subfeature(top_bar), &now_playing, out_commands);
}
