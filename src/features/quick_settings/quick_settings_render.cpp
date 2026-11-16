#include "reach/features/quick_settings.h"
#include "reach/features/common/popup.h"
#include "reach/features/common/progress_bar_render.h"

#include "quick_settings_common.h"
#include "reach/features/common/ui_controls.h"

#include "reach/features/common/level_presentation.h"
#include "quick_settings_metrics.h"

#include <math.h>

static void reach_quick_settings_push_rounded_rect(reach_render_command_buffer *commands,
                                                   reach_rect_f32 rect, float radius,
                                                   reach_color color);

static void reach_quick_settings_push_text(reach_render_command_buffer *commands,
                                           reach_rect_f32 rect, const uint16_t *text, float size,
                                           int32_t weight, int32_t alignment, reach_color color);
static void reach_quick_settings_push_fitted_text(reach_render_command_buffer *commands,
                                                  reach_rect_f32 rect, const uint16_t *text,
                                                  float size, int32_t weight, int32_t alignment,
                                                  reach_color color,
                                                  const reach_text_measure_port *text_measure);

static reach_result reach_quick_settings_retain_part(reach_render_command_buffer *commands,
                                                     size_t first, uint64_t id, uint64_t parent,
                                                     const reach_motion *opacity)
{
    reach_motion y = {};
    return reach_render_retain_commands(commands, first, id, parent, opacity, &y);
}

static reach_result
reach_quick_settings_push_chevron_crossfade(reach_render_command_buffer *commands,
                                            reach_rect_f32 rect, const reach_motion *progress,
                                            reach_color color, uint64_t parent, uint64_t id)
{
    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = rect;
    icon.color = color;
    reach_motion opacity = reach_motion_affine(progress, -1.0f, 1.0f);
    size_t first = commands->count;
    icon.icon_id = REACH_VECTOR_ICON_ARROW_DOWN;
    reach_result result = reach_render_command_buffer_push(commands, &icon);
    if (result != REACH_OK)
        return result;
    result = reach_quick_settings_retain_part(commands, first, id, parent, &opacity);
    if (result != REACH_OK)
        return result;
    first = commands->count;
    icon.icon_id = REACH_VECTOR_ICON_ARROW_UP;
    result = reach_render_command_buffer_push(commands, &icon);
    return result != REACH_OK
               ? result
               : reach_quick_settings_retain_part(commands, first, id + 1, parent, progress);
}

static reach_result reach_quick_settings_declare_group(
    reach_render_command_buffer *commands, uint64_t id, uint64_t parent, reach_rect_f32 bounds,
    reach_rect_f32 clip, const reach_motion *y, const reach_motion *bottom,
    const reach_motion *opacity, bool clipped = true, float clip_radius = 0.0f)
{
    reach_render_group group = {};
    group.id = id;
    group.parent_id = parent;
    group.bounds = bounds;
    group.bounds.height = fmaxf(1.0f, bounds.height);
    group.has_clip = clipped ? 1 : 0;
    group.clip = clip;
    group.clip_radius = clip_radius;
    group.clip_uniform_radius = clip_radius > 0.0f ? 1 : 0;
    if (y != nullptr)
        group.y = *y;
    if (bottom != nullptr)
        group.clip_insets[REACH_RENDER_CLIP_BOTTOM] = *bottom;
    if (opacity != nullptr)
        group.opacity = *opacity;
    reach_result result = reach_render_command_buffer_begin_group(commands, &group);
    reach_render_command_buffer_end_group(commands);
    return result;
}

enum
{
    REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS = 6,
    REACH_QUICK_SETTINGS_APP_ROW_GROUPS = 8
};

static reach_rect_f32 reach_quick_settings_section_frame(reach_rect_f32 panel,
                                                         const reach_motion *span, float gap)
{
    float minimum = 0, maximum = 0;
    if (reach_motion_range(span, &minimum, &maximum) == REACH_OK &&
        maximum - gap > panel.height + 0.5f)
        panel.height = ceilf(maximum - gap);
    return panel;
}

static float reach_quick_settings_pill_radius(reach_rect_f32 rect, const reach_theme *theme)
{
    const float half_height = rect.height * 0.5f;
    const float radius_large = theme != nullptr ? theme->radius_large : 0.0f;
    return half_height < radius_large ? half_height : radius_large;
}

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
    return reach_wifi_signal_icon(state->signal_strength);
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
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_push_fitted_text(reach_render_command_buffer *commands,
                                                  reach_rect_f32 rect, const uint16_t *text,
                                                  float size, int32_t weight, int32_t alignment,
                                                  reach_color color,
                                                  const reach_text_measure_port *text_measure)
{
    reach_render_command command = {};
    command.type = REACH_RENDER_COMMAND_TEXT;
    command.rect = rect;
    command.text_size = size;
    command.text_weight = weight;
    command.text_alignment = alignment;
    command.text_ellipsis =
        reach_text_width_or_estimate(text_measure, text, size, weight, 0.62f) > rect.width;
    command.color = color;
    reach_copy_utf16(command.text, 260, text);
    (void)reach_render_command_buffer_push(commands, &command);
}

static void reach_quick_settings_toggle_text(const reach_quick_settings_model *model, size_t toggle,
                                             uint16_t *out_title, uint16_t *out_status,
                                             uint32_t *out_icon)
{
    static const uint16_t on[] = {'O', 'n', 0};
    static const uint16_t off[] = {'O', 'f', 'f', 0};
    static const uint16_t unavailable[] = {'U', 'n', 'a', 'v', 'a', 'i',
                                           'l', 'a', 'b', 'l', 'e', 0};
    static const uint16_t wifi[] = {'W', 'i', '-', 'F', 'i', 0};
    static const uint16_t ethernet[] = {'E', 't', 'h', 'e', 'r', 'n', 'e', 't', 0};
    static const uint16_t bluetooth[] = {'B', 'l', 'u', 'e', 't', 'o', 'o', 't', 'h', 0};
    static const uint16_t dark_mode[] = {'D', 'a', 'r', 'k', ' ', 'm', 'o', 'd', 'e', 0};
    static const uint16_t project[] = {'P', 'r', 'o', 'j', 'e', 'c', 't', 0};
    static const uint16_t displays[] = {'D', 'i', 's', 'p', 'l', 'a', 'y', 's', 0};

    const uint16_t *title = wifi;
    const uint16_t *status = off;
    switch (toggle)
    {
    case REACH_QUICK_SETTINGS_TOGGLE_WIFI:
        *out_icon = reach_quick_settings_network_icon_id(&model->network);
        if (model->wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE)
        {
            status = model->network.connected && model->network.kind == REACH_NETWORK_KIND_ETHERNET
                         ? ethernet
                         : unavailable;
        }
        else if (model->wifi_radio == REACH_WIFI_RADIO_ON)
        {
            status = model->network.connected && model->network.kind == REACH_NETWORK_KIND_WIFI &&
                             model->network.label[0] != 0
                         ? model->network.label
                         : on;
            if (!model->network.connected || model->network.kind != REACH_NETWORK_KIND_WIFI)
            {
                *out_icon = REACH_VECTOR_ICON_WIFI_HIGH;
            }
        }
        else
        {
            *out_icon = REACH_VECTOR_ICON_NO_INTERNET;
        }
        break;
    case REACH_QUICK_SETTINGS_TOGGLE_BLUETOOTH:
    {
        int32_t enabled =
            model->bluetooth_pending ? model->bluetooth_pending_enabled : model->bluetooth.enabled;
        title = bluetooth;
        status = !model->bluetooth.available ? unavailable : enabled ? on : off;
        *out_icon = enabled ? REACH_VECTOR_ICON_BLUETOOTH_ON : REACH_VECTOR_ICON_BLUETOOTH_OFF;
        break;
    }
    case REACH_QUICK_SETTINGS_TOGGLE_DARK_MODE:
        title = dark_mode;
        status = model->light_theme ? off : on;
        *out_icon = REACH_VECTOR_ICON_ACCENT;
        break;
    default:
        title = project;
        status = displays;
        *out_icon = REACH_VECTOR_ICON_PROJECT;
        break;
    }
    reach_copy_utf16(out_title, REACH_SYSTEM_NETWORK_LABEL_CAPACITY, title);
    reach_copy_utf16(out_status, REACH_SYSTEM_NETWORK_LABEL_CAPACITY, status);
}

static reach_result reach_quick_settings_push_toggle(
    reach_render_command_buffer *commands, const reach_quick_settings_toggle_layout *layout,
    const reach_quick_settings_model *model, size_t toggle, const reach_motion *state,
    const reach_theme *theme, const reach_quick_settings_metrics *metrics,
    const reach_text_measure_port *text_measure, uint64_t group_id, uint64_t parent)
{
    float radius = layout->bounds.height * 0.5f;
    reach_render_group background = {};
    background.id = group_id;
    background.parent_id = parent;
    background.bounds = layout->bounds;
    reach_motion_set(&background.opacity, 1.0f);
    if (toggle != REACH_QUICK_SETTINGS_TOGGLE_PROJECT)
    {
        reach_ui_color_feedback_apply(&background, theme->quick_settings_button_background,
                                      theme->accent, state);
    }
    reach_result result = reach_render_command_buffer_begin_group(commands, &background);
    if (result == REACH_OK)
    {
        reach_quick_settings_push_rounded_rect(commands, layout->bounds, radius,
                                               toggle == REACH_QUICK_SETTINGS_TOGGLE_PROJECT
                                                   ? theme->quick_settings_button_background
                                                   : reach_color{1.0f, 1.0f, 1.0f, 1.0f});
    }
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
    {
        return result;
    }

    uint16_t title[REACH_SYSTEM_NETWORK_LABEL_CAPACITY] = {};
    uint16_t status[REACH_SYSTEM_NETWORK_LABEL_CAPACITY] = {};
    uint32_t icon_id = REACH_VECTOR_ICON_NONE;
    reach_quick_settings_toggle_text(model, toggle, title, status, &icon_id);

    reach_render_group content = {};
    content.id = group_id + 1;
    content.parent_id = parent;
    content.bounds = layout->bounds;
    result = reach_render_command_buffer_begin_group(commands, &content);
    if (result == REACH_OK)
    {
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = layout->icon;
        icon.icon_id = icon_id;
        icon.color = theme->primary_text;
        (void)reach_render_command_buffer_push(commands, &icon);
        reach_quick_settings_push_rounded_rect(commands, layout->divider, 0.0f,
                                               theme->quick_settings_separator);
        reach_quick_settings_push_fitted_text(
            commands, layout->title, title, metrics->toggle_title_text_size,
            REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_CENTER, theme->primary_text,
            text_measure);
        reach_quick_settings_push_fitted_text(commands, layout->status, status,
                                              metrics->toggle_status_text_size,
                                              REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_CENTER,
                                              theme->quick_settings_secondary_text, text_measure);
    }
    reach_render_command_buffer_end_group(commands);
    return result;
}

static void reach_quick_settings_push_level_pill(reach_render_command_buffer *commands,
                                                 reach_rect_f32 pill, reach_rect_f32 icon_rect,
                                                 reach_rect_f32 value_rect, float level,
                                                 int32_t muted, uint32_t icon_id,
                                                 const reach_theme *theme,
                                                 const reach_quick_settings_metrics *metrics)
{
    float radius = pill.height * 0.5f;
    reach_quick_settings_push_rounded_rect(commands, pill, radius,
                                           theme->quick_settings_slider_track);
    reach_rect_f32 fill = pill;
    fill.width = fmaxf(pill.height, pill.width * reach_quick_settings_clamp01(level));
    reach_quick_settings_push_rounded_rect(
        commands, fill, radius, muted ? theme->quick_settings_slider_muted_fill : theme->accent);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = icon_rect;
    icon.icon_id = icon_id;
    icon.color = theme->primary_text;
    (void)reach_render_command_buffer_push(commands, &icon);

    static const uint16_t muted_label[] = {'M', 'u', 't', 'e', 'd', 0};
    uint16_t value[16] = {};
    if (muted)
    {
        reach_copy_utf16(value, 16, muted_label);
    }
    else
    {
        reach_quick_settings_format_percent(value, 16, level);
    }
    reach_quick_settings_push_text(commands, value_rect, value,
                                   metrics->overview_pill_value_text_size, REACH_TEXT_WEIGHT_NORMAL,
                                   REACH_TEXT_ALIGNMENT_TRAILING, theme->primary_text);
}

static size_t reach_quick_settings_append_text(uint16_t *dst, size_t capacity, size_t length,
                                               const char *text)
{
    while (text != nullptr && *text != 0 && length + 1 < capacity)
    {
        dst[length++] = (uint16_t)(unsigned char)*text++;
    }
    dst[length] = 0;
    return length;
}

static size_t reach_quick_settings_append_unit(uint16_t *dst, size_t capacity, size_t length,
                                               int64_t value, const char *unit)
{
    char digits[24] = {};
    size_t count = 0;
    do
    {
        digits[count++] = (char)('0' + value % 10);
        value /= 10;
    } while (value > 0 && count < sizeof(digits) - 1);
    char ordered[24] = {};
    for (size_t index = 0; index < count; ++index)
    {
        ordered[index] = digits[count - 1 - index];
    }
    length = reach_quick_settings_append_text(dst, capacity, length, ordered);
    return reach_quick_settings_append_text(dst, capacity, length, unit);
}

static size_t reach_quick_settings_append_duration(uint16_t *dst, size_t capacity, size_t length,
                                                   int64_t minutes)
{
    int64_t days = minutes / (60 * 24);
    int64_t hours = minutes / 60 % 24;
    int64_t remainder = minutes % 60;
    if (days > 0)
    {
        length = reach_quick_settings_append_unit(dst, capacity, length, days, "d ");
        return reach_quick_settings_append_unit(dst, capacity, length, hours, "h");
    }
    if (hours > 0)
    {
        length = reach_quick_settings_append_unit(dst, capacity, length, hours, "h ");
    }
    return reach_quick_settings_append_unit(dst, capacity, length, remainder, "m");
}

static void reach_quick_settings_identity_detail(const reach_quick_settings_model *model,
                                                 uint16_t *dst, size_t capacity)
{
    size_t length = 0;
    dst[0] = 0;
    if (model->uptime_minutes >= 0)
    {
        length = reach_quick_settings_append_text(dst, capacity, length, "up ");
        length = reach_quick_settings_append_duration(dst, capacity, length, model->uptime_minutes);
    }
    if (model->power.has_battery && model->power.battery_percent >= 0)
    {
        if (length > 0)
        {
            length = reach_quick_settings_append_text(dst, capacity, length, " \xB7 ");
        }
        length = reach_quick_settings_append_unit(dst, capacity, length,
                                                  model->power.battery_percent, "%");
        if (!model->power.battery_charging && model->power.battery_seconds_remaining >= 0)
        {
            length = reach_quick_settings_append_text(dst, capacity, length, " \xB7 ");
            int64_t minutes = (model->power.battery_seconds_remaining + 30) / 60;
            length = reach_quick_settings_append_duration(dst, capacity, length,
                                                          minutes > 0 ? minutes : 1);
            (void)reach_quick_settings_append_text(dst, capacity, length, " left");
        }
    }
}

static reach_result reach_quick_settings_push_header(const reach_quick_settings_render_input *input,
                                                     reach_render_command_buffer *commands,
                                                     const reach_quick_settings_metrics *metrics)
{
    const reach_quick_settings_layout &layout = input->layout;
    const reach_theme &theme = input->theme;
    reach_render_group group = {};
    group.id = 40;
    group.parent_id = 8;
    group.bounds = layout.banner;
    group.bounds.height =
        layout.identity_detail.y + layout.identity_detail.height - layout.banner.y;
    reach_result result = reach_render_command_buffer_begin_group(commands, &group);
    if (result == REACH_OK)
    {
        float radius = theme.radius_large * input->dpi_scale;
        if (input->model.wallpaper_image_id != 0)
        {
            reach_render_command banner = {};
            banner.type = REACH_RENDER_COMMAND_ICON;
            banner.rect = layout.banner;
            banner.icon_id = input->model.wallpaper_image_id;
            banner.icon_crop_to_fill = 1;
            banner.radius = radius;
            banner.color.a = 1.0f;
            (void)reach_render_command_buffer_push(commands, &banner);
        }
        else
        {
            reach_quick_settings_push_rounded_rect(commands, layout.banner, radius,
                                                   theme.quick_settings_button_background);
        }

        reach_quick_settings_push_rounded_rect(commands, layout.avatar_ring,
                                               layout.avatar_ring.width * 0.5f,
                                               theme.top_bar_dropdown_background);
        if (input->model.picture_image_id != 0)
        {
            reach_render_command avatar = {};
            avatar.type = REACH_RENDER_COMMAND_ICON;
            avatar.rect = layout.avatar;
            avatar.icon_id = input->model.picture_image_id;
            avatar.icon_crop_to_fill = 1;
            avatar.radius = layout.avatar.width * 0.5f;
            avatar.color.a = 1.0f;
            (void)reach_render_command_buffer_push(commands, &avatar);
        }
        else
        {
            reach_quick_settings_push_rounded_rect(commands, layout.avatar,
                                                   layout.avatar.width * 0.5f, theme.accent);
            uint16_t initial[2] = {input->model.display_name[0], 0};
            reach_quick_settings_push_text(
                commands, layout.avatar, initial, metrics->identity_name_text_size,
                REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_CENTER, theme.primary_text);
        }

        reach_quick_settings_push_fitted_text(
            commands, layout.identity_name, input->model.display_name,
            metrics->identity_name_text_size, REACH_TEXT_WEIGHT_SEMIBOLD,
            REACH_TEXT_ALIGNMENT_LEADING, theme.primary_text, &input->text_measure);
        uint16_t detail[64] = {};
        reach_quick_settings_identity_detail(&input->model, detail, 64);
        reach_quick_settings_push_fitted_text(
            commands, layout.identity_detail, detail, metrics->identity_detail_text_size,
            REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_LEADING,
            theme.quick_settings_secondary_text, &input->text_measure);
    }
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_result reach_quick_settings_push_rail(const reach_quick_settings_render_input *input,
                                                   reach_render_command_buffer *commands)
{
    const reach_quick_settings_layout &layout = input->layout;
    const reach_theme &theme = input->theme;
    reach_render_group rail = {};
    rail.id = 30;
    rail.parent_id = 8;
    rail.bounds = layout.rail;
    reach_result result = reach_render_command_buffer_begin_group(commands, &rail);
    if (result == REACH_OK)
    {
        reach_quick_settings_push_rounded_rect(commands, layout.rail, layout.rail.width * 0.5f,
                                               theme.quick_settings_button_background);
    }
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
    {
        return result;
    }

    reach_render_group marker = {};
    marker.id = 31;
    marker.parent_id = 8;
    marker.bounds = layout.rail_items[0];
    marker.y = input->rail_marker;
    result = reach_render_command_buffer_begin_group(commands, &marker);
    if (result == REACH_OK)
    {
        reach_quick_settings_push_rounded_rect(commands, layout.rail_items[0],
                                               layout.rail_items[0].width * 0.5f, theme.accent);
    }
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
    {
        return result;
    }

    static const uint32_t icons[REACH_QUICK_SETTINGS_PAGE_COUNT] = {
        REACH_VECTOR_ICON_QUICK_SETTINGS, REACH_VECTOR_ICON_VOLUME_HIGH};
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(input->dpi_scale);
    reach_render_group glyphs = {};
    glyphs.id = 32;
    glyphs.parent_id = 8;
    glyphs.bounds = layout.rail;
    result = reach_render_command_buffer_begin_group(commands, &glyphs);
    for (size_t index = 0; result == REACH_OK && index < REACH_QUICK_SETTINGS_PAGE_COUNT; ++index)
    {
        reach_rect_f32 item = layout.rail_items[index];
        float inset = (item.width - metrics.toggle_icon_size) * 0.5f;
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = {item.x + inset, item.y + inset, metrics.toggle_icon_size,
                     metrics.toggle_icon_size};
        icon.icon_id = icons[index];
        icon.color = (size_t)input->model.page == index ? theme.primary_text
                                                        : theme.quick_settings_secondary_text;
        result = reach_render_command_buffer_push(commands, &icon);
    }
    reach_render_command_buffer_end_group(commands);
    return result;
}

static reach_result reach_quick_settings_begin_page(const reach_quick_settings_render_input *input,
                                                    reach_render_command_buffer *commands,
                                                    reach_quick_settings_page page,
                                                    uint64_t group_id)
{
    reach_render_group group = {};
    group.id = group_id;
    group.parent_id = 8;
    group.bounds = input->layout.page_bounds;
    group.bounds.height = fmaxf(1.0f, group.bounds.height);
    group.opacity = input->page_opacity[page];
    group.x = input->page_shift[page];
    return reach_render_command_buffer_begin_group(commands, &group);
}

static int32_t reach_quick_settings_page_visible(const reach_quick_settings_render_input *input,
                                                 reach_quick_settings_page page)
{
    return input->model.page == page || reach_motion_active(&input->page_opacity[page]);
}

static reach_result
reach_quick_settings_push_overview(const reach_quick_settings_render_input *input,
                                   reach_render_command_buffer *commands,
                                   const reach_quick_settings_metrics *metrics)
{
    const reach_quick_settings_layout &layout = input->layout;
    const reach_theme &theme = input->theme;
    reach_result result =
        reach_quick_settings_begin_page(input, commands, REACH_QUICK_SETTINGS_PAGE_OVERVIEW, 20);
    if (result == REACH_OK)
    {
        reach_quick_settings_push_level_pill(
            commands, layout.overview_volume, layout.overview_volume_icon,
            layout.overview_volume_value, input->model.main_volume_level, input->model.main_muted,
            reach_quick_settings_volume_icon_id(input->model.main_volume_level,
                                                input->model.main_muted),
            &theme, metrics);
        reach_quick_settings_push_rounded_rect(commands, layout.sound_page_button,
                                               layout.sound_page_button.height * 0.5f,
                                               theme.quick_settings_button_background);
        float inset = (layout.sound_page_button.width - metrics->toggle_icon_size) * 0.5f;
        reach_render_command icon = {};
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.rect = {layout.sound_page_button.x + inset, layout.sound_page_button.y + inset,
                     metrics->toggle_icon_size, metrics->toggle_icon_size};
        icon.icon_id = REACH_VECTOR_ICON_ARROW_RIGHT;
        icon.color = theme.system_glyph;
        (void)reach_render_command_buffer_push(commands, &icon);
        if (input->model.brightness.available)
        {
            reach_quick_settings_push_level_pill(
                commands, layout.overview_brightness, layout.overview_brightness_icon,
                layout.overview_brightness_value, input->model.brightness.level, 0,
                REACH_VECTOR_ICON_BRIGHTNESS, &theme, metrics);
        }
    }
    reach_render_command_buffer_end_group(commands);
    for (size_t index = 0; result == REACH_OK && index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
    {
        result = reach_quick_settings_push_toggle(
            commands, &layout.toggles[index], &input->model, index, &input->toggle_states[index],
            &theme, metrics, &input->text_measure, 600 + index * 2, 20);
    }
    return result;
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
    icon.color = theme->system_glyph;
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
    reach_render_command_buffer *commands, float label_text_size, int32_t label_centered_with_icon,
    const reach_quick_settings_metrics *metrics, float dpi_scale)
{
    if (model == nullptr || layout == nullptr || theme == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_quick_settings_metrics *values =
        metrics != nullptr ? metrics : &reach_quick_settings_metrics_values;
    float volume = reach_quick_settings_clamp01(model->volume_level);
    reach_quick_settings_volume_pill_layout pill = *layout;
    pill.slider_fill = reach_progress_bar_fill_rect(pill.slider_track, volume);

    reach_render_command icon = {};
    icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
    icon.rect = pill.header_icon;
    icon.icon_id = model->icon_id;
    icon.color = theme->system_glyph;
    (void)reach_render_command_buffer_push(commands, &icon);

    reach_rect_f32 label_rect = pill.header_label;
    if (label_centered_with_icon)
    {
        label_rect.height = label_text_size;
        label_rect.y = pill.header_icon.y + (pill.header_icon.height - label_rect.height) * 0.5f;
    }

    reach_quick_settings_push_text(commands, label_rect, model->label, label_text_size,
                                   values->pill_label_text_weight, 0, theme->primary_text);

    reach_color slider_fill_color =
        model->muted ? theme->quick_settings_slider_muted_fill : theme->accent;
    reach_rect_f32 origin = {0.0f, 0.0f, 0.0f, 0.0f};

    return reach_progress_bar_build_render_commands(pill.slider_track, pill.slider_fill, origin,
                                                    theme->quick_settings_slider_track,
                                                    slider_fill_color, commands);
}

static reach_result reach_quick_settings_push_output_device_row_commands(
    const reach_audio_output_device *device,
    const reach_quick_settings_output_device_row_layout *layout, int32_t separator,
    const reach_theme *theme, reach_render_command_buffer *commands,
    const reach_quick_settings_metrics *metrics, const reach_text_measure_port *text_measure)
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

    reach_quick_settings_push_fitted_text(
        commands, primary_rect, primary_label, values->output_row_primary_text_size,
        REACH_TEXT_WEIGHT_NORMAL, 0, theme->primary_text, text_measure);

    if (secondary_label[0] != 0)
    {
        reach_rect_f32 secondary_rect = layout->label;
        secondary_rect.y += values->output_row_secondary_top;
        secondary_rect.height = values->output_row_secondary_height;

        reach_quick_settings_push_fitted_text(
            commands, secondary_rect, secondary_label, values->output_row_secondary_text_size,
            REACH_TEXT_WEIGHT_NORMAL, 0, theme->quick_settings_secondary_text, text_measure);
    }

    if (device->is_default)
    {
        reach_render_command check = {};
        check.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        check.rect = layout->checkmark;
        check.icon_id = REACH_VECTOR_ICON_CHECK;
        check.color = theme->accent_text;
        (void)reach_render_command_buffer_push(commands, &check);
    }

    if (separator)
    {
        reach_ui_horizontal_separator_render(commands, layout->separator,
                                             theme->quick_settings_separator);
    }

    return REACH_OK;
}

static reach_result reach_quick_settings_push_app_volume_row_commands(
    const reach_audio_volume_session *session,
    const reach_quick_settings_app_volume_row_layout *layout, int32_t separator,
    const reach_theme *theme, reach_render_command_buffer *commands,
    const reach_quick_settings_metrics *metrics, const reach_text_measure_port *text_measure)
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
    icon.color = theme->system_glyph;
    if (session->icon_id != 0)
    {
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.icon_id = session->icon_id;
    }
    else
    {
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.icon_id = REACH_VECTOR_ICON_QUICK_SETTINGS;
    }
    (void)reach_render_command_buffer_push(commands, &icon);

    uint16_t display_label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY] = {};
    reach_quick_settings_copy_display_label(
        display_label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY, session->label);
    reach_quick_settings_capitalize_first_utf16(display_label);

    reach_quick_settings_push_fitted_text(commands, layout->app_label, display_label,
                                          values->app_row_text_size, REACH_TEXT_WEIGHT_NORMAL, 0,
                                          theme->primary_text, text_measure);

    reach_color line_color = theme->quick_settings_app_volume_track;
    reach_color level_color =
        session->muted ? theme->quick_settings_app_volume_muted_fill : theme->accent;

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
                                   values->app_row_percent_text_size, REACH_TEXT_WEIGHT_SEMIBOLD, 2,
                                   theme->primary_text);

    if (separator)
    {
        reach_ui_horizontal_separator_render(commands, layout->separator,
                                             theme->quick_settings_separator);
    }

    return REACH_OK;
}

static reach_rect_f32
reach_quick_settings_press_feedback_bounds(const reach_quick_settings_render_input *input)
{
    reach_rect_f32 bounds = {};
    if (input == nullptr)
    {
        return bounds;
    }
    switch (reach_quick_settings_pressable_target_kind(input->press_feedback_target))
    {
    case REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM:
    {
        size_t index = reach_quick_settings_pressable_target_index(input->press_feedback_target);
        return index < REACH_QUICK_SETTINGS_PAGE_COUNT ? input->layout.rail_items[index] : bounds;
    }
    case REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON:
        return input->layout.sound_page_button;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE:
    {
        size_t index = reach_quick_settings_pressable_target_index(input->press_feedback_target);
        return index < REACH_QUICK_SETTINGS_TOGGLE_COUNT ? input->layout.toggles[index].bounds
                                                         : bounds;
    }
    case REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_BUTTON:
        return input->layout.output_device_button;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW:
    {
        size_t index = reach_quick_settings_pressable_target_index(input->press_feedback_target);
        return index < input->layout.output_device_row_count
                   ? input->layout.output_device_rows[index].bounds
                   : bounds;
    }
    case REACH_QUICK_SETTINGS_PRESS_TARGET_EXPAND_BUTTON:
        return input->layout.expand_button;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_NONE:
    default:
        return bounds;
    }
}

static reach_result
reach_quick_settings_push_press_feedback(const reach_quick_settings_render_input *input,
                                         reach_render_command_buffer *commands)
{
    if (input == nullptr || commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_rect_f32 bounds = reach_quick_settings_press_feedback_bounds(input);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return REACH_OK;
    }
    reach_color overlay = {0.0f, 0.0f, 0.0f, 0.12f};
    auto kind = reach_quick_settings_pressable_target_kind(input->press_feedback_target);
    int32_t pill = kind == REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM ||
                   kind == REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON ||
                   kind == REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE;
    float radius =
        pill ? bounds.height * 0.5f
             : input->theme.radius_small * (input->dpi_scale > 0.0f ? input->dpi_scale : 1.0f);
    reach_render_group placement = {};
    placement.bounds = bounds;
    placement.parent_id = kind == REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW ? 6
                          : kind == REACH_QUICK_SETTINGS_PRESS_TARGET_EXPAND_BUTTON   ? 2
                          : kind == REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM       ? 8
                          : pill                                                      ? 20
                                                                                      : 21;
    return reach_ui_feedback_render(commands, 900, bounds, radius, overlay, &input->press_feedback,
                                    &placement);
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
    reach_quick_settings_metrics metrics = reach_quick_settings_metrics_for_scale(input->dpi_scale);
    reach_rect_f32 viewport =
        input->viewport.width > 0 ? input->viewport : input->layout.content_bounds;
    reach_rect_f32 body_bounds = input->render_bounds.width > 0 ? input->render_bounds : viewport;
    const reach_motion *body_y =
        input->surface_motion.height.generation != 0 ? &input->surface_motion.y : nullptr;
    if (reach_quick_settings_declare_group(commands, 7, 0, body_bounds, body_bounds, body_y,
                                           nullptr, nullptr, false) != REACH_OK)
        return REACH_ERROR;
    if (input->has_background)
    {
        reach_popup_background_input background = {};
        background.theme = &input->theme;
        background.bounds = body_bounds;
        background.notch_center_x = input->notch_center_x;
        background.notch_side = input->notch_side;
        background.dpi_scale = input->dpi_scale;
        background.background_color_override = input->theme.top_bar_dropdown_background;
        background.has_background_color_override = 1;
        background.hide_notch = 1;
        reach_result result = reach_popup_push_height_background(
            &background, &input->surface_motion.height, 7, 1700, commands);
        if (result != REACH_OK)
            return result;
    }
    reach_motion viewport_bottom = {};
    if (input->surface_motion.height.generation != 0)
        viewport_bottom =
            reach_motion_affine(&input->surface_motion.height, -1, body_bounds.height);
    reach_motion scrolling = reach_motion_affine(&input->scroll_motion, -1, 0);
    reach_motion opaque = {}, stationary = {};
    reach_motion_set(&opaque, 1);
    reach_rect_f32 output_panel = reach_quick_settings_section_frame(
        input->layout.output_devices_panel, &input->output_devices_span, metrics.output_panel_gap);
    reach_rect_f32 app_panel = reach_quick_settings_section_frame(
        input->layout.app_volumes_panel, &input->app_volumes_span, metrics.app_panel_gap);
    reach_motion output_bottom = reach_motion_affine(
        &input->output_devices_span, -1, output_panel.height + metrics.output_panel_gap);
    reach_motion app_bottom =
        reach_motion_affine(&input->app_volumes_span, -1, app_panel.height + metrics.app_panel_gap);
    float output_radius = reach_quick_settings_pill_radius(output_panel, &input->theme);
    float app_radius = reach_quick_settings_pill_radius(app_panel, &input->theme);
    if (reach_quick_settings_declare_group(commands, 1, 7, viewport, viewport, nullptr,
                                           &viewport_bottom, nullptr) != REACH_OK ||
        reach_quick_settings_declare_group(commands, 8, 1, viewport, viewport, &scrolling, nullptr,
                                           nullptr, false) != REACH_OK)
        return REACH_ERROR;
    reach_result result = reach_quick_settings_push_header(input, commands, &metrics);
    if (result == REACH_OK)
        result = reach_quick_settings_push_rail(input, commands);
    if (result == REACH_OK &&
        reach_quick_settings_page_visible(input, REACH_QUICK_SETTINGS_PAGE_OVERVIEW))
        result = reach_quick_settings_push_overview(input, commands, &metrics);
    if (result != REACH_OK)
        return result;
    if (!reach_quick_settings_page_visible(input, REACH_QUICK_SETTINGS_PAGE_SOUND))
        return reach_quick_settings_push_press_feedback(input, commands);
    result = reach_quick_settings_begin_page(input, commands, REACH_QUICK_SETTINGS_PAGE_SOUND, 21);
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK ||
        reach_quick_settings_declare_group(commands, 2, 21, viewport, viewport,
                                           &input->output_devices_span, nullptr, nullptr,
                                           false) != REACH_OK ||
        reach_quick_settings_declare_group(commands, 3, 21, output_panel, output_panel, nullptr,
                                           &output_bottom, &input->output_devices_opacity, true,
                                           output_radius) != REACH_OK ||
        reach_quick_settings_declare_group(commands, 4, 2, app_panel, app_panel, nullptr,
                                           &app_bottom, &input->app_volumes_opacity, true,
                                           app_radius) != REACH_OK ||
        reach_quick_settings_declare_group(commands, 6, 21, output_panel, output_panel, nullptr,
                                           &output_bottom, nullptr, true,
                                           output_radius) != REACH_OK)
        return REACH_ERROR;

    reach_render_group base = {};
    base.id = 5;
    base.parent_id = 21;
    if (reach_render_command_buffer_begin_group(commands, &base) != REACH_OK)
        return REACH_ERROR;

    static const uint16_t master_volume_label[] = {'M', 'a', 's', 't', 'e', 'r', ' ',
                                                   'v', 'o', 'l', 'u', 'm', 'e', 0};

    reach_quick_settings_volume_pill_model pill_model = {};
    reach_quick_settings_volume_pill_model_init(&pill_model, input->model.main_volume_level,
                                                input->model.main_muted, master_volume_label);

    result = reach_quick_settings_push_volume_pill_commands_with_label(
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

    reach_quick_settings_push_rounded_rect(
        commands, input->layout.output_device_button,
        reach_quick_settings_pill_radius(input->layout.output_device_button, &input->theme),
        input->theme.quick_settings_button_background);

    reach_quick_settings_push_output_icon(commands, input->layout.output_device_button_icon,
                                          current_device != nullptr ? current_device->icon_id : 0,
                                          &input->theme);

    static const uint16_t output_title_label[] = {'O', 'u', 't', 'p', 'u', 't', 0};

    uint16_t output_device_label[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
    reach_quick_settings_copy_device_primary_label(
        output_device_label, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
        current_device != nullptr ? current_device->label : output_device_fallback_label);

    reach_rect_f32 output_title_rect = input->layout.output_device_button_label;
    output_title_rect.y += metrics.output_button_title_top;
    output_title_rect.height = metrics.output_button_title_height;

    reach_quick_settings_push_text(commands, output_title_rect, output_title_label,
                                   metrics.output_button_title_text_size, REACH_TEXT_WEIGHT_NORMAL,
                                   0, input->theme.quick_settings_section_label);

    reach_rect_f32 output_device_rect = input->layout.output_device_button_label;
    output_device_rect.y += metrics.output_button_device_top;
    output_device_rect.height = metrics.output_button_device_height;

    reach_quick_settings_push_fitted_text(
        commands, output_device_rect, output_device_label, metrics.output_button_device_text_size,
        REACH_TEXT_WEIGHT_SEMIBOLD, 0, input->theme.primary_text, &input->text_measure);

    reach_render_command_buffer_end_group(commands);
    result = reach_quick_settings_push_chevron_crossfade(
        commands, input->layout.output_device_button_chevron, &input->output_devices_expansion,
        input->theme.system_glyph, 21, 10);
    if (result != REACH_OK)
        return result;

    if ((input->model.output_devices_expanded ||
         reach_motion_active(&input->output_devices_expansion) ||
         reach_motion_active(&input->output_devices_opacity)) &&
        output_panel.height > 0.0f)
    {
        size_t first_command = commands->count;
        reach_quick_settings_push_rounded_rect(commands, output_panel, output_radius,
                                               input->theme.quick_settings_button_background);
        result =
            reach_render_retain_commands(commands, first_command, 100, 3, &opaque, &stationary);
        for (size_t slot = 0; result == REACH_OK && slot < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
             ++slot)
        {
            const reach_keyed_row &row = input->output_device_row_motions[slot];
            const reach_quick_settings_output_device_row &item =
                input->output_device_row_items[slot];
            if (row.lifecycle == REACH_KEYED_ROW_EMPTY)
                continue;
            reach_quick_settings_output_device_row_layout layout =
                reach_quick_settings_output_device_row_layout_at(
                    output_panel, output_panel.y + row.y.to, input->dpi_scale);
            first_command = commands->count;
            result = reach_quick_settings_push_output_device_row_commands(
                &item.device, &layout, item.separator, &input->theme, commands, &metrics,
                &input->text_measure);
            REACH_ASSERT(commands->count - first_command <= REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS);
            reach_motion offset = reach_motion_affine(&row.y, 1, -row.y.to);
            if (result == REACH_OK)
                result = reach_render_retain_commands(
                    commands, first_command, 101 + slot * REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS, 3,
                    &row.opacity, &offset);
        }
        if (result != REACH_OK)
            return result;
        reach_render_command_buffer_clear_scissor(commands);
    }

    auto feedback_kind = reach_quick_settings_pressable_target_kind(input->press_feedback_target);
    if (feedback_kind == REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW)
    {
        result = reach_quick_settings_push_press_feedback(input, commands);
        if (result != REACH_OK)
            return result;
    }
    size_t first_part = commands->count;
    reach_quick_settings_push_rounded_rect(
        commands, input->layout.expand_button,
        reach_quick_settings_pill_radius(input->layout.expand_button, &input->theme),
        input->theme.quick_settings_button_background);

    result = reach_quick_settings_retain_part(commands, first_part, 200, 2, &opaque);
    if (result != REACH_OK)
        return result;
    first_part = commands->count;
    reach_motion collapsed = reach_motion_affine(&input->app_volumes_expansion, -1, 1);
    static const uint16_t expand_label[] = {'A', 'l', 'l', ' ', 'v', 'o', 'l', 'u', 'm', 'e',
                                            ' ', 's', 'l', 'i', 'd', 'e', 'r', 's', 0};
    static const uint16_t collapse_label[] = {'H', 'i', 'd', 'e', ' ', 'a', 'p', 'p', ' ',
                                              'v', 'o', 'l', 'u', 'm', 'e', 's', 0};

    reach_quick_settings_push_text(commands, input->layout.expand_button_label, expand_label,
                                   metrics.expand_button_text_size, REACH_TEXT_WEIGHT_NORMAL, 0,
                                   input->theme.primary_text);
    result = reach_quick_settings_retain_part(commands, first_part, 201, 2, &collapsed);
    if (result != REACH_OK)
        return result;
    first_part = commands->count;
    reach_quick_settings_push_text(commands, input->layout.expand_button_label, collapse_label,
                                   metrics.expand_button_text_size, REACH_TEXT_WEIGHT_NORMAL, 0,
                                   input->theme.primary_text);
    result = reach_quick_settings_retain_part(commands, first_part, 202, 2,
                                              &input->app_volumes_expansion);
    if (result != REACH_OK)
        return result;

    result = reach_quick_settings_push_chevron_crossfade(commands, input->layout.expand_button_icon,
                                                         &input->app_volumes_expansion,
                                                         input->theme.system_glyph, 2, 203);
    if (result != REACH_OK)
        return result;

    if ((input->model.expanded || reach_motion_active(&input->app_volumes_expansion) ||
         reach_motion_active(&input->app_volumes_opacity)) &&
        app_panel.height > 0.0f)
    {
        size_t first_command = commands->count;
        reach_quick_settings_push_rounded_rect(commands, app_panel, app_radius,
                                               input->theme.quick_settings_button_background);
        result =
            reach_render_retain_commands(commands, first_command, 300, 4, &opaque, &stationary);
        for (size_t slot = 0; result == REACH_OK && slot < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++slot)
        {
            const reach_keyed_row &row = input->app_volume_row_motions[slot];
            const reach_quick_settings_app_volume_row &item = input->app_volume_row_items[slot];
            if (row.lifecycle == REACH_KEYED_ROW_EMPTY)
                continue;
            reach_quick_settings_app_volume_row_layout layout =
                reach_quick_settings_app_volume_row_layout_at(app_panel, app_panel.y + row.y.to,
                                                              item.session.level, input->dpi_scale);
            first_command = commands->count;
            result = reach_quick_settings_push_app_volume_row_commands(
                &item.session, &layout, item.separator, &input->theme, commands, &metrics,
                &input->text_measure);
            REACH_ASSERT(commands->count - first_command <= REACH_QUICK_SETTINGS_APP_ROW_GROUPS);
            reach_motion offset = reach_motion_affine(&row.y, 1, -row.y.to);
            if (result == REACH_OK)
                result = reach_render_retain_commands(
                    commands, first_command, 301 + slot * REACH_QUICK_SETTINGS_APP_ROW_GROUPS, 4,
                    &row.opacity, &offset);
        }
        if (result != REACH_OK)
            return result;
        reach_render_command_buffer_clear_scissor(commands);
    }

    return feedback_kind == REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW
               ? REACH_OK
               : reach_quick_settings_push_press_feedback(input, commands);
}

reach_result reach_quick_settings_append_render_commands(
    reach_quick_settings *quick_settings, const reach_theme *theme, float dpi_scale,
    const reach_text_measure_port *text_measure, reach_render_command_buffer *out_commands)
{
    if (quick_settings == nullptr || theme == nullptr || out_commands == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_quick_settings_state *state = reach_quick_settings_state_mut(quick_settings);

    reach_quick_settings_render_input input = {};
    input.theme = *theme;
    input.model = state->model;
    reach_rect_f32 source_bounds = reach_quick_settings_content_bounds_for(
        {0, 0, state->render_bounds.width, state->render_bounds.height}, theme, dpi_scale,
        state->drop_direction);
    input.layout = reach_quick_settings_layout_for_sections_scaled(source_bounds, theme,
                                                                   &state->model, 0, 0, dpi_scale);
    input.viewport = source_bounds;
    input.surface_motion = state->surface_motion;
    input.scroll_motion = state->scroll_motion;
    input.render_bounds = {0, 0, state->render_bounds.width, state->render_bounds.height};
    input.has_background = 1;
    input.notch_center_x = state->notch_anchor_x - state->render_bounds.x;
    input.notch_side = reach_popup_notch_side(state->drop_direction);
    input.dpi_scale = dpi_scale;
    input.text_measure = text_measure != nullptr ? *text_measure : reach_text_measure_port{};
    input.output_devices_expansion = state->output_devices_expansion;
    input.app_volumes_expansion = state->app_volumes_expansion;
    input.output_devices_span = state->output_devices_span;
    input.app_volumes_span = state->app_volumes_span;
    input.output_devices_opacity = state->output_devices_opacity;
    input.app_volumes_opacity = state->app_volumes_opacity;
    reach_quick_settings_row_motions(quick_settings, input.output_device_row_motions,
                                     input.output_device_row_items, input.app_volume_row_motions,
                                     input.app_volume_row_items);
    reach_quick_settings_page_motions(quick_settings, input.toggle_states, input.page_opacity,
                                      input.page_shift, &input.rail_marker);
    input.press_feedback_target = reach_quick_settings_press_feedback_target(quick_settings);
    input.press_feedback = reach_quick_settings_press_feedback_motion(quick_settings);

    reach_result result = reach_quick_settings_build_render_commands(&input, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    return REACH_OK;
}
