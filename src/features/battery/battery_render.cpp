#include "battery_common.h"

#include "reach/features/common/ui_controls.h"
#include "reach/support/util.h"

static void reach_battery_push_text(reach_render_command_buffer *commands, reach_rect_f32 rect,
                                    const uint16_t *text, float size, int32_t weight,
                                    int32_t alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis = 1;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

reach_result reach_battery_append_render_commands(const reach_battery *battery,
                                                  const reach_battery_render_context *ctx,
                                                  reach_render_command_buffer *out_commands)
{
    if (battery == nullptr || ctx == nullptr || ctx->theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_battery_state *state = &battery->state;
    if (!state->open || state->bounds.width <= 0.0f)
    {
        return REACH_OK;
    }

    const reach_battery_metrics &metrics = reach_battery_metrics_values;
    const reach_theme *theme = ctx->theme;
    float scale = ctx->dpi_scale > 0.0f ? ctx->dpi_scale : 1.0f;

    static const uint16_t percent_label[] = {'B', 'a', 't', 't', 'e', 'r', 'y', ' ', 'p', 'e',
                                             'r', 'c', 'e', 'n', 't', 'a', 'g', 'e', 0};
    static const uint16_t saver_label[] = {'B', 'a', 't', 't', 'e', 'r', 'y',
                                           ' ', 's', 'a', 'v', 'e', 'r', 0};

    reach_battery_push_text(out_commands, state->percent_label, percent_label,
                            metrics.label_text_size * scale, REACH_TEXT_WEIGHT_NORMAL,
                            REACH_TEXT_ALIGNMENT_LEADING, theme->shared_text);

    uint16_t value[8] = {};
    reach_battery_format_percent(value, 8, state->model.percent);
    reach_battery_push_text(out_commands, state->percent_label, value,
                            metrics.value_text_size * scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                            REACH_TEXT_ALIGNMENT_TRAILING, theme->shared_text);

    static const uint16_t time_label[] = {'T', 'i', 'm', 'e', ' ', 'r', 'e', 'm',
                                          'a', 'i', 'n', 'i', 'n', 'g', 0};
    static const uint16_t source_label[] = {'P', 'o', 'w', 'e', 'r', ' ', 's',
                                            'o', 'u', 'r', 'c', 'e', 0};
    reach_battery_push_text(out_commands, state->time_label,
                            state->model.plugged_in ? source_label : time_label,
                            metrics.label_text_size * scale, REACH_TEXT_WEIGHT_NORMAL,
                            REACH_TEXT_ALIGNMENT_LEADING, theme->shared_text);

    uint16_t time_value[REACH_BATTERY_TIME_TEXT_CAPACITY] = {};
    reach_battery_format_time_remaining(time_value, REACH_BATTERY_TIME_TEXT_CAPACITY,
                                        &state->model);
    reach_render_group time_group = {};
    time_group.id = UINT64_C(0x4200000000000001);
    time_group.bounds = state->time_label;
    time_group.opacity = battery->time_reveal_opacity;
    time_group.y = battery->time_reveal_y;
    reach_render_command_buffer_begin_group(out_commands, &time_group);
    reach_battery_push_text(out_commands, state->time_label, time_value,
                            metrics.value_text_size * scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                            REACH_TEXT_ALIGNMENT_TRAILING, theme->shared_text);
    reach_render_command_buffer_end_group(out_commands);

    (void)reach_ui_horizontal_separator_render(out_commands, state->separator,
                                               theme->popup_divider);

    reach_battery_push_text(out_commands, state->saver_label, saver_label,
                            metrics.label_text_size * scale, REACH_TEXT_WEIGHT_NORMAL,
                            REACH_TEXT_ALIGNMENT_LEADING, theme->shared_text);

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = theme->toggle_off_track;
    toggle_style.track_on = theme->accent_fill;
    toggle_style.knob = theme->toggle_knob;
    reach_ui_toggle_render(out_commands, state->saver_toggle, &toggle_style,
                           reach_battery_model_saver_effective(&state->model) ? 1.0f : 0.0f);

    return reach_ui_feedback_render(out_commands, 1, state->saver_row, theme->radius_small * scale,
                                    theme->battery_press_overlay, &battery->press_feedback,
                                    nullptr);
}
