#include "reach/features/quick_settings.h"
#include "reach/features/popup.h"

#include "quick_settings_common.h"
#include "quick_settings_metrics.h"

static float reach_quick_settings_min_f32(float a, float b)
{
    return a < b ? a : b;
}

static reach_color reach_quick_settings_color_alpha(reach_color color, float alpha)
{
    color.a = alpha;
    return color;
}

static void reach_quick_settings_push_rounded_rect(reach_render_command_buffer *commands,
                                                   reach_rect_f32 rect, float radius,
                                                   reach_color color);

static void reach_quick_settings_push_text(reach_render_command_buffer *commands,
                                           reach_rect_f32 rect, const uint16_t *text, float size,
                                           int32_t weight, int32_t alignment, reach_color color);
static void reach_quick_settings_push_ellipsized_text(reach_render_command_buffer *commands,
                                                      reach_rect_f32 rect, const uint16_t *text,
                                                      float size, int32_t weight,
                                                      int32_t alignment, reach_color color);

static size_t reach_quick_settings_utf16_length(const uint16_t *text)
{
    size_t length = 0;
    if (text == nullptr)
    {
        return 0;
    }
    while (text[length] != 0)
    {
        ++length;
    }
    return length;
}

static uint16_t reach_quick_settings_ascii_lower(uint16_t value)
{
    if (value >= 'A' && value <= 'Z')
    {
        return (uint16_t)(value - 'A' + 'a');
    }
    return value;
}

static int reach_quick_settings_label_has_exe_suffix(const uint16_t *text, size_t length)
{
    return length > 4 && text[length - 4] == '.' &&
           reach_quick_settings_ascii_lower(text[length - 3]) == 'e' &&
           reach_quick_settings_ascii_lower(text[length - 2]) == 'x' &&
           reach_quick_settings_ascii_lower(text[length - 1]) == 'e';
}

static void reach_quick_settings_copy_display_label(uint16_t *dst, size_t dst_count,
                                                    const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t length = reach_quick_settings_utf16_length(src);
    if (reach_quick_settings_label_has_exe_suffix(src, length))
    {
        length -= 4;
    }
    if (length + 1 > dst_count)
    {
        length = dst_count - 1;
    }

    for (size_t index = 0; index < length; ++index)
    {
        dst[index] = src[index];
    }
    dst[length] = 0;
}

static void reach_quick_settings_copy_device_primary_label(uint16_t *dst, size_t dst_count,
                                                           const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t index = 0;
    if (src != nullptr)
    {
        while (index + 1 < dst_count && src[index] != 0 && src[index] != '(')
        {
            dst[index] = src[index];
            ++index;
        }

        while (index > 0 && dst[index - 1] == ' ')
        {
            --index;
        }
    }

    dst[index] = 0;
}

static void reach_quick_settings_copy_device_secondary_label(uint16_t *dst, size_t dst_count,
                                                             const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    size_t out_index = 0;
    size_t in_index = 0;

    if (src != nullptr)
    {
        while (src[in_index] != 0 && src[in_index] != '(')
        {
            ++in_index;
        }

        if (src[in_index] == '(')
        {
            ++in_index;
            while (out_index + 1 < dst_count && src[in_index] != 0 && src[in_index] != ')')
            {
                dst[out_index++] = src[in_index++];
            }
        }
    }

    dst[out_index] = 0;
}

static void reach_quick_settings_capitalize_first_utf16(uint16_t *text)
{
    if (text == nullptr || text[0] == 0)
    {
        return;
    }

    if (text[0] >= 'a' && text[0] <= 'z')
    {
        text[0] = (uint16_t)(text[0] - 'a' + 'A');
    }
}

static void reach_quick_settings_format_percent(uint16_t *dst, size_t dst_count, float volume)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    int percent = (int)(reach_quick_settings_clamp01(volume) * 100.0f + 0.5f);
    if (percent < 0)
    {
        percent = 0;
    }
    if (percent > 100)
    {
        percent = 100;
    }

    size_t index = 0;
    if (percent >= 100 && index + 1 < dst_count)
    {
        dst[index++] = '1';
        dst[index++] = '0';
        dst[index++] = '0';
    }
    else if (percent >= 10 && index + 1 < dst_count)
    {
        dst[index++] = (uint16_t)('0' + (percent / 10));
        dst[index++] = (uint16_t)('0' + (percent % 10));
    }
    else if (index + 1 < dst_count)
    {
        dst[index++] = (uint16_t)('0' + percent);
    }
    if (index + 1 < dst_count)
    {
        dst[index++] = '%';
    }
    dst[index] = 0;
}

static uint32_t reach_quick_settings_network_icon_id(const reach_network_state *state)
{
    if (state == nullptr || !state->connected)
    {
        return REACH_VECTOR_ICON_NO_INTERNET;
    }
    if (state->kind == REACH_NETWORK_KIND_ETHERNET)
    {
        return REACH_VECTOR_ICON_ETHERNET;
    }
    if (state->signal_strength < 34)
    {
        return REACH_VECTOR_ICON_WIFI_LOW;
    }
    if (state->signal_strength < 67)
    {
        return REACH_VECTOR_ICON_WIFI_MEDIUM;
    }
    return REACH_VECTOR_ICON_WIFI_HIGH;
}

static void reach_quick_settings_network_label(const reach_network_state *state,
                                               uint16_t *out_label, size_t out_label_count)
{
    static const uint16_t no_internet[] = {'N', 'o', ' ', 'i', 'n', 't',
                                           'e', 'r', 'n', 'e', 't', 0};
    static const uint16_t ethernet[] = {'E', 't', 'h', 'e', 'r', 'n', 'e', 't', 0};
    static const uint16_t wifi[] = {'W', 'i', '-', 'F', 'i', 0};

    if (state == nullptr || !state->connected)
    {
        reach_quick_settings_copy_utf16(out_label, out_label_count, no_internet);
        return;
    }
    if (state->kind == REACH_NETWORK_KIND_ETHERNET)
    {
        reach_quick_settings_copy_utf16(out_label, out_label_count, ethernet);
        return;
    }
    if (state->label[0] != 0)
    {
        reach_quick_settings_copy_utf16(out_label, out_label_count, state->label);
    }
    else
    {
        reach_quick_settings_copy_utf16(out_label, out_label_count, wifi);
    }
}

static void reach_quick_settings_push_system_tile_commands(
    reach_render_command_buffer *commands, const reach_quick_settings_tile_layout *layout,
    uint32_t icon_id, const uint16_t *label, int32_t active, const reach_theme *theme,
    const reach_quick_settings_metrics *metrics, float dpi_scale)
{
    if (commands == nullptr || layout == nullptr || theme == nullptr)
    {
        return;
    }

    reach_color active_background = {1.0f, 1.0f, 1.0f, 1.0f};
    reach_color active_foreground = {0.0f, 0.0f, 0.0f, 1.0f};
    reach_color foreground = active ? active_foreground : theme->light_text;

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    reach_quick_settings_push_rounded_rect(commands, layout->bounds,
                                           reach_popup_radius_scaled(dpi_scale),
                                           active ? active_background
                                                  : theme->quick_settings_button_color);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = layout->icon;
    icon.icon_id = icon_id;
    icon.color = foreground;
    (void)reach_render_command_buffer_push(commands, &icon);

    reach_quick_settings_push_text(commands, layout->label, label,
                                   values->system_tile_text_size,
                                   REACH_TEXT_WEIGHT_SEMIBOLD, 0, foreground);
}

static void reach_quick_settings_push_rounded_rect(reach_render_command_buffer *commands,
                                                   reach_rect_f32 rect, float radius,
                                                   reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_RECT;
    command.rect = rect;
    command.radius = radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_push_clipped_rounded_rect(reach_render_command_buffer *commands,
                                                           reach_rect_f32 rect, float radius,
                                                           reach_rect_f32 clip_rect,
                                                           float clip_radius, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_CLIPPED_ROUNDED_RECT;
    command.rect = rect;
    command.clip_rect = clip_rect;
    command.radius = radius;
    command.clip_radius = clip_radius;
    command.color = color;
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_push_text(reach_render_command_buffer *commands,
                                           reach_rect_f32 rect, const uint16_t *text, float size,
                                           int32_t weight, int32_t alignment, reach_color color)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.color = color;
    reach_quick_settings_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_push_ellipsized_text(reach_render_command_buffer *commands,
                                                      reach_rect_f32 rect, const uint16_t *text,
                                                      float size, int32_t weight,
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
    reach_quick_settings_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static const reach_audio_output_device *
reach_quick_settings_current_output_device(const reach_quick_settings_model *model)
{
    if (model == nullptr)
    {
        return nullptr;
    }

    for (size_t index = 0; index < model->output_devices.count; ++index)
    {
        if (model->output_devices.devices[index].is_default)
        {
            return &model->output_devices.devices[index];
        }
    }

    return model->output_devices.count > 0 ? &model->output_devices.devices[0] : nullptr;
}

static void reach_quick_settings_push_output_icon(reach_render_command_buffer *commands,
                                                  reach_rect_f32 rect, uint64_t icon_id,
                                                  const reach_theme *theme)
{
    reach_render_command icon = {};
    icon.rect = rect;
    icon.color = theme->icon_backplate_background;
    if (icon_id != 0)
    {
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.icon_id = icon_id;
    }
    else
    {
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.icon_id = REACH_VECTOR_ICON_VOLUME_HIGH;
    }
    (void)reach_render_command_buffer_push(commands, &icon);
}

static reach_result reach_quick_settings_push_volume_pill_commands_with_label(
    const reach_quick_settings_volume_pill_model *model,
    const reach_quick_settings_volume_pill_layout *layout, const reach_theme *theme,
    reach_render_command_buffer *commands, float label_text_size,
    int32_t label_centered_with_icon, const reach_quick_settings_metrics *metrics,
    float dpi_scale)
{
    if (model == nullptr || layout == nullptr || theme == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    float volume = reach_quick_settings_clamp01(model->volume_level);
    reach_quick_settings_volume_pill_layout pill = *layout;
    pill.slider_fill.y = pill.slider_track.y;
    pill.slider_fill.width = pill.slider_track.width * volume;
    pill.slider_fill.height = pill.slider_track.height;
    if (pill.slider_fill.width < 0.0f)
    {
        pill.slider_fill.width = 0.0f;
    }

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = pill.header_icon;
    icon.icon_id = model->icon_id;
    icon.color = theme->icon_backplate_background;
    (void)reach_render_command_buffer_push(commands, &icon);

    reach_rect_f32 label_rect = pill.header_label;
    if (label_centered_with_icon)
    {
        label_rect.height = label_text_size;
        label_rect.y = pill.header_icon.y + (pill.header_icon.height - label_rect.height) * 0.5f;
    }

    reach_quick_settings_push_text(commands, label_rect, model->label, label_text_size,
                                   values->pill_label_text_weight, 0, theme->light_text);

    float track_radius = reach_popup_radius_scaled(dpi_scale);
    float fill_radius =
        reach_quick_settings_min_f32(track_radius, pill.slider_fill.width * 0.5f);

    reach_color slider_fill_color = model->muted ? theme->quick_settings_slider_muted_fill_color
                                                 : theme->quick_settings_slider_fill_color;

    reach_quick_settings_push_rounded_rect(commands, pill.slider_track, track_radius,
                                           theme->quick_settings_slider_track_color);

    if (pill.slider_fill.width > 0.0f)
    {
        reach_quick_settings_push_clipped_rounded_rect(commands, pill.slider_fill, fill_radius,
                                                       pill.slider_track, track_radius,
                                                       slider_fill_color);
    }

    return REACH_OK;
}

reach_result reach_quick_settings_push_volume_pill_commands(
    const reach_quick_settings_volume_pill_model *model,
    const reach_quick_settings_volume_pill_layout *layout, const reach_theme *theme,
    reach_render_command_buffer *commands)
{
    return reach_quick_settings_push_volume_pill_commands_with_label(
        model, layout, theme, commands,
        reach_quick_settings_metrics_values.default_pill_label_text_size, 0, nullptr, 1.0f);
}

static reach_result reach_quick_settings_push_output_device_row_commands(
    const reach_audio_output_device *device,
    const reach_quick_settings_output_device_row_layout *layout, size_t row_index, size_t row_count,
    const reach_theme *theme, reach_render_command_buffer *commands,
    const reach_quick_settings_metrics *metrics)
{
    if (device == nullptr || layout == nullptr || theme == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    reach_quick_settings_push_output_icon(commands, layout->icon, device->icon_id, theme);

    uint16_t primary_label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
    uint16_t secondary_label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};

    reach_quick_settings_copy_device_primary_label(
        primary_label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY, device->label);

    reach_quick_settings_copy_device_secondary_label(
        secondary_label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY, device->label);

    reach_rect_f32 primary_rect = layout->label;
    primary_rect.y += values->output_row_primary_top;
    primary_rect.height = values->output_row_primary_height;

    reach_quick_settings_push_text(commands, primary_rect, primary_label,
                                   values->output_row_primary_text_size,
                                   REACH_TEXT_WEIGHT_NORMAL, 0, theme->light_text);

    if (secondary_label[0] != 0)
    {
        reach_rect_f32 secondary_rect = layout->label;
        secondary_rect.y += values->output_row_secondary_top;
        secondary_rect.height = values->output_row_secondary_height;

        reach_quick_settings_push_text(
            commands, secondary_rect, secondary_label,
            values->output_row_secondary_text_size,
            REACH_TEXT_WEIGHT_NORMAL, 0,
            reach_quick_settings_color_alpha(theme->light_text, 0.56f));
    }

    if (device->is_default)
    {
        reach_render_command check = {};
        check.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        check.rect = layout->checkmark;
        check.icon_id = REACH_VECTOR_ICON_CHECK;
        check.color = theme->icon_backplate_background;
        (void)reach_render_command_buffer_push(commands, &check);
    }

    if (row_index + 1 < row_count)
    {
        reach_quick_settings_push_rounded_rect(
            commands, layout->separator, 0.0f,
            reach_quick_settings_color_alpha(theme->light_text, 0.16f));
    }

    return REACH_OK;
}

static reach_result reach_quick_settings_push_app_volume_row_commands(
    const reach_audio_volume_session *session,
    const reach_quick_settings_app_volume_row_layout *layout, size_t row_index, size_t row_count,
    const reach_theme *theme, reach_render_command_buffer *commands,
    const reach_quick_settings_metrics *metrics)
{
    if (session == nullptr || layout == nullptr || theme == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    float volume = reach_quick_settings_clamp01(session->level);

    reach_render_command icon = {};
    icon.rect = layout->app_icon;
    icon.color = theme->icon_backplate_background;
    if (session->icon_id != 0)
    {
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.icon_id = session->icon_id;
    }
    else
    {
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.icon_id = REACH_VECTOR_ICON_SETTINGS;
    }
    (void)reach_render_command_buffer_push(commands, &icon);

    uint16_t display_label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY] = {};
    reach_quick_settings_copy_display_label(
        display_label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, session->label);
    reach_quick_settings_capitalize_first_utf16(display_label);

    reach_quick_settings_push_ellipsized_text(commands, layout->app_label, display_label,
                                              values->app_row_text_size,
                                              REACH_TEXT_WEIGHT_NORMAL, 0, theme->light_text);

    reach_color line_color = reach_quick_settings_color_alpha(theme->light_text, 0.32f);
    reach_color level_color =
        reach_quick_settings_color_alpha(theme->light_text, session->muted ? 0.42f : 0.92f);

    reach_quick_settings_push_rounded_rect(commands, layout->slider_full_range_line,
                                           layout->slider_full_range_line.height * 0.5f,
                                           line_color);

    reach_rect_f32 level_line = layout->slider_full_range_line;
    level_line.width = level_line.width * volume;
    if (level_line.width > 0.0f)
    {
        reach_quick_settings_push_rounded_rect(commands, level_line, level_line.height * 0.5f,
                                               level_color);
    }

    reach_rect_f32 thumb = layout->slider_thumb;
    thumb.x = layout->slider_full_range_line.x + layout->slider_full_range_line.width * volume -
              thumb.width * 0.5f;
    reach_quick_settings_push_rounded_rect(commands, thumb, thumb.width * 0.5f, level_color);

    uint16_t percent_text[8] = {};
    reach_quick_settings_format_percent(percent_text,
                                        sizeof(percent_text) / sizeof(percent_text[0]), volume);
    reach_quick_settings_push_text(commands, layout->app_volume_percent, percent_text,
                                   values->app_row_percent_text_size,
                                   REACH_TEXT_WEIGHT_SEMIBOLD, 2, theme->light_text);

    if (row_index + 1 < row_count)
    {
        reach_quick_settings_push_rounded_rect(
            commands, layout->separator, 0.0f,
            reach_quick_settings_color_alpha(theme->light_text, 0.16f));
    }

    return REACH_OK;
}

reach_result
reach_quick_settings_build_render_commands(const reach_quick_settings_render_input *input,
                                           reach_render_command_buffer *commands)
{
    if (input == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_render_command_buffer_clear(commands);
    reach_quick_settings_metrics metrics =
        reach_quick_settings_metrics_for_scale(input->dpi_scale);
    float popup_radius = reach_popup_radius_scaled(input->dpi_scale);

    uint16_t network_label[REACH_SYSTEM_NETWORK_LABEL_CAPACITY] = {};
    reach_quick_settings_network_label(&input->model.network, network_label,
                                       REACH_SYSTEM_NETWORK_LABEL_CAPACITY);
    reach_quick_settings_push_system_tile_commands(
        commands, &input->layout.network_tile,
        reach_quick_settings_network_icon_id(&input->model.network), network_label,
        input->model.network.connected, &input->theme, &metrics, input->dpi_scale);

    static const uint16_t bluetooth_label[] = {'B', 'l', 'u', 'e', 't', 'o', 'o', 't', 'h', 0};
    int32_t bluetooth_enabled = input->model.bluetooth_pending
                                    ? input->model.bluetooth_pending_enabled
                                    : input->model.bluetooth.enabled;
    reach_quick_settings_push_system_tile_commands(
        commands, &input->layout.bluetooth_tile,
        bluetooth_enabled ? REACH_VECTOR_ICON_BLUETOOTH_ON : REACH_VECTOR_ICON_BLUETOOTH_OFF,
        bluetooth_label, bluetooth_enabled, &input->theme, &metrics, input->dpi_scale);

    if (input->model.power.has_battery)
    {
        static const uint16_t battery_saver_label[] = {'B', 'a', 't', 't', 'e', 'r', 'y',
                                                       ' ', 's', 'a', 'v', 'e', 'r', 0};
        reach_quick_settings_push_system_tile_commands(
            commands, &input->layout.battery_saver_tile, REACH_VECTOR_ICON_BATTERY_SAVER,
            battery_saver_label, input->model.power.battery_saver_on, &input->theme, &metrics,
            input->dpi_scale);
    }

    static const uint16_t project_label[] = {'P', 'r', 'o', 'j', 'e', 'c', 't', 0};
    reach_quick_settings_push_system_tile_commands(commands, &input->layout.project_tile,
                                                   REACH_VECTOR_ICON_PROJECT, project_label, 0,
                                                   &input->theme, &metrics, input->dpi_scale);

    if (input->model.brightness.available)
    {
        static const uint16_t brightness_label[] = {'B', 'r', 'i', 'g', 'h', 't',
                                                    'n', 'e', 's', 's', 0};
        reach_quick_settings_volume_pill_model brightness_model = {};
        reach_quick_settings_volume_pill_model_init(
            &brightness_model, input->model.brightness.level, 0, brightness_label);
        brightness_model.icon_id = REACH_VECTOR_ICON_BRIGHTNESS;

        reach_result brightness_result = reach_quick_settings_push_volume_pill_commands_with_label(
            &brightness_model, &input->layout.brightness_pill, &input->theme, commands,
            metrics.default_pill_label_text_size, 0, &metrics, input->dpi_scale);
        if (brightness_result != REACH_OK)
        {
            return brightness_result;
        }
    }

    static const uint16_t master_volume_label[] = {'M', 'a', 's', 't', 'e', 'r', ' ',
                                                   'v', 'o', 'l', 'u', 'm', 'e', 0};

    reach_quick_settings_volume_pill_model pill_model = {};
    reach_quick_settings_volume_pill_model_init(&pill_model, input->model.main_volume_level,
                                                input->model.main_muted, master_volume_label);

    reach_result result = reach_quick_settings_push_volume_pill_commands_with_label(
        &pill_model, &input->layout.main_volume_pill, &input->theme, commands,
        metrics.master_pill_label_text_size, 1, &metrics, input->dpi_scale);
    if (result != REACH_OK)
    {
        return result;
    }

    const reach_audio_output_device *current_device =
        reach_quick_settings_current_output_device(&input->model);
    static const uint16_t output_device_fallback_label[] = {'O', 'u', 't', 'p', 'u', 't', ' ',
                                                            'd', 'e', 'v', 'i', 'c', 'e', 0};

    if (!input->model.output_devices_expanded)
    {
        reach_quick_settings_push_rounded_rect(commands, input->layout.output_device_button,
                                               popup_radius,
                                               input->theme.quick_settings_button_color);

        reach_quick_settings_push_output_icon(
            commands, input->layout.output_device_button_icon,
            current_device != nullptr ? current_device->icon_id : 0, &input->theme);

        static const uint16_t output_title_label[] = {'O', 'u', 't', 'p', 'u', 't', 0};

        uint16_t output_device_label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
        reach_quick_settings_copy_device_primary_label(
            output_device_label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
            current_device != nullptr ? current_device->label : output_device_fallback_label);

        reach_rect_f32 output_title_rect = input->layout.output_device_button_label;
        output_title_rect.y += metrics.output_button_title_top;
        output_title_rect.height = metrics.output_button_title_height;

        reach_quick_settings_push_text(
            commands, output_title_rect, output_title_label,
            metrics.output_button_title_text_size,
            REACH_TEXT_WEIGHT_NORMAL, 0,
            reach_quick_settings_color_alpha(input->theme.light_text, 0.60f));

        reach_rect_f32 output_device_rect = input->layout.output_device_button_label;
        output_device_rect.y += metrics.output_button_device_top;
        output_device_rect.height = metrics.output_button_device_height;

        reach_quick_settings_push_text(
            commands, output_device_rect, output_device_label,
            metrics.output_button_device_text_size,
            REACH_TEXT_WEIGHT_SEMIBOLD, 0, input->theme.light_text);

        reach_render_command output_chevron = {};
        output_chevron.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        output_chevron.rect = input->layout.output_device_button_chevron;
        output_chevron.icon_id = REACH_VECTOR_ICON_ARROW_DOWN;
        output_chevron.color = input->theme.icon_backplate_background;
        (void)reach_render_command_buffer_push(commands, &output_chevron);
    }
    else
    {
        static const uint16_t output_devices_title[] = {'O', 'u', 't', 'p', 'u', 't', ' ', 'd',
                                                        'e', 'v', 'i', 'c', 'e', 's', 0};

        reach_quick_settings_push_text(commands, input->layout.output_devices_title,
                                       output_devices_title,
                                       metrics.section_title_text_size,
                                       REACH_TEXT_WEIGHT_SEMIBOLD, 0, input->theme.light_text);

        reach_render_command output_chevron = {};
        output_chevron.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        output_chevron.rect = input->layout.output_devices_title_chevron;
        output_chevron.icon_id = REACH_VECTOR_ICON_ARROW_UP;
        output_chevron.color = input->theme.icon_backplate_background;
        (void)reach_render_command_buffer_push(commands, &output_chevron);

        reach_quick_settings_push_rounded_rect(commands, input->layout.output_devices_panel,
                                               popup_radius,
                                               input->theme.quick_settings_button_color);

        for (size_t index = 0; index < input->layout.output_device_row_count &&
                               index < input->model.output_devices.count;
             ++index)
        {
            result = reach_quick_settings_push_output_device_row_commands(
                &input->model.output_devices.devices[index],
                &input->layout.output_device_rows[index], index,
                input->layout.output_device_row_count, &input->theme, commands, &metrics);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }

    if (input->model.expanded)
    {
        static const uint16_t app_volumes_title[] = {'A', 'p', 'p', ' ', 'v', 'o',
                                                     'l', 'u', 'm', 'e', 's', 0};
        reach_quick_settings_push_text(commands, input->layout.app_volumes_title, app_volumes_title,
                                       metrics.section_title_text_size,
                                       REACH_TEXT_WEIGHT_SEMIBOLD, 0, input->theme.light_text);

        reach_quick_settings_push_rounded_rect(commands, input->layout.app_volumes_panel,
                                               popup_radius,
                                               input->theme.quick_settings_button_color);

        for (size_t index = 0;
             index < input->layout.app_volume_row_count && index < input->model.sessions.count;
             ++index)
        {
            result = reach_quick_settings_push_app_volume_row_commands(
                &input->model.sessions.sessions[index], &input->layout.app_volume_rows[index],
                index, input->layout.app_volume_row_count + 1, &input->theme, commands, &metrics);
            if (result != REACH_OK)
            {
                return result;
            }
        }
    }

    if (!input->model.expanded)
    {
        reach_quick_settings_push_rounded_rect(commands, input->layout.expand_button,
                                               popup_radius,
                                               input->theme.quick_settings_button_color);
    }

    static const uint16_t expand_label[] = {'A', 'l', 'l', ' ', 'v', 'o', 'l', 'u', 'm', 'e',
                                            ' ', 's', 'l', 'i', 'd', 'e', 'r', 's', 0};
    static const uint16_t collapse_label[] = {'H', 'i', 'd', 'e', ' ', 'a', 'p', 'p', ' ',
                                              'v', 'o', 'l', 'u', 'm', 'e', 's', 0};

    reach_quick_settings_push_text(commands, input->layout.expand_button_label,
                                   input->model.expanded ? collapse_label : expand_label,
                                   metrics.expand_button_text_size,
                                   REACH_TEXT_WEIGHT_NORMAL, 0, input->theme.light_text);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = input->layout.expand_button_icon;
    icon.icon_id =
        input->model.expanded ? REACH_VECTOR_ICON_ARROW_UP : REACH_VECTOR_ICON_ARROW_DOWN;
    icon.color = input->theme.icon_backplate_background;
    (void)reach_render_command_buffer_push(commands, &icon);

    return REACH_OK;
}
