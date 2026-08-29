#include "settings_pages_internal.h"

#define REACH_SETTINGS_BLUETOOTH_ROW_HEIGHT 58.0f
#define REACH_SETTINGS_BLUETOOTH_ROW_GAP 7.0f
#define REACH_SETTINGS_BLUETOOTH_ACTION_STRIP 46.0f
#define REACH_SETTINGS_BLUETOOTH_PIN_STRIP 72.0f

static reach_rect_f32 reach_settings_bluetooth_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width > 0.0f ? width : 0.0f;
    rect.height = height > 0.0f ? height : 0.0f;
    return rect;
}

static void reach_settings_bluetooth_layout_row_actions(reach_settings_layout *layout,
                                                        const reach_settings_model *model,
                                                        reach_rect_f32 row, size_t index,
                                                        float scale)
{
    float inset = 14.0f * scale;
    float button_height = 28.0f * scale;
    float gap = 8.0f * scale;
    float right = row.x + row.width - inset;

    if (reach_settings_bluetooth_row_is_pairing(model, index))
    {
        float button_width = 76.0f * scale;
        float strip_y = row.y + row.height - button_height - 12.0f * scale;
        layout->bluetooth_pin_reject_button = reach_settings_bluetooth_rect(
            right - button_width, strip_y, button_width, button_height);
        layout->bluetooth_pin_accept_button = reach_settings_bluetooth_rect(
            right - button_width * 2.0f - gap, strip_y, button_width, button_height);
        return;
    }

    const reach_bluetooth_device *device = &model->bluetooth_devices.devices[index];
    if (!device->paired && !device->can_pair)
    {
        return;
    }
    float action_width = 104.0f * scale;
    float strip_y = row.y + row.height - REACH_SETTINGS_BLUETOOTH_ACTION_STRIP * scale +
                    (REACH_SETTINGS_BLUETOOTH_ACTION_STRIP * scale - button_height) * 0.5f;
    layout->bluetooth_action_button =
        reach_settings_bluetooth_rect(right - action_width, strip_y, action_width, button_height);
}

void reach_settings_layout_bluetooth(reach_settings_layout *layout, reach_settings_model *model,
                                     float scale)
{
    if (layout == nullptr || model == nullptr)
    {
        return;
    }

    float scrollbar_width = 5.0f * scale;
    float area_x = layout->content_title.x;
    float area_y = layout->content_title.y + layout->content_title.height + 12.0f * scale;
    float area_width = layout->content.width - 64.0f * scale - scrollbar_width;
    float area_bottom = layout->content.y + layout->content.height - 22.0f * scale;

    float card_height = 72.0f * scale;
    float icon_box = 34.0f * scale;
    float toggle_width = 40.0f * scale;
    float toggle_height = 22.0f * scale;
    layout->bluetooth_radio_card =
        reach_settings_bluetooth_rect(area_x, area_y, area_width, card_height);
    layout->bluetooth_radio_icon = reach_settings_bluetooth_rect(
        area_x + 16.0f * scale, area_y + (card_height - icon_box) * 0.5f, icon_box, icon_box);
    layout->bluetooth_radio_toggle = reach_settings_bluetooth_rect(
        area_x + area_width - 18.0f * scale - toggle_width,
        area_y + (card_height - toggle_height) * 0.5f, toggle_width, toggle_height);
    float text_x = layout->bluetooth_radio_icon.x + icon_box + 14.0f * scale;
    float text_width = layout->bluetooth_radio_toggle.x - 14.0f * scale - text_x;
    layout->bluetooth_radio_title =
        reach_settings_bluetooth_rect(text_x, area_y + 15.0f * scale, text_width, 20.0f * scale);
    layout->bluetooth_radio_subtitle =
        reach_settings_bluetooth_rect(text_x, area_y + 37.0f * scale, text_width, 16.0f * scale);

    float button_y = area_y + card_height + 12.0f * scale;
    float button_height = 32.0f * scale;
    float scan_width = 118.0f * scale;
    layout->bluetooth_scan_button =
        reach_settings_bluetooth_rect(area_x, button_y, scan_width, button_height);

    float viewport_y = button_y + button_height + 12.0f * scale;
    if (model->bluetooth_status == REACH_SETTINGS_BLUETOOTH_STATUS_SCANNING)
    {
        layout->bluetooth_loader_bar =
            reach_settings_bluetooth_rect(area_x, viewport_y, area_width, 3.0f * scale);
        viewport_y += 3.0f * scale + 10.0f * scale;
    }

    layout->bluetooth_viewport =
        reach_settings_bluetooth_rect(area_x, viewport_y, area_width, area_bottom - viewport_y);
    layout->bluetooth_scrollbar_track = reach_settings_bluetooth_rect(
        layout->bluetooth_viewport.x + layout->bluetooth_viewport.width + 11.0f * scale,
        layout->bluetooth_viewport.y, scrollbar_width, layout->bluetooth_viewport.height);

    float row_gap = REACH_SETTINGS_BLUETOOTH_ROW_GAP * scale;
    float section_title_height = 22.0f * scale;
    float section_gap = 14.0f * scale;
    float content_y = 0.0f;

    for (size_t section = 0; section < 2; ++section)
    {
        const int32_t want_paired = section == 0 ? 1 : 0;
        size_t section_count = 0;
        for (size_t index = 0; index < model->bluetooth_devices.count; ++index)
        {
            if (model->bluetooth_devices.devices[index].paired == want_paired)
            {
                ++section_count;
            }
        }
        if (section_count == 0)
        {
            continue;
        }

        if (content_y > 0.0f)
        {
            content_y += section_gap;
        }
        size_t section_index = layout->bluetooth_section_count++;
        layout->bluetooth_section_ids[section_index] = section;
        layout->bluetooth_section_titles[section_index] = reach_settings_bluetooth_rect(
            layout->bluetooth_viewport.x,
            layout->bluetooth_viewport.y + content_y - model->bluetooth_scrollbar.offset,
            layout->bluetooth_viewport.width, section_title_height);
        content_y += section_title_height + 5.0f * scale;

        for (size_t index = 0; index < model->bluetooth_devices.count &&
                               layout->bluetooth_row_count < REACH_BLUETOOTH_MAX_DEVICES;
             ++index)
        {
            if (model->bluetooth_devices.devices[index].paired != want_paired)
            {
                continue;
            }

            float progress = reach_animation_manager_value(&model->bluetooth_row_animations, index);
            float extra = reach_settings_bluetooth_row_is_pairing(model, index)
                              ? REACH_SETTINGS_BLUETOOTH_PIN_STRIP * scale
                              : REACH_SETTINGS_BLUETOOTH_ACTION_STRIP * scale;
            float height = REACH_SETTINGS_BLUETOOTH_ROW_HEIGHT * scale + extra * progress;

            size_t row_index = layout->bluetooth_row_count++;
            layout->bluetooth_row_indices[row_index] = index;
            layout->bluetooth_rows[row_index] = reach_settings_bluetooth_rect(
                layout->bluetooth_viewport.x,
                layout->bluetooth_viewport.y + content_y - model->bluetooth_scrollbar.offset,
                layout->bluetooth_viewport.width, height);

            if (model->bluetooth_expanded_row >= 0 &&
                (size_t)model->bluetooth_expanded_row == index && progress > 0.99f)
            {
                reach_settings_bluetooth_layout_row_actions(
                    layout, model, layout->bluetooth_rows[row_index], index, scale);
            }
            content_y += height + row_gap;
        }
    }

    float base_content_height = content_y > 0.0f ? content_y - row_gap : 0.0f;
    layout->bluetooth_content_height = base_content_height > layout->bluetooth_viewport.height
                                           ? base_content_height + 20.0f * scale
                                           : base_content_height;
    reach_scrollbar_set_extents(&model->bluetooth_scrollbar, layout->bluetooth_content_height,
                                layout->bluetooth_viewport.height);
    if (layout->bluetooth_content_height > layout->bluetooth_viewport.height)
    {
        reach_scrollbar_layout scrollbar = reach_scrollbar_compute_layout(
            &model->bluetooth_scrollbar, layout->bluetooth_scrollbar_track,
            layout->bluetooth_viewport.height, layout->bluetooth_content_height, 34.0f * scale);
        layout->bluetooth_scrollbar_track = scrollbar.track;
        layout->bluetooth_scrollbar_thumb = scrollbar.thumb;
    }
}
