#include "settings_pages_internal.h"

#define REACH_SETTINGS_WIFI_ROW_HEIGHT 58.0f
#define REACH_SETTINGS_WIFI_ROW_GAP 7.0f
#define REACH_SETTINGS_WIFI_ACTION_STRIP 46.0f
#define REACH_SETTINGS_WIFI_KEY_FORM 96.0f
#define REACH_SETTINGS_WIFI_ADD_ROW_HEIGHT 46.0f
#define REACH_SETTINGS_WIFI_ADD_FORM 152.0f

static reach_rect_f32 reach_settings_wifi_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width > 0.0f ? width : 0.0f;
    rect.height = height > 0.0f ? height : 0.0f;
    return rect;
}

int32_t reach_settings_wifi_row_needs_key_form(const reach_settings_model *model, size_t index)
{
    if (model == nullptr || index >= model->wifi_networks.count)
    {
        return 0;
    }
    const reach_wifi_network *network = &model->wifi_networks.networks[index];
    if (network->connected || model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN)
    {
        return 0;
    }
    if (!reach_wifi_security_is_supported(network->security))
    {
        return 0;
    }
    return reach_wifi_security_needs_key(network->security) && !network->saved;
}

static void reach_settings_wifi_layout_row_actions(reach_settings_layout *layout,
                                                   const reach_settings_model *model,
                                                   reach_rect_f32 row, size_t index, float scale)
{
    const reach_wifi_network *network = &model->wifi_networks.networks[index];
    float inset = 14.0f * scale;
    float button_height = 28.0f * scale;
    float gap = 8.0f * scale;
    float right = row.x + row.width - inset;
    float strip_y = row.y + row.height - REACH_SETTINGS_WIFI_ACTION_STRIP * scale +
                    (REACH_SETTINGS_WIFI_ACTION_STRIP * scale - button_height) * 0.5f;

    if (reach_settings_wifi_row_needs_key_form(model, index))
    {
        float field_height = 30.0f * scale;
        float show_width = 58.0f * scale;
        float connect_width = 92.0f * scale;
        float form_y = row.y + row.height - REACH_SETTINGS_WIFI_KEY_FORM * scale + 8.0f * scale;

        layout->wifi_key_field = reach_settings_wifi_rect(
            row.x + inset, form_y, row.width - inset * 2.0f - show_width - gap, field_height);
        layout->wifi_show_button = reach_settings_wifi_rect(right - show_width, form_y, show_width,
                                                            field_height);

        float toggle_width = 34.0f * scale;
        float toggle_height = 18.0f * scale;
        float toggle_y = form_y + field_height + 14.0f * scale;
        layout->wifi_auto_toggle =
            reach_settings_wifi_rect(row.x + inset, toggle_y, toggle_width, toggle_height);
        layout->wifi_connect_button = reach_settings_wifi_rect(
            right - connect_width, toggle_y - 5.0f * scale, connect_width, button_height);
        return;
    }

    float forget_width = 84.0f * scale;
    float action_width = 104.0f * scale;
    if (network->saved || network->connected)
    {
        layout->wifi_forget_button =
            reach_settings_wifi_rect(right - forget_width, strip_y, forget_width, button_height);
        right -= forget_width + gap;
    }
    if (network->connected)
    {
        layout->wifi_disconnect_button =
            reach_settings_wifi_rect(right - action_width, strip_y, action_width, button_height);
    }
    else if (model->wifi_view != REACH_SETTINGS_WIFI_VIEW_KNOWN &&
             reach_wifi_security_is_supported(network->security))
    {
        layout->wifi_connect_button =
            reach_settings_wifi_rect(right - action_width, strip_y, action_width, button_height);
    }
}

static void reach_settings_wifi_layout_add_form(reach_settings_layout *layout,
                                                const reach_settings_model *model,
                                                reach_rect_f32 row, float scale)
{
    float inset = 14.0f * scale;
    float field_height = 30.0f * scale;
    float gap = 8.0f * scale;
    float width = row.width - inset * 2.0f;
    float form_y = row.y + row.height - REACH_SETTINGS_WIFI_ADD_FORM * scale + 6.0f * scale;

    layout->wifi_add_name_field =
        reach_settings_wifi_rect(row.x + inset, form_y, width, field_height);

    float option_y = form_y + field_height + gap;
    float option_height = 26.0f * scale;
    float option_width =
        (width - gap * (REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT - 1)) /
        (float)REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT;
    for (size_t option = 0; option < REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT; ++option)
    {
        layout->wifi_add_security_options[option] = reach_settings_wifi_rect(
            row.x + inset + (float)option * (option_width + gap), option_y, option_width,
            option_height);
    }

    float key_y = option_y + option_height + gap;
    float show_width = 58.0f * scale;
    if (model->wifi_add_security != REACH_WIFI_SECURITY_OPEN)
    {
        layout->wifi_add_key_field = reach_settings_wifi_rect(
            row.x + inset, key_y, width - show_width - gap, field_height);
        layout->wifi_add_show_button = reach_settings_wifi_rect(
            row.x + inset + width - show_width, key_y, show_width, field_height);
    }

    float toggle_width = 34.0f * scale;
    float toggle_height = 18.0f * scale;
    float submit_width = 92.0f * scale;
    float submit_height = 28.0f * scale;
    float footer_y = key_y + field_height + 12.0f * scale;
    layout->wifi_add_auto_toggle =
        reach_settings_wifi_rect(row.x + inset, footer_y, toggle_width, toggle_height);
    layout->wifi_add_submit_button =
        reach_settings_wifi_rect(row.x + inset + width - submit_width, footer_y - 5.0f * scale,
                                 submit_width, submit_height);
}

void reach_settings_layout_wifi(reach_settings_layout *layout, reach_settings_model *model,
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
    float viewport_y = area_y;

    if (model->wifi_view == REACH_SETTINGS_WIFI_VIEW_KNOWN)
    {
        layout->wifi_back_button =
            reach_settings_wifi_rect(area_x, area_y, 132.0f * scale, 30.0f * scale);
        viewport_y = area_y + 30.0f * scale + 14.0f * scale;
    }
    else
    {
        float card_height = 72.0f * scale;
        float icon_box = 34.0f * scale;
        float toggle_width = 40.0f * scale;
        float toggle_height = 22.0f * scale;
        layout->wifi_radio_card =
            reach_settings_wifi_rect(area_x, area_y, area_width, card_height);
        layout->wifi_radio_icon = reach_settings_wifi_rect(
            area_x + 16.0f * scale, area_y + (card_height - icon_box) * 0.5f, icon_box, icon_box);
        layout->wifi_radio_toggle = reach_settings_wifi_rect(
            area_x + area_width - 18.0f * scale - toggle_width,
            area_y + (card_height - toggle_height) * 0.5f, toggle_width, toggle_height);
        float text_x = layout->wifi_radio_icon.x + icon_box + 14.0f * scale;
        float text_width = layout->wifi_radio_toggle.x - 14.0f * scale - text_x;
        layout->wifi_radio_title =
            reach_settings_wifi_rect(text_x, area_y + 15.0f * scale, text_width, 20.0f * scale);
        layout->wifi_radio_subtitle =
            reach_settings_wifi_rect(text_x, area_y + 37.0f * scale, text_width, 16.0f * scale);

        float button_y = area_y + card_height + 12.0f * scale;
        float button_height = 32.0f * scale;
        float gap = 8.0f * scale;
        float scan_width = 118.0f * scale;
        float add_width = 118.0f * scale;
        float known_width = 148.0f * scale;
        layout->wifi_scan_button =
            reach_settings_wifi_rect(area_x, button_y, scan_width, button_height);
        layout->wifi_add_button = reach_settings_wifi_rect(
            area_x + area_width - known_width - gap - add_width, button_y, add_width,
            button_height);
        layout->wifi_known_button = reach_settings_wifi_rect(
            area_x + area_width - known_width, button_y, known_width, button_height);
        viewport_y = button_y + button_height + 12.0f * scale;
    }

    if (model->wifi_status == REACH_SETTINGS_WIFI_STATUS_SCANNING)
    {
        layout->wifi_loader_bar =
            reach_settings_wifi_rect(area_x, viewport_y, area_width, 3.0f * scale);
        viewport_y += 3.0f * scale + 10.0f * scale;
    }

    layout->wifi_viewport =
        reach_settings_wifi_rect(area_x, viewport_y, area_width, area_bottom - viewport_y);
    layout->wifi_scrollbar_track = reach_settings_wifi_rect(
        layout->wifi_viewport.x + layout->wifi_viewport.width + 11.0f * scale,
        layout->wifi_viewport.y, scrollbar_width, layout->wifi_viewport.height);

    float row_gap = REACH_SETTINGS_WIFI_ROW_GAP * scale;
    float content_y = 0.0f;

    if (model->wifi_view == REACH_SETTINGS_WIFI_VIEW_AVAILABLE)
    {
        float progress =
            reach_animation_manager_value(&model->wifi_row_animations, REACH_WIFI_MAX_NETWORKS);
        if (progress > 0.0f)
        {
            float full_height =
                (REACH_SETTINGS_WIFI_ADD_ROW_HEIGHT + REACH_SETTINGS_WIFI_ADD_FORM) * scale;
            float visible_height = full_height * progress;
            layout->wifi_add_row = reach_settings_wifi_rect(
                layout->wifi_viewport.x,
                layout->wifi_viewport.y + content_y - model->wifi_scrollbar.offset,
                layout->wifi_viewport.width, full_height);
            float clip_top = layout->wifi_add_row.y > layout->wifi_viewport.y
                                 ? layout->wifi_add_row.y
                                 : layout->wifi_viewport.y;
            float clip_bottom = layout->wifi_add_row.y + visible_height;
            float viewport_bottom = layout->wifi_viewport.y + layout->wifi_viewport.height;
            if (clip_bottom > viewport_bottom)
            {
                clip_bottom = viewport_bottom;
            }
            layout->wifi_add_clip = reach_settings_wifi_rect(
                layout->wifi_add_row.x, clip_top, layout->wifi_add_row.width,
                clip_bottom - clip_top);
            reach_settings_wifi_layout_add_form(layout, model, layout->wifi_add_row, scale);
            content_y += visible_height + row_gap;
        }
    }

    for (size_t index = 0;
         index < model->wifi_networks.count && layout->wifi_row_count < REACH_WIFI_MAX_NETWORKS;
         ++index)
    {
        if (!reach_settings_model_wifi_row_visible(model, index))
        {
            continue;
        }

        float progress = reach_animation_manager_value(&model->wifi_row_animations, index);
        float extra = reach_settings_wifi_row_needs_key_form(model, index)
                          ? REACH_SETTINGS_WIFI_KEY_FORM * scale
                          : REACH_SETTINGS_WIFI_ACTION_STRIP * scale;
        float height = REACH_SETTINGS_WIFI_ROW_HEIGHT * scale + extra * progress;

        size_t row_index = layout->wifi_row_count++;
        layout->wifi_row_indices[row_index] = index;
        layout->wifi_rows[row_index] = reach_settings_wifi_rect(
            layout->wifi_viewport.x,
            layout->wifi_viewport.y + content_y - model->wifi_scrollbar.offset,
            layout->wifi_viewport.width, height);

        if (model->wifi_expanded_row >= 0 && (size_t)model->wifi_expanded_row == index &&
            progress > 0.99f)
        {
            reach_settings_wifi_layout_row_actions(layout, model, layout->wifi_rows[row_index],
                                                   index, scale);
        }
        content_y += height + row_gap;
    }

    float base_content_height = content_y > 0.0f ? content_y - row_gap : 0.0f;
    layout->wifi_content_height = base_content_height > layout->wifi_viewport.height
                                      ? base_content_height + 20.0f * scale
                                      : base_content_height;
    reach_scrollbar_set_extents(&model->wifi_scrollbar, layout->wifi_content_height,
                                layout->wifi_viewport.height);
    if (layout->wifi_content_height > layout->wifi_viewport.height)
    {
        reach_scrollbar_layout scrollbar = reach_scrollbar_compute_layout(
            &model->wifi_scrollbar, layout->wifi_scrollbar_track, layout->wifi_viewport.height,
            layout->wifi_content_height, 34.0f * scale);
        layout->wifi_scrollbar_track = scrollbar.track;
        layout->wifi_scrollbar_thumb = scrollbar.thumb;
    }
}
