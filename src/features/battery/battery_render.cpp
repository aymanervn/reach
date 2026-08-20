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

    static const uint16_t percent_label[] = {'B', 'a', 't', 't', 'e', 'r', 'y', ' ', 'p',
                                             'e', 'r', 'c', 'e', 'n', 't', 'a', 'g', 'e', 0};
    static const uint16_t saver_label[] = {'B', 'a', 't', 't', 'e', 'r', 'y',
                                           ' ', 's', 'a', 'v', 'e', 'r', 0};

    reach_battery_push_text(out_commands, state->percent_label, percent_label,
                            metrics.label_text_size * scale, REACH_TEXT_WEIGHT_NORMAL,
                            REACH_TEXT_ALIGNMENT_LEADING, theme->context_menu_text);

    uint16_t value[8] = {};
    reach_battery_format_percent(value, 8, state->model.percent);
    reach_battery_push_text(out_commands, state->percent_value, value,
                            metrics.value_text_size * scale, REACH_TEXT_WEIGHT_DEMIBOLD,
                            REACH_TEXT_ALIGNMENT_TRAILING, theme->primary_text);

    reach_render_command separator = {};
    separator.type = REACH_RENDER_COMMAND_RECT;
    separator.rect = state->separator;
    separator.radius = state->separator.height * 0.5f;
    separator.color = theme->quick_settings_separator;
    (void)reach_render_command_buffer_push(out_commands, &separator);

    reach_battery_push_text(out_commands, state->saver_label, saver_label,
                            metrics.label_text_size * scale, REACH_TEXT_WEIGHT_NORMAL,
                            REACH_TEXT_ALIGNMENT_LEADING, theme->context_menu_text);

    reach_ui_toggle_style toggle_style = {};
    toggle_style.track_off = theme->settings_toggle_track_off;
    toggle_style.track_on = theme->bar_battery_saver;
    toggle_style.knob = theme->settings_toggle_knob;
    reach_ui_toggle_render(out_commands, state->saver_toggle, &toggle_style,
                           reach_battery_model_saver_effective(&state->model) ? 1.0f : 0.0f);

    return REACH_OK;
}
