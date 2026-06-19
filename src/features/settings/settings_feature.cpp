#include "reach/core/render_commands.h"
#include "reach/features/settings.h"

#include <string.h>

#include "settings_pages_internal.h"

void reach_settings_model_init(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    memset(model, 0, sizeof(*model));
    model->selected_page = REACH_SETTINGS_PAGE_WIFI;
    model->update_page_state = REACH_SETTINGS_UPDATE_NOT_SCANNED;
}

void reach_settings_model_select_page(reach_settings_model *model, reach_settings_page page)
{
    if (model == nullptr)
    {
        return;
    }
    if (page < REACH_SETTINGS_PAGE_WIFI || page > REACH_SETTINGS_PAGE_UPDATE)
    {
        return;
    }
    model->selected_page = page;
}

const reach_settings_nav_item *reach_settings_nav_items(size_t *out_count)
{
    static const reach_settings_nav_item items[REACH_SETTINGS_NAV_ITEM_COUNT] = {
        {REACH_SETTINGS_PAGE_WIFI,
         REACH_VECTOR_ICON_WIFI_HIGH,
         (const uint16_t *)L"Wi-Fi",
         {0.22f, 0.82f, 0.43f, 1.0f},
         {0.22f, 0.82f, 0.43f, 0.20f}},
        {REACH_SETTINGS_PAGE_BLUETOOTH,
         REACH_VECTOR_ICON_BLUETOOTH_ON,
         (const uint16_t *)L"Bluetooth",
         {0.24f, 0.58f, 1.0f, 1.0f},
         {0.24f, 0.58f, 1.0f, 0.20f}},
        {REACH_SETTINGS_PAGE_ACCOUNT,
         REACH_VECTOR_ICON_LOCK,
         (const uint16_t *)L"Account",
         {0.31f, 0.78f, 0.86f, 1.0f},
         {0.31f, 0.78f, 0.86f, 0.20f}},
        {REACH_SETTINGS_PAGE_STARTUP_APPS,
         REACH_VECTOR_ICON_SETTINGS,
         (const uint16_t *)L"Startup Apps",
         {0.70f, 0.38f, 0.95f, 1.0f},
         {0.70f, 0.38f, 0.95f, 0.20f}},
        {REACH_SETTINGS_PAGE_POWER_SLEEP,
         REACH_VECTOR_ICON_SLEEP,
         (const uint16_t *)L"Power and Sleep",
         {0.95f, 0.55f, 0.22f, 1.0f},
         {0.95f, 0.55f, 0.22f, 0.20f}},
        {REACH_SETTINGS_PAGE_MONITORS_SCALING,
         REACH_VECTOR_ICON_RESIZE,
         (const uint16_t *)L"Monitors and Scaling",
         {0.97f, 0.75f, 0.22f, 1.0f},
         {0.97f, 0.75f, 0.22f, 0.20f}},
        {REACH_SETTINGS_PAGE_UPDATE,
         REACH_VECTOR_ICON_RESTART,
         (const uint16_t *)L"Windows Updates",
         {0.20f, 0.72f, 0.96f, 1.0f},
         {0.20f, 0.72f, 0.96f, 0.20f}},
    };

    if (out_count != nullptr)
    {
        *out_count = REACH_SETTINGS_NAV_ITEM_COUNT;
    }
    return items;
}

const uint16_t *reach_settings_page_title(reach_settings_page page)
{
    switch (page)
    {
    case REACH_SETTINGS_PAGE_WIFI:
        return reach_settings_wifi_page_title();
    case REACH_SETTINGS_PAGE_BLUETOOTH:
        return reach_settings_bluetooth_page_title();
    case REACH_SETTINGS_PAGE_ACCOUNT:
        return reach_settings_account_page_title();
    case REACH_SETTINGS_PAGE_STARTUP_APPS:
        return reach_settings_startup_apps_page_title();
    case REACH_SETTINGS_PAGE_POWER_SLEEP:
        return reach_settings_power_sleep_page_title();
    case REACH_SETTINGS_PAGE_MONITORS_SCALING:
        return reach_settings_monitors_scaling_page_title();
    case REACH_SETTINGS_PAGE_UPDATE:
        return reach_settings_update_page_title();
    default:
        return (const uint16_t *)L"Settings";
    }
}

const uint16_t *reach_settings_page_placeholder(reach_settings_page page)
{
    switch (page)
    {
    case REACH_SETTINGS_PAGE_WIFI:
        return reach_settings_wifi_page_placeholder();
    case REACH_SETTINGS_PAGE_BLUETOOTH:
        return reach_settings_bluetooth_page_placeholder();
    case REACH_SETTINGS_PAGE_ACCOUNT:
        return reach_settings_account_page_placeholder();
    case REACH_SETTINGS_PAGE_STARTUP_APPS:
        return reach_settings_startup_apps_page_placeholder();
    case REACH_SETTINGS_PAGE_POWER_SLEEP:
        return reach_settings_power_sleep_page_placeholder();
    case REACH_SETTINGS_PAGE_MONITORS_SCALING:
        return reach_settings_monitors_scaling_page_placeholder();
    case REACH_SETTINGS_PAGE_UPDATE:
        return (const uint16_t *)L"";
    default:
        return (const uint16_t *)L"Settings page";
    }
}

static reach_rect_f32 reach_settings_rect(float x, float y, float width, float height)
{
    reach_rect_f32 rect = {};
    rect.x = x;
    rect.y = y;
    rect.width = width > 0.0f ? width : 0.0f;
    rect.height = height > 0.0f ? height : 0.0f;
    return rect;
}

static int32_t reach_settings_update_in_select_section(reach_windows_update_state state)
{
    return state == REACH_WINDOWS_UPDATE_DISCOVERED || state == REACH_WINDOWS_UPDATE_SELECTED ||
           state == REACH_WINDOWS_UPDATE_DOWNLOADING || state == REACH_WINDOWS_UPDATE_DOWNLOADED ||
           state == REACH_WINDOWS_UPDATE_INSTALLING ||
           state == REACH_WINDOWS_UPDATE_INSTALLED_NO_REBOOT_REQUIRED ||
           state == REACH_WINDOWS_UPDATE_REBOOT_OBSERVED;
}

static int32_t reach_settings_update_in_restart_section(reach_windows_update_state state)
{
    return state == REACH_WINDOWS_UPDATE_INSTALLED_REBOOT_REQUIRED;
}

static int32_t reach_settings_update_in_failed_section(reach_windows_update_state state)
{
    return state == REACH_WINDOWS_UPDATE_FAILED;
}

reach_settings_layout reach_settings_layout_for_bounds(reach_rect_f32 bounds,
                                                       const reach_theme *theme, float dpi_scale,
                                                       reach_settings_model *model)
{
    (void)theme;
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_settings_layout layout = {};
    layout.bounds = bounds;

    float nav_width = bounds.width * 0.25f;
    if (nav_width < 176.0f * scale)
    {
        nav_width = 176.0f * scale;
    }
    if (nav_width > 240.0f * scale)
    {
        nav_width = 240.0f * scale;
    }
    layout.nav = reach_settings_rect(0.0f, 0.0f, nav_width, bounds.height);
    layout.content = reach_settings_rect(nav_width, 0.0f, bounds.width - nav_width, bounds.height);

    float control_size = 24.0f * scale;
    float control_gap = 10.0f * scale;
    float control_y = layout.content.y + 18.0f * scale;
    layout.close_button =
        reach_settings_rect(layout.content.x + layout.content.width - 20.0f * scale - control_size,
                            control_y, control_size, control_size);
    layout.minimize_button = reach_settings_rect(layout.close_button.x - control_gap - control_size,
                                                 control_y, control_size, control_size);

    float nav_padding_x = 16.0f * scale;
    float item_height = 42.0f * scale;
    float item_gap = 8.0f * scale;
    float item_y = layout.nav.y + 20.0f * scale;
    float icon_bg = 28.0f * scale;
    float icon_size = 16.0f * scale;

    layout.nav_item_count = REACH_SETTINGS_NAV_ITEM_COUNT;
    for (size_t index = 0; index < layout.nav_item_count; ++index)
    {
        reach_settings_nav_item_layout *item = &layout.nav_items[index];
        item->bounds = reach_settings_rect(nav_padding_x, item_y,
                                           layout.nav.width - nav_padding_x * 2.0f, item_height);
        item->icon_background =
            reach_settings_rect(item->bounds.x + 8.0f * scale,
                                item->bounds.y + (item_height - icon_bg) * 0.5f, icon_bg, icon_bg);
        item->icon = reach_settings_rect(item->icon_background.x + (icon_bg - icon_size) * 0.5f,
                                         item->icon_background.y + (icon_bg - icon_size) * 0.5f,
                                         icon_size, icon_size);
        item->label =
            reach_settings_rect(item->icon_background.x + icon_bg + 10.0f * scale, item->bounds.y,
                                item->bounds.width - icon_bg - 26.0f * scale, item_height);
        item_y += item_height + item_gap;
    }

    layout.content_title =
        reach_settings_rect(layout.content.x + 28.0f * scale, layout.content.y + 36.0f * scale,
                            layout.content.width - 124.0f * scale, 42.0f * scale);
    layout.content_placeholder =
        reach_settings_rect(layout.content_title.x, layout.content_title.y + 56.0f * scale,
                            layout.content_title.width, 34.0f * scale);

    float button_y = layout.content_title.y + 54.0f * scale;
    float button_height = 34.0f * scale;
    float button_gap = 10.0f * scale;
    float refresh_width = 92.0f * scale;
    float install_width = 154.0f * scale;
    float restart_width = 122.0f * scale;
    layout.update_refresh_button =
        reach_settings_rect(layout.content_title.x, button_y, refresh_width, button_height);
    layout.update_restart_button =
        reach_settings_rect(layout.content.x + layout.content.width - 28.0f * scale - restart_width,
                            button_y, restart_width, button_height);
    layout.update_install_button =
        reach_settings_rect(layout.update_restart_button.x - button_gap - install_width, button_y,
                            install_width, button_height);

    float viewport_y = button_y + button_height + 14.0f * scale;
    float scrollbar_width = 5.0f * scale;
    float viewport_bottom = layout.content.y + layout.content.height - 22.0f * scale;
    layout.update_viewport = reach_settings_rect(
        layout.content_title.x, viewport_y, layout.content.width - 64.0f * scale - scrollbar_width,
        viewport_bottom - viewport_y);
    layout.update_scrollbar_track = reach_settings_rect(
        layout.update_viewport.x + layout.update_viewport.width + 11.0f * scale,
        layout.update_viewport.y, scrollbar_width, layout.update_viewport.height);

    float row_height = 68.0f * scale;
    float row_gap = 7.0f * scale;
    float section_title_height = 22.0f * scale;
    float section_gap = 14.0f * scale;
    float content_y = 0.0f;
    typedef int32_t (*section_matcher)(reach_windows_update_state);
    const section_matcher matchers[3] = {reach_settings_update_in_select_section,
                                         reach_settings_update_in_restart_section,
                                         reach_settings_update_in_failed_section};
    for (size_t section = 0; section < 3; ++section)
    {
        size_t section_count = 0;
        if (model != nullptr)
            for (size_t index = 0; index < model->update_list.count; ++index)
                if (matchers[section](model->update_list.updates[index].state))
                    ++section_count;
        if (section_count == 0)
            continue;

        if (layout.update_section_count > 0)
            content_y += section_gap;
        size_t section_index = layout.update_section_count++;
        layout.update_section_ids[section_index] = section;
        layout.update_section_titles[section_index] =
            reach_settings_rect(layout.update_viewport.x,
                                layout.update_viewport.y + content_y -
                                    (model != nullptr ? model->update_scroll_offset : 0.0f),
                                layout.update_viewport.width, section_title_height);
        content_y += section_title_height + 5.0f * scale;

        for (size_t update_index = 0; model != nullptr && update_index < model->update_list.count;
             ++update_index)
        {
            if (!matchers[section](model->update_list.updates[update_index].state))
                continue;
            size_t row_index = layout.update_row_count++;
            layout.update_indices[row_index] = update_index;
            layout.update_rows[row_index] = reach_settings_rect(
                layout.update_viewport.x,
                layout.update_viewport.y + content_y - model->update_scroll_offset,
                layout.update_viewport.width, row_height);
            float checkbox_size = 18.0f * scale;
            layout.update_checkboxes[row_index] = reach_settings_rect(
                layout.update_rows[row_index].x + 12.0f * scale,
                layout.update_rows[row_index].y + (row_height - checkbox_size) * 0.5f,
                checkbox_size, checkbox_size);
            content_y += row_height + row_gap;
        }
    }
    layout.update_content_height = content_y > 0.0f ? content_y - row_gap : 0.0f;
    float scroll_max = layout.update_content_height > layout.update_viewport.height
                           ? layout.update_content_height - layout.update_viewport.height
                           : 0.0f;
    if (model != nullptr)
    {
        model->update_scroll_max = scroll_max;
        if (model->update_scroll_target > scroll_max)
            model->update_scroll_target = scroll_max;
        if (model->update_scroll_offset > scroll_max)
            model->update_scroll_offset = scroll_max;
    }
    if (scroll_max > 0.0f)
    {
        float thumb_height = layout.update_scrollbar_track.height * layout.update_viewport.height /
                             layout.update_content_height;
        if (thumb_height < 34.0f * scale)
            thumb_height = 34.0f * scale;
        float travel = layout.update_scrollbar_track.height - thumb_height;
        float progress = model != nullptr ? model->update_scroll_offset / scroll_max : 0.0f;
        layout.update_scrollbar_thumb = reach_settings_rect(
            layout.update_scrollbar_track.x, layout.update_scrollbar_track.y + travel * progress,
            layout.update_scrollbar_track.width, thumb_height);
    }
    return layout;
}

static int32_t reach_settings_rect_contains(reach_rect_f32 rect, float x, float y)
{
    return x >= rect.x && x <= rect.x + rect.width && y >= rect.y && y <= rect.y + rect.height;
}

reach_settings_hit_result reach_settings_hit_test(const reach_settings_layout *layout, float x,
                                                  float y)
{
    reach_settings_hit_result result = {};
    result.type = REACH_SETTINGS_HIT_NONE;
    result.page = REACH_SETTINGS_PAGE_WIFI;
    if (layout == nullptr)
    {
        return result;
    }

    if (reach_settings_rect_contains(layout->close_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_CLOSE;
        return result;
    }
    if (reach_settings_rect_contains(layout->minimize_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_MINIMIZE;
        return result;
    }
    if (reach_settings_rect_contains(layout->update_refresh_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_UPDATE_REFRESH;
        return result;
    }
    if (reach_settings_rect_contains(layout->update_install_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_UPDATE_INSTALL;
        return result;
    }
    if (reach_settings_rect_contains(layout->update_restart_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_UPDATE_RESTART;
        return result;
    }
    if (reach_settings_rect_contains(layout->update_scrollbar_thumb, x, y))
    {
        result.type = REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_THUMB;
        return result;
    }
    if (reach_settings_rect_contains(layout->update_scrollbar_track, x, y))
    {
        result.type = REACH_SETTINGS_HIT_UPDATE_SCROLLBAR_TRACK;
        return result;
    }
    for (size_t index = 0; index < layout->update_row_count; ++index)
    {
        if (layout->update_rows[index].y < layout->update_viewport.y ||
            layout->update_rows[index].y + layout->update_rows[index].height >
                layout->update_viewport.y + layout->update_viewport.height)
            continue;
        if (reach_settings_rect_contains(layout->update_checkboxes[index], x, y) ||
            reach_settings_rect_contains(layout->update_rows[index], x, y))
        {
            result.type = REACH_SETTINGS_HIT_UPDATE_CHECKBOX;
            result.update_index = layout->update_indices[index];
            return result;
        }
    }
    size_t nav_count = 0;
    const reach_settings_nav_item *items = reach_settings_nav_items(&nav_count);

    for (size_t index = 0; index < layout->nav_item_count &&
                           index < REACH_SETTINGS_NAV_ITEM_COUNT && index < nav_count;
         ++index)
    {
        if (reach_settings_rect_contains(layout->nav_items[index].bounds, x, y))
        {
            result.type = REACH_SETTINGS_HIT_NAV_ITEM;
            result.page = items[index].page;
            return result;
        }
    }
    return result;
}
