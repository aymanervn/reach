#include "reach/features/quick_settings.h"
#include "reach/features/common/popup.h"
#include "reach/features/common/progress_bar_render.h"

#include "quick_settings_common.h"
#include "reach/features/common/ui_controls.h"

#include "reach/features/common/level_presentation.h"
#include "reach/features/common/wifi_session.h"
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
        REACH_VECTOR_ICON_QUICK_SETTINGS, REACH_VECTOR_ICON_VOLUME_HIGH,
        REACH_VECTOR_ICON_WIFI_HIGH};
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

enum reach_quick_settings_feedback_slot
{
    REACH_QUICK_SETTINGS_FEEDBACK_OVERVIEW = 0,
    REACH_QUICK_SETTINGS_FEEDBACK_SOUND_TOP,
    REACH_QUICK_SETTINGS_FEEDBACK_OUTPUTS,
    REACH_QUICK_SETTINGS_FEEDBACK_APPS,
    REACH_QUICK_SETTINGS_FEEDBACK_INPUT,
    REACH_QUICK_SETTINGS_FEEDBACK_WIFI,
    REACH_QUICK_SETTINGS_FEEDBACK_TRAILING
};

enum
{
    REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS = 6,
    REACH_QUICK_SETTINGS_APP_ROW_GROUPS = 6
};

static reach_result
reach_quick_settings_push_press_feedback(const reach_quick_settings_render_input *input,
                                         reach_render_command_buffer *commands, int32_t slot);

static reach_rect_f32 reach_quick_settings_section_frame(reach_rect_f32 label, float full_span,
                                                         const reach_motion *span)
{
    float minimum = 0, maximum = 0;
    float height = full_span;
    if (reach_motion_range(span, &minimum, &maximum) == REACH_OK && maximum > height)
        height = maximum;
    reach_rect_f32 frame = label;
    frame.height = fmaxf(1.0f, ceilf(height));
    return frame;
}

static void reach_quick_settings_push_pill_icon(reach_render_command_buffer *commands,
                                                reach_rect_f32 rect, uint32_t vector_icon,
                                                uint64_t image_icon, const reach_theme *theme)
{
    reach_render_command icon = {};
    icon.rect = rect;
    icon.color = theme->primary_text;
    if (image_icon != 0)
    {
        icon.type = REACH_RENDER_COMMAND_ICON;
        icon.icon_id = image_icon;
        icon.color.a = 1.0f;
    }
    else
    {
        icon.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        icon.icon_id = vector_icon;
    }
    (void)reach_render_command_buffer_push(commands, &icon);
}

static void reach_quick_settings_push_pill(reach_render_command_buffer *commands,
                                           const reach_quick_settings_level_pill_layout *layout,
                                           float level, int32_t muted, uint32_t vector_icon,
                                           uint64_t image_icon, const uint16_t *label,
                                           const reach_theme *theme,
                                           const reach_quick_settings_metrics *metrics,
                                           const reach_text_measure_port *text_measure)
{
    reach_rect_f32 pill = layout->bounds;
    float radius = pill.height * 0.5f;
    reach_quick_settings_push_rounded_rect(commands, pill, radius,
                                           theme->quick_settings_slider_track);
    reach_rect_f32 fill = pill;
    fill.width = fmaxf(pill.height, pill.width * reach_quick_settings_clamp01(level));
    reach_quick_settings_push_rounded_rect(
        commands, fill, radius, muted ? theme->quick_settings_slider_muted_fill : theme->accent);
    reach_quick_settings_push_pill_icon(commands, layout->icon, vector_icon, image_icon, theme);
    if (label != nullptr && label[0] != 0 && layout->label.width > 0.0f)
    {
        reach_quick_settings_push_fitted_text(
            commands, layout->label, label, metrics->pill_label_text_size,
            REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_LEADING, theme->primary_text,
            text_measure);
    }
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
    reach_quick_settings_push_text(commands, layout->value, value,
                                   metrics->overview_pill_value_text_size, REACH_TEXT_WEIGHT_NORMAL,
                                   REACH_TEXT_ALIGNMENT_TRAILING, theme->primary_text);
}

static void reach_quick_settings_push_section_label(reach_render_command_buffer *commands,
                                                    reach_rect_f32 rect, const uint16_t *text,
                                                    const reach_theme *theme,
                                                    const reach_quick_settings_metrics *metrics)
{
    reach_rect_f32 inset = rect;
    inset.x += metrics->overview_pill_icon_inset;
    inset.width = fmaxf(0.0f, inset.width - metrics->overview_pill_icon_inset);
    reach_quick_settings_push_text(commands, inset, text, metrics->section_label_text_size,
                                   REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_LEADING,
                                   theme->quick_settings_section_label);
}

static void reach_quick_settings_push_output_row(
    reach_render_command_buffer *commands, const reach_audio_output_device *device,
    const reach_quick_settings_output_device_row_layout *layout, const reach_theme *theme,
    const reach_quick_settings_metrics *metrics, const reach_text_measure_port *text_measure)
{
    if (device->is_default)
    {
        reach_quick_settings_push_rounded_rect(commands, layout->bounds,
                                               layout->bounds.height * 0.5f, theme->accent);
    }
    reach_quick_settings_push_pill_icon(commands, layout->icon, REACH_VECTOR_ICON_VOLUME_HIGH,
                                        device->icon_id, theme);
    uint16_t title[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
    uint16_t detail[REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY] = {};
    reach_quick_settings_copy_device_primary_label(title, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY,
                                                   device->label);
    reach_quick_settings_copy_device_secondary_label(
        detail, REACH_AUDIO_VOLUME_DEVICE_LABEL_CAPACITY, device->label);
    reach_quick_settings_push_fitted_text(commands, layout->title, title,
                                          metrics->output_row_title_text_size,
                                          REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_LEADING,
                                          theme->primary_text, text_measure);
    reach_quick_settings_push_fitted_text(commands, layout->detail, detail,
                                          metrics->output_row_detail_text_size,
                                          REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_LEADING,
                                          theme->quick_settings_secondary_text, text_measure);
    if (device->is_default)
    {
        reach_render_command check = {};
        check.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        check.rect = layout->checkmark;
        check.icon_id = REACH_VECTOR_ICON_CHECK;
        check.color = theme->primary_text;
        (void)reach_render_command_buffer_push(commands, &check);
    }
}

static reach_result reach_quick_settings_push_sound(const reach_quick_settings_render_input *input,
                                                    reach_render_command_buffer *commands,
                                                    const reach_quick_settings_metrics *metrics)
{
    const reach_quick_settings_layout &layout = input->layout;
    const reach_theme &theme = input->theme;
    float output_full = 0, app_full = 0;
    if (layout.output_device_row_count > 0)
        output_full = metrics->section_label_height +
                      (float)layout.output_device_row_count * metrics->output_device_row_height;
    if (layout.app_volume_row_count > 0)
        app_full = metrics->section_label_height +
                   (float)layout.app_volume_row_count * metrics->app_volume_row_height;
    reach_rect_f32 output_frame = reach_quick_settings_section_frame(
        layout.output_label, output_full, &input->output_devices_span);
    reach_rect_f32 app_frame =
        reach_quick_settings_section_frame(layout.app_label, app_full, &input->app_volumes_span);
    reach_motion output_bottom =
        reach_motion_affine(&input->output_devices_span, -1, output_frame.height);
    reach_motion app_bottom = reach_motion_affine(&input->app_volumes_span, -1, app_frame.height);

    reach_result result =
        reach_quick_settings_begin_page(input, commands, REACH_QUICK_SETTINGS_PAGE_SOUND, 21);
    if (result == REACH_OK)
    {
        reach_quick_settings_push_pill(commands, &layout.sound_volume,
                                       input->model.main_volume_level, input->model.main_muted,
                                       reach_quick_settings_volume_icon_id(
                                           input->model.main_volume_level, input->model.main_muted),
                                       0, nullptr, &theme, metrics, &input->text_measure);
    }
    reach_render_command_buffer_end_group(commands);
    if (result == REACH_OK)
        result = reach_quick_settings_push_press_feedback(input, commands,
                                                          REACH_QUICK_SETTINGS_FEEDBACK_SOUND_TOP);
    if (result != REACH_OK)
        return result;

    reach_render_group output_group = {};
    output_group.id = 3;
    output_group.parent_id = 21;
    output_group.bounds = output_frame;
    output_group.has_clip = 1;
    output_group.clip = output_frame;
    output_group.clip_insets[REACH_RENDER_CLIP_BOTTOM] = output_bottom;
    result = reach_render_command_buffer_begin_group(commands, &output_group);
    if (result == REACH_OK && layout.output_device_row_count > 0)
    {
        static const uint16_t output_label[] = {'O', 'u', 't', 'p', 'u', 't', 0};
        reach_quick_settings_push_section_label(commands, layout.output_label, output_label, &theme,
                                                metrics);
    }
    reach_render_command_buffer_end_group(commands);
    for (size_t slot = 0; result == REACH_OK && slot < REACH_AUDIO_VOLUME_MAX_OUTPUT_DEVICES;
         ++slot)
    {
        const reach_keyed_row &row = input->output_device_row_motions[slot];
        if (row.lifecycle == REACH_KEYED_ROW_EMPTY)
            continue;
        reach_quick_settings_output_device_row_layout row_layout =
            reach_quick_settings_output_device_row_layout_at(
                layout.output_devices_panel, layout.output_devices_panel.y + row.y.to,
                input->dpi_scale);
        size_t first_command = commands->count;
        reach_quick_settings_push_output_row(commands, &input->output_device_row_items[slot].device,
                                             &row_layout, &theme, metrics, &input->text_measure);
        REACH_ASSERT(commands->count - first_command <= REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS);
        reach_motion offset = reach_motion_affine(&row.y, 1, -row.y.to);
        result = reach_render_retain_commands(commands, first_command,
                                              101 + slot * REACH_QUICK_SETTINGS_OUTPUT_ROW_GROUPS,
                                              3, &row.opacity, &offset);
    }
    if (result == REACH_OK)
        result = reach_quick_settings_push_press_feedback(input, commands,
                                                          REACH_QUICK_SETTINGS_FEEDBACK_OUTPUTS);
    if (result != REACH_OK || reach_quick_settings_declare_group(
                                  commands, 2, 21, input->viewport, input->viewport,
                                  &input->output_devices_span, nullptr, nullptr, false) != REACH_OK)
        return REACH_ERROR;

    reach_render_group app_group = {};
    app_group.id = 4;
    app_group.parent_id = 2;
    app_group.bounds = app_frame;
    app_group.has_clip = 1;
    app_group.clip = app_frame;
    app_group.clip_insets[REACH_RENDER_CLIP_BOTTOM] = app_bottom;
    result = reach_render_command_buffer_begin_group(commands, &app_group);
    if (result == REACH_OK && layout.app_volume_row_count > 0)
    {
        static const uint16_t apps_label[] = {'A', 'p', 'p', 's', 0};
        reach_quick_settings_push_section_label(commands, layout.app_label, apps_label, &theme,
                                                metrics);
    }
    reach_render_command_buffer_end_group(commands);
    for (size_t slot = 0; result == REACH_OK && slot < REACH_AUDIO_VOLUME_MAX_SESSIONS; ++slot)
    {
        const reach_keyed_row &row = input->app_volume_row_motions[slot];
        if (row.lifecycle == REACH_KEYED_ROW_EMPTY)
            continue;
        const reach_audio_volume_session &session = input->app_volume_row_items[slot].session;
        reach_quick_settings_level_pill_layout row_layout =
            reach_quick_settings_app_volume_row_layout_at(
                layout.app_volumes_panel, layout.app_volumes_panel.y + row.y.to, input->dpi_scale);
        uint16_t label[REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY] = {};
        reach_quick_settings_copy_display_label(label, REACH_AUDIO_VOLUME_SESSION_LABEL_CAPACITY,
                                                session.label);
        size_t first_command = commands->count;
        reach_quick_settings_push_pill(commands, &row_layout, session.level, session.muted,
                                       REACH_VECTOR_ICON_MUSIC_NOTE, session.icon_id, label, &theme,
                                       metrics, &input->text_measure);
        REACH_ASSERT(commands->count - first_command <= REACH_QUICK_SETTINGS_APP_ROW_GROUPS);
        reach_motion offset = reach_motion_affine(&row.y, 1, -row.y.to);
        result = reach_render_retain_commands(commands, first_command,
                                              301 + slot * REACH_QUICK_SETTINGS_APP_ROW_GROUPS, 4,
                                              &row.opacity, &offset);
    }
    if (result == REACH_OK)
        result = reach_quick_settings_push_press_feedback(input, commands,
                                                          REACH_QUICK_SETTINGS_FEEDBACK_APPS);
    if (result != REACH_OK || !input->model.input_available)
        return result;

    reach_render_group input_group = {};
    input_group.id = 13;
    input_group.parent_id = 2;
    input_group.bounds = layout.input_label;
    input_group.bounds.height =
        layout.input_volume.bounds.y + layout.input_volume.bounds.height - layout.input_label.y;
    input_group.y = input->app_volumes_span;
    result = reach_render_command_buffer_begin_group(commands, &input_group);
    if (result == REACH_OK)
    {
        static const uint16_t input_label[] = {'I', 'n', 'p', 'u', 't', 0};
        reach_quick_settings_push_section_label(commands, layout.input_label, input_label, &theme,
                                                metrics);
        reach_quick_settings_push_pill(commands, &layout.input_volume, input->model.input.level,
                                       input->model.input.muted, REACH_VECTOR_ICON_ACTIVITY, 0,
                                       nullptr, &theme, metrics, &input->text_measure);
    }
    reach_render_command_buffer_end_group(commands);
    if (result == REACH_OK)
        result = reach_quick_settings_push_press_feedback(input, commands,
                                                          REACH_QUICK_SETTINGS_FEEDBACK_INPUT);
    return result;
}

static void reach_quick_settings_push_wifi_row(
    reach_render_command_buffer *commands, const reach_wifi_network *network,
    const reach_quick_settings_output_device_row_layout *row,
    const reach_quick_settings_model *model, const reach_theme *theme,
    const reach_quick_settings_metrics *metrics, const reach_text_measure_port *text_measure)
{
    if (network->connected)
    {
        reach_quick_settings_push_rounded_rect(commands, row->bounds, row->bounds.height * 0.5f,
                                               theme->accent);
    }
    reach_quick_settings_push_pill_icon(commands, row->icon,
                                        reach_wifi_signal_icon(network->signal_strength), 0, theme);
    reach_quick_settings_push_fitted_text(commands, row->title, network->ssid,
                                          metrics->output_row_title_text_size,
                                          REACH_TEXT_WEIGHT_SEMIBOLD, REACH_TEXT_ALIGNMENT_LEADING,
                                          theme->primary_text, text_measure);
    const uint16_t *detail = reach_wifi_session_network_detail(network, 0);
    if (model->wifi_status != REACH_WIFI_SESSION_STATUS_IDLE &&
        model->wifi_status != REACH_WIFI_SESSION_STATUS_SCANNING &&
        reach_utf16_equal(model->wifi_status_ssid, network->ssid))
    {
        detail = reach_wifi_session_status_message((reach_wifi_session_status)model->wifi_status);
    }
    reach_quick_settings_push_fitted_text(commands, row->detail, detail,
                                          metrics->output_row_detail_text_size,
                                          REACH_TEXT_WEIGHT_NORMAL, REACH_TEXT_ALIGNMENT_LEADING,
                                          theme->quick_settings_secondary_text, text_measure);
    if (network->connected || network->security != REACH_WIFI_SECURITY_OPEN)
    {
        reach_render_command trailing = {};
        trailing.type = REACH_RENDER_COMMAND_VECTOR_ICON;
        trailing.rect = row->checkmark;
        trailing.icon_id = network->connected ? REACH_VECTOR_ICON_CHECK : REACH_VECTOR_ICON_LOCK;
        trailing.color =
            network->connected ? theme->primary_text : theme->quick_settings_secondary_text;
        (void)reach_render_command_buffer_push(commands, &trailing);
    }
}

static reach_result reach_quick_settings_push_wifi(const reach_quick_settings_render_input *input,
                                                   reach_render_command_buffer *commands,
                                                   const reach_quick_settings_metrics *metrics)
{
    const reach_quick_settings_layout &layout = input->layout;
    const reach_theme &theme = input->theme;
    const reach_quick_settings_model &model = input->model;
    reach_result result =
        reach_quick_settings_begin_page(input, commands, REACH_QUICK_SETTINGS_PAGE_WIFI, 22);
    if (result == REACH_OK)
    {
        static const uint16_t title[] = {'W', 'i', '-', 'F', 'i', 0};
        reach_quick_settings_push_text(commands, layout.wifi_title, title,
                                       metrics->wifi_title_text_size, REACH_TEXT_WEIGHT_SEMIBOLD,
                                       REACH_TEXT_ALIGNMENT_LEADING, theme.primary_text);
        static const uint16_t off[] = {'W', 'i', '-', 'F', 'i', ' ', 'i',
                                       's', ' ', 'o', 'f', 'f', 0};
        static const uint16_t unavailable[] = {'N', 'o', ' ', 'W', 'i', '-', 'F', 'i', ' ',
                                               'a', 'd', 'a', 'p', 't', 'e', 'r', 0};
        static const uint16_t searching[] = {'L', 'o', 'o', 'k', 'i',    'n', 'g', ' ',
                                             'f', 'o', 'r', ' ', 'n',    'e', 't', 'w',
                                             'o', 'r', 'k', 's', 0x2026, 0};
        static const uint16_t none[] = {'N', 'o', ' ', 'n', 'e', 't', 'w', 'o', 'r',
                                        'k', 's', ' ', 'f', 'o', 'u', 'n', 'd', 0};
        if (layout.wifi_message.height > 0.0f)
        {
            const uint16_t *message = model.wifi_radio == REACH_WIFI_RADIO_UNAVAILABLE ? unavailable
                                      : model.wifi_radio != REACH_WIFI_RADIO_ON        ? off
                                      : model.wifi_status == REACH_WIFI_SESSION_STATUS_SCANNING
                                          ? searching
                                          : none;
            reach_quick_settings_push_section_label(commands, layout.wifi_message, message, &theme,
                                                    metrics);
        }
        if (layout.wifi_nearby_label.height > 0.0f)
        {
            static const uint16_t nearby[] = {'N', 'e', 'a', 'r', 'b', 'y', 0};
            reach_quick_settings_push_section_label(
                commands, layout.wifi_nearby_label,
                model.wifi_status == REACH_WIFI_SESSION_STATUS_SCANNING ? searching : nearby,
                &theme, metrics);
        }
        if (model.wifi_radio != REACH_WIFI_RADIO_UNAVAILABLE)
        {
            reach_ui_toggle_style style = {};
            style.track_off = theme.toggle_track_off;
            style.track_on = theme.accent;
            style.knob = theme.toggle_knob;
            result = reach_ui_toggle_motion_render(commands, layout.wifi_switch, &style, 700,
                                                   &input->wifi_switch);
        }
    }
    reach_render_command_buffer_end_group(commands);
    if (result != REACH_OK)
        return result;

    float base = layout.wifi_title.y + layout.wifi_title.height + metrics->overview_row_gap;
    reach_rect_f32 panel = layout.page_bounds;
    panel.x += metrics->content_padding;
    panel.width -= metrics->content_padding * 2.0f;
    for (size_t slot = 0; result == REACH_OK && slot < REACH_QUICK_SETTINGS_WIFI_ROWS; ++slot)
    {
        const reach_keyed_row &row = input->wifi_row_motions[slot];
        if (row.lifecycle == REACH_KEYED_ROW_EMPTY)
            continue;
        reach_quick_settings_output_device_row_layout row_layout =
            reach_quick_settings_output_device_row_layout_at(panel, base + row.y.to,
                                                             input->dpi_scale);
        reach_render_group group = {};
        group.id = 720 + slot;
        group.parent_id = 22;
        group.bounds = row_layout.bounds;
        group.opacity = row.opacity;
        group.y = reach_motion_affine(&row.y, 1, -row.y.to);
        result = reach_render_command_buffer_begin_group(commands, &group);
        if (result == REACH_OK)
        {
            reach_quick_settings_push_wifi_row(commands, &input->wifi_row_items[slot], &row_layout,
                                               &model, &theme, metrics, &input->text_measure);
        }
        reach_render_command_buffer_end_group(commands);
    }
    if (result == REACH_OK)
        result = reach_quick_settings_push_press_feedback(input, commands,
                                                          REACH_QUICK_SETTINGS_FEEDBACK_WIFI);
    return result;
}

static int32_t reach_quick_settings_feedback_slot(uint64_t target)
{
    size_t index = reach_quick_settings_pressable_target_index(target);
    switch (reach_quick_settings_pressable_target_kind(target))
    {
    case REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM:
        return REACH_QUICK_SETTINGS_FEEDBACK_TRAILING;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON:
    case REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE:
        return REACH_QUICK_SETTINGS_FEEDBACK_OVERVIEW;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW:
        return REACH_QUICK_SETTINGS_FEEDBACK_OUTPUTS;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_SWITCH:
    case REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_ROW:
        return REACH_QUICK_SETTINGS_FEEDBACK_WIFI;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_MUTE:
        return index == REACH_QUICK_SETTINGS_MUTE_OVERVIEW ? REACH_QUICK_SETTINGS_FEEDBACK_OVERVIEW
               : index == REACH_QUICK_SETTINGS_MUTE_SOUND  ? REACH_QUICK_SETTINGS_FEEDBACK_SOUND_TOP
               : index == REACH_QUICK_SETTINGS_MUTE_INPUT  ? REACH_QUICK_SETTINGS_FEEDBACK_INPUT
                                                           : REACH_QUICK_SETTINGS_FEEDBACK_APPS;
    default:
        return -1;
    }
}

static reach_rect_f32 reach_quick_settings_mute_bounds(reach_rect_f32 pill)
{
    return {pill.x, pill.y, pill.height, pill.height};
}

static reach_rect_f32
reach_quick_settings_press_feedback_bounds(const reach_quick_settings_render_input *input)
{
    const reach_quick_settings_layout &layout = input->layout;
    size_t index = reach_quick_settings_pressable_target_index(input->press_feedback_target);
    switch (reach_quick_settings_pressable_target_kind(input->press_feedback_target))
    {
    case REACH_QUICK_SETTINGS_PRESS_TARGET_RAIL_ITEM:
        return index < REACH_QUICK_SETTINGS_PAGE_COUNT ? layout.rail_items[index]
                                                       : reach_rect_f32{};
    case REACH_QUICK_SETTINGS_PRESS_TARGET_SOUND_PAGE_BUTTON:
        return layout.sound_page_button;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_TOGGLE:
        return index < REACH_QUICK_SETTINGS_TOGGLE_COUNT ? layout.toggles[index].bounds
                                                         : reach_rect_f32{};
    case REACH_QUICK_SETTINGS_PRESS_TARGET_OUTPUT_DEVICE_ROW:
        return index < layout.output_device_row_count ? layout.output_device_rows[index].bounds
                                                      : reach_rect_f32{};
    case REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_SWITCH:
        return layout.wifi_switch;
    case REACH_QUICK_SETTINGS_PRESS_TARGET_WIFI_ROW:
        return index < layout.wifi_row_count ? layout.wifi_rows[index].bounds : reach_rect_f32{};
    case REACH_QUICK_SETTINGS_PRESS_TARGET_MUTE:
        if (index == REACH_QUICK_SETTINGS_MUTE_OVERVIEW)
            return reach_quick_settings_mute_bounds(layout.overview_volume.bounds);
        if (index == REACH_QUICK_SETTINGS_MUTE_SOUND)
            return reach_quick_settings_mute_bounds(layout.sound_volume.bounds);
        if (index == REACH_QUICK_SETTINGS_MUTE_INPUT)
            return reach_quick_settings_mute_bounds(layout.input_volume.bounds);
        index -= REACH_QUICK_SETTINGS_MUTE_FIRST_SESSION;
        return index < layout.app_volume_row_count
                   ? reach_quick_settings_mute_bounds(layout.app_volume_rows[index].bounds)
                   : reach_rect_f32{};
    default:
        return reach_rect_f32{};
    }
}

static reach_result
reach_quick_settings_push_press_feedback(const reach_quick_settings_render_input *input,
                                         reach_render_command_buffer *commands, int32_t slot)
{
    if (reach_quick_settings_feedback_slot(input->press_feedback_target) != slot)
    {
        return REACH_OK;
    }
    if (slot == REACH_QUICK_SETTINGS_FEEDBACK_INPUT && !input->model.input_available)
    {
        return REACH_OK;
    }
    reach_rect_f32 bounds = reach_quick_settings_press_feedback_bounds(input);
    if (bounds.width <= 0.0f || bounds.height <= 0.0f)
    {
        return REACH_OK;
    }
    static const uint64_t parents[] = {20, 21, 3, 4, 13, 22, 8};
    reach_render_group placement = {};
    placement.bounds = bounds;
    placement.parent_id = parents[slot];
    reach_color overlay = {0.0f, 0.0f, 0.0f, 0.12f};
    return reach_ui_feedback_render(commands, 900, bounds, bounds.height * 0.5f, overlay,
                                    &input->press_feedback, &placement);
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
        reach_quick_settings_push_pill(commands, &layout.overview_volume,
                                       input->model.main_volume_level, input->model.main_muted,
                                       reach_quick_settings_volume_icon_id(
                                           input->model.main_volume_level, input->model.main_muted),
                                       0, nullptr, &theme, metrics, &input->text_measure);
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
            reach_quick_settings_push_pill(
                commands, &layout.overview_brightness, input->model.brightness.level, 0,
                REACH_VECTOR_ICON_BRIGHTNESS, 0, nullptr, &theme, metrics, &input->text_measure);
        }
    }
    reach_render_command_buffer_end_group(commands);
    for (size_t index = 0; result == REACH_OK && index < REACH_QUICK_SETTINGS_TOGGLE_COUNT; ++index)
    {
        result = reach_quick_settings_push_toggle(
            commands, &layout.toggles[index], &input->model, index, &input->toggle_states[index],
            &theme, metrics, &input->text_measure, 600 + index * 2, 20);
    }
    return result == REACH_OK ? reach_quick_settings_push_press_feedback(
                                    input, commands, REACH_QUICK_SETTINGS_FEEDBACK_OVERVIEW)
                              : result;
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
    if (result == REACH_OK &&
        reach_quick_settings_page_visible(input, REACH_QUICK_SETTINGS_PAGE_SOUND))
        result = reach_quick_settings_push_sound(input, commands, &metrics);
    if (result == REACH_OK &&
        reach_quick_settings_page_visible(input, REACH_QUICK_SETTINGS_PAGE_WIFI))
        result = reach_quick_settings_push_wifi(input, commands, &metrics);
    if (result != REACH_OK)
        return result;
    return reach_quick_settings_push_press_feedback(input, commands,
                                                    REACH_QUICK_SETTINGS_FEEDBACK_TRAILING);
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
    input.output_devices_span = state->output_devices_span;
    input.app_volumes_span = state->app_volumes_span;
    reach_quick_settings_row_motions(quick_settings, input.output_device_row_motions,
                                     input.output_device_row_items, input.app_volume_row_motions,
                                     input.app_volume_row_items);
    reach_quick_settings_page_motions(quick_settings, input.toggle_states, input.page_opacity,
                                      input.page_shift, &input.rail_marker);
    reach_quick_settings_wifi_motions(quick_settings, input.wifi_row_motions, input.wifi_row_items,
                                      &input.wifi_switch);
    input.press_feedback_target = reach_quick_settings_press_feedback_target(quick_settings);
    input.press_feedback = reach_quick_settings_press_feedback_motion(quick_settings);

    reach_result result = reach_quick_settings_build_render_commands(&input, out_commands);
    if (result != REACH_OK)
    {
        return result;
    }
    return REACH_OK;
}
