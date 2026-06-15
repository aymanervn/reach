#include "reach/features/settings.h"

#include "settings_pages_internal.h"

void reach_settings_model_init(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return;
    }
    model->selected_page = REACH_SETTINGS_PAGE_WIFI;
}

void reach_settings_model_select_page(reach_settings_model *model, reach_settings_page page)
{
    if (model == nullptr)
    {
        return;
    }
    if (page < REACH_SETTINGS_PAGE_WIFI || page > REACH_SETTINGS_PAGE_MONITORS_SCALING)
    {
        return;
    }
    model->selected_page = page;
}

const reach_settings_nav_item *reach_settings_nav_items(size_t *out_count)
{
    static const reach_settings_nav_item items[REACH_SETTINGS_NAV_ITEM_COUNT] = {
        {REACH_SETTINGS_PAGE_WIFI, REACH_VECTOR_ICON_WIFI_HIGH, (const uint16_t *)L"Wi-Fi",
         {0.22f, 0.82f, 0.43f, 1.0f}, {0.22f, 0.82f, 0.43f, 0.20f}},
        {REACH_SETTINGS_PAGE_BLUETOOTH, REACH_VECTOR_ICON_BLUETOOTH_ON,
         (const uint16_t *)L"Bluetooth", {0.24f, 0.58f, 1.0f, 1.0f},
         {0.24f, 0.58f, 1.0f, 0.20f}},
        {REACH_SETTINGS_PAGE_ACCOUNT, REACH_VECTOR_ICON_LOCK, (const uint16_t *)L"Account",
         {0.31f, 0.78f, 0.86f, 1.0f}, {0.31f, 0.78f, 0.86f, 0.20f}},
        {REACH_SETTINGS_PAGE_STARTUP_APPS, REACH_VECTOR_ICON_SETTINGS,
         (const uint16_t *)L"Startup Apps", {0.70f, 0.38f, 0.95f, 1.0f},
         {0.70f, 0.38f, 0.95f, 0.20f}},
        {REACH_SETTINGS_PAGE_POWER_SLEEP, REACH_VECTOR_ICON_SLEEP,
         (const uint16_t *)L"Power and Sleep", {0.95f, 0.55f, 0.22f, 1.0f},
         {0.95f, 0.55f, 0.22f, 0.20f}},
        {REACH_SETTINGS_PAGE_MONITORS_SCALING, REACH_VECTOR_ICON_PROJECT,
         (const uint16_t *)L"Monitors and Scaling", {0.97f, 0.75f, 0.22f, 1.0f},
         {0.97f, 0.75f, 0.22f, 0.20f}},
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

reach_settings_layout reach_settings_layout_for_bounds(reach_rect_f32 bounds,
                                                       const reach_theme *theme, float dpi_scale)
{
    (void)theme;
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    reach_settings_layout layout = {};
    layout.bounds = bounds;
    layout.topbar = reach_settings_rect(0.0f, 0.0f, bounds.width, 44.0f * scale);

    float control_size = 18.0f * scale;
    float control_gap = 10.0f * scale;
    float control_y = (layout.topbar.height - control_size) * 0.5f;
    layout.close_button =
        reach_settings_rect(bounds.width - 18.0f * scale - control_size, control_y,
                            control_size, control_size);
    layout.maximize_button =
        reach_settings_rect(layout.close_button.x - control_gap - control_size, control_y,
                            control_size, control_size);

    float nav_width = bounds.width * 0.25f;
    if (nav_width < 176.0f * scale)
    {
        nav_width = 176.0f * scale;
    }
    if (nav_width > 240.0f * scale)
    {
        nav_width = 240.0f * scale;
    }
    layout.nav = reach_settings_rect(0.0f, layout.topbar.height, nav_width,
                                     bounds.height - layout.topbar.height);
    layout.content = reach_settings_rect(nav_width, layout.topbar.height, bounds.width - nav_width,
                                         bounds.height - layout.topbar.height);

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
                                           layout.nav.width - nav_padding_x * 2.0f,
                                           item_height);
        item->icon_background =
            reach_settings_rect(item->bounds.x + 8.0f * scale,
                                item->bounds.y + (item_height - icon_bg) * 0.5f, icon_bg,
                                icon_bg);
        item->icon = reach_settings_rect(item->icon_background.x + (icon_bg - icon_size) * 0.5f,
                                         item->icon_background.y + (icon_bg - icon_size) * 0.5f,
                                         icon_size, icon_size);
        item->label =
            reach_settings_rect(item->icon_background.x + icon_bg + 10.0f * scale,
                                item->bounds.y, item->bounds.width - icon_bg - 26.0f * scale,
                                item_height);
        item_y += item_height + item_gap;
    }

    layout.content_title =
        reach_settings_rect(layout.content.x + 28.0f * scale, layout.content.y + 28.0f * scale,
                            layout.content.width - 56.0f * scale, 42.0f * scale);
    layout.content_placeholder =
        reach_settings_rect(layout.content_title.x, layout.content_title.y + 56.0f * scale,
                            layout.content_title.width, 34.0f * scale);
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
    if (reach_settings_rect_contains(layout->maximize_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_MAXIMIZE;
        return result;
    }
    for (size_t index = 0; index < layout->nav_item_count && index < REACH_SETTINGS_NAV_ITEM_COUNT;
         ++index)
    {
        if (reach_settings_rect_contains(layout->nav_items[index].bounds, x, y))
        {
            result.type = REACH_SETTINGS_HIT_NAV_ITEM;
            result.page = (reach_settings_page)index;
            return result;
        }
    }
    if (reach_settings_rect_contains(layout->topbar, x, y))
    {
        result.type = REACH_SETTINGS_HIT_TOPBAR_DRAG;
    }
    return result;
}
