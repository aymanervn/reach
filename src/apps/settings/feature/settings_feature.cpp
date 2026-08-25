#include "reach/core/render_commands.h"
#include "reach/apps/settings/settings.h"
#include "reach/protocol/version.h"

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
    model->reach_update_state = REACH_SETTINGS_REACH_UPDATE_IDLE;
    {
        const char *version_text = REACH_VERSION_STRING;
        uint16_t version_wide[REACH_APP_UPDATE_VERSION_CAPACITY] = {};
        for (size_t index = 0;
             version_text[index] != 0 && index + 1 < REACH_APP_UPDATE_VERSION_CAPACITY; ++index)
            version_wide[index] = (uint16_t)version_text[index];
        reach_settings_model_set_current_version(model, version_wide);
    }
    reach_scrollbar_model_init(&model->update_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_scrollbar_model_init(&model->startup_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_scrollbar_model_init(&model->wifi_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_scrollbar_model_init(&model->bluetooth_scrollbar, REACH_SCROLLBAR_DRAG_FREE, 0.0f);
    reach_loader_model_init(&model->update_loader, 0.7f);
    reach_loader_model_init(&model->wifi_loader, 0.7f);
    reach_loader_model_init(&model->bluetooth_loader, 0.7f);
    reach_animation_manager_init(&model->wifi_row_animations, model->wifi_row_tracks,
                                 REACH_WIFI_MAX_NETWORKS + 1);
    reach_animation_manager_init(&model->wifi_view_animation, &model->wifi_view_track, 1);
    reach_animation_manager_init(&model->bluetooth_row_animations, model->bluetooth_row_tracks,
                                 REACH_BLUETOOTH_MAX_DEVICES);
    model->wifi_expanded_row = REACH_SETTINGS_WIFI_ROW_NONE;
    model->wifi_focused_field = REACH_SETTINGS_WIFI_FIELD_NONE;
    model->wifi_add_security = REACH_WIFI_SECURITY_WPA2_PERSONAL;
    model->wifi_connect_automatically = 1;
    model->bluetooth_expanded_row = REACH_SETTINGS_BLUETOOTH_ROW_NONE;
    reach_text_edit_init(&model->wifi_key_edit, REACH_WIFI_KEY_MAXIMUM_LENGTH);
    reach_text_edit_init(&model->wifi_add_key_edit, REACH_WIFI_KEY_MAXIMUM_LENGTH);
    reach_text_edit_init(&model->wifi_add_name_edit, REACH_WIFI_SSID_CAPACITY - 1);
    reach_animation_manager_init(&model->startup_animations, model->startup_tracks,
                                 REACH_STARTUP_APP_MAX_ENTRIES);
    reach_animation_manager_init(&model->power_animations, model->power_tracks,
                                 REACH_SETTINGS_POWER_TIMER_COUNT);
    reach_animation_manager_init(&model->power_wait_animations, model->power_wait_tracks,
                                 REACH_SETTINGS_POWER_TIMER_COUNT);
    model->power_focused_timer = -1;
    model->account_focused_field = -1;
    model->hovered_button = REACH_SETTINGS_HIT_NONE;
    reach_animation_manager_init(&model->display_fps_animation, &model->display_fps_track, 1);
    reach_animation_manager_init(&model->display_font_animation, &model->display_font_track, 1);
    reach_animation_manager_init(&model->display_theme_animation, &model->display_theme_track, 1);
    reach_animation_manager_init(&model->button_press_animation, &model->button_press_track, 1);
    reach_pressable_init(&model->button_pressable);
    for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
    {
        reach_text_edit_init(&model->account_password_edits[field],
                             REACH_SETTINGS_ACCOUNT_PASSWORD_CAPACITY);
    }
    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        for (size_t field = 0; field < REACH_SETTINGS_POWER_FIELD_COUNT; ++field)
        {
            reach_text_edit_init(&model->power_custom_edits[timer][field],
                                 REACH_SETTINGS_POWER_CUSTOM_DIGITS);
        }
    }
    reach_settings_model_set_power_minutes(model, REACH_SETTINGS_POWER_TIMER_SCREEN_OFF, 10);
    reach_settings_model_set_power_minutes(model, REACH_SETTINGS_POWER_TIMER_SLEEP, 30);
    reach_settings_model_set_power_minutes(model, REACH_SETTINGS_POWER_TIMER_LOCK, 15);
    reach_settings_model_set_power_minutes(model, REACH_SETTINGS_POWER_TIMER_SHUTDOWN, 0);
    reach_settings_model_set_power_minutes(model, REACH_SETTINGS_POWER_TIMER_RESTART, 0);
    reach_settings_model_set_power_wait_apps(model, REACH_SETTINGS_POWER_TIMER_SLEEP, 1);
    reach_settings_model_set_power_wait_apps(model, REACH_SETTINGS_POWER_TIMER_RESTART, 1);
    reach_settings_model_power_mark_applied(model);
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

float reach_settings_model_button_press_value(const reach_settings_model *model, int32_t hit_type)
{
    if (model == nullptr ||
        reach_pressable_feedback_index(&model->button_pressable) != (size_t)hit_type)
    {
        return 0.0f;
    }
    return reach_animation_manager_value(&model->button_press_animation, 0);
}

int32_t reach_settings_model_tick_button_press(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr || !reach_animation_manager_any_active(&model->button_press_animation))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->button_press_animation, delta_seconds);
    reach_pressable_feedback_style feedback = {};
    feedback.animations = &model->button_press_animation;
    feedback.track = 0;
    reach_pressable_settle_feedback(&model->button_pressable, &feedback);
    return 1;
}

int32_t reach_settings_model_button_press_active(const reach_settings_model *model)
{
    return model != nullptr && reach_animation_manager_any_active(&model->button_press_animation);
}

int32_t reach_settings_model_set_hovered_button(reach_settings_model *model, int32_t hit_type)
{
    if (model == nullptr || model->hovered_button == hit_type)
    {
        return 0;
    }
    model->hovered_button = hit_type;
    return 1;
}

const reach_settings_nav_item *reach_settings_nav_items(size_t *out_count)
{
    static const reach_settings_nav_item items[REACH_SETTINGS_NAV_ITEM_COUNT] = {
        {REACH_SETTINGS_PAGE_WIFI, REACH_VECTOR_ICON_WIFI_HIGH, (const uint16_t *)L"Wi-Fi",
         REACH_THEME_ACCENT_GREEN},
        {REACH_SETTINGS_PAGE_BLUETOOTH, REACH_VECTOR_ICON_BLUETOOTH_ON,
         (const uint16_t *)L"Bluetooth", REACH_THEME_ACCENT_BLUE},
        {REACH_SETTINGS_PAGE_ACCOUNT, REACH_VECTOR_ICON_LOCK, (const uint16_t *)L"Account",
         REACH_THEME_ACCENT_TEAL},
        {REACH_SETTINGS_PAGE_STARTUP_APPS, REACH_VECTOR_ICON_QUICK_SETTINGS,
         (const uint16_t *)L"Startup Apps", REACH_THEME_ACCENT_PURPLE},
        {REACH_SETTINGS_PAGE_POWER_SLEEP, REACH_VECTOR_ICON_SLEEP, (const uint16_t *)L"Energy",
         REACH_THEME_ACCENT_ORANGE},
        {REACH_SETTINGS_PAGE_DISPLAY, REACH_VECTOR_ICON_RESIZE, (const uint16_t *)L"Display",
         REACH_THEME_ACCENT_YELLOW},
        {REACH_SETTINGS_PAGE_UPDATE, REACH_VECTOR_ICON_RESTART, (const uint16_t *)L"Updates",
         REACH_THEME_ACCENT_CYAN},
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
    case REACH_SETTINGS_PAGE_DISPLAY:
        return reach_settings_display_page_title();
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
    case REACH_SETTINGS_PAGE_DISPLAY:
        return reach_settings_display_page_placeholder();
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

typedef struct reach_settings_toggle_card_rects
{
    reach_rect_f32 *card;
    reach_rect_f32 *icon;
    reach_rect_f32 *title;
    reach_rect_f32 *subtitle;
    reach_rect_f32 *toggle;
} reach_settings_toggle_card_rects;

static void reach_settings_layout_toggle_card(const reach_settings_toggle_card_rects *rects,
                                              float x, float y, float width, float height,
                                              float scale)
{
    float icon_box = 34.0f * scale;
    float toggle_width = 40.0f * scale;
    float toggle_height = 22.0f * scale;
    float toggle_x = x + width - 18.0f * scale - toggle_width;
    float text_x = x + 16.0f * scale + icon_box + 14.0f * scale;
    float text_width = toggle_x - 14.0f * scale - text_x;

    *rects->card = reach_settings_rect(x, y, width, height);
    *rects->icon =
        reach_settings_rect(x + 16.0f * scale, y + (height - icon_box) * 0.5f, icon_box, icon_box);
    *rects->toggle = reach_settings_rect(toggle_x, y + (height - toggle_height) * 0.5f,
                                         toggle_width, toggle_height);
    *rects->title = reach_settings_rect(text_x, y + 15.0f * scale, text_width, 20.0f * scale);
    *rects->subtitle = reach_settings_rect(text_x, y + 37.0f * scale, text_width, 16.0f * scale);
}

#define REACH_SETTINGS_THEME_CHOICE_OPTIONS_WIDTH 318.0f
#define REACH_SETTINGS_THEME_CHOICE_WIDE_WIDTH 520.0f

static void
reach_settings_layout_theme_choice_card(reach_rect_f32 *card, reach_rect_f32 *title,
                                        reach_rect_f32 *subtitle,
                                        reach_rect_f32 options[REACH_SETTINGS_THEME_OPTION_COUNT],
                                        float x, float y, float width, float height, float scale)
{
    *card = reach_settings_rect(x, y, width, height);
    float option_gap = 6.0f * scale;
    float option_height = 28.0f * scale;
    if (width >= REACH_SETTINGS_THEME_CHOICE_WIDE_WIDTH * scale)
    {
        float options_width = REACH_SETTINGS_THEME_CHOICE_OPTIONS_WIDTH * scale;
        float options_x = x + width - 16.0f * scale - options_width;
        float text_x = x + 18.0f * scale;
        *title = reach_settings_rect(text_x, y + 15.0f * scale, options_x - text_x - 16.0f * scale,
                                     20.0f * scale);
        *subtitle = reach_settings_rect(text_x, y + 37.0f * scale,
                                        options_x - text_x - 16.0f * scale, 16.0f * scale);
        float option_width =
            (options_width - option_gap * (REACH_SETTINGS_THEME_OPTION_COUNT - 1)) /
            REACH_SETTINGS_THEME_OPTION_COUNT;
        float option_y = y + (height - option_height) * 0.5f;
        for (size_t option = 0; option < REACH_SETTINGS_THEME_OPTION_COUNT; ++option)
        {
            options[option] =
                reach_settings_rect(options_x + (float)option * (option_width + option_gap),
                                    option_y, option_width, option_height);
        }
        return;
    }

    float text_x = x + 16.0f * scale;
    *title = reach_settings_rect(text_x, y + 11.0f * scale, width - 32.0f * scale, 20.0f * scale);
    *subtitle =
        reach_settings_rect(text_x, y + 31.0f * scale, width - 32.0f * scale, 16.0f * scale);
    float options_x = text_x;
    float options_width = width - 32.0f * scale;
    float option_width = (options_width - option_gap * (REACH_SETTINGS_THEME_OPTION_COUNT - 1)) /
                         REACH_SETTINGS_THEME_OPTION_COUNT;
    float option_y = y + height - option_height - 10.0f * scale;
    for (size_t option = 0; option < REACH_SETTINGS_THEME_OPTION_COUNT; ++option)
    {
        options[option] =
            reach_settings_rect(options_x + (float)option * (option_width + option_gap), option_y,
                                option_width, option_height);
    }
}

static int32_t reach_settings_update_in_select_section(reach_windows_update_state state)
{
    return state == REACH_WINDOWS_UPDATE_DISCOVERED || state == REACH_WINDOWS_UPDATE_SELECTED ||
           state == REACH_WINDOWS_UPDATE_DOWNLOADING || state == REACH_WINDOWS_UPDATE_DOWNLOADED ||
           state == REACH_WINDOWS_UPDATE_INSTALLING ||
           state == REACH_WINDOWS_UPDATE_INSTALLED_NO_REBOOT_REQUIRED ||
           state == REACH_WINDOWS_UPDATE_REBOOT_OBSERVED ||
           state == REACH_WINDOWS_UPDATE_VERIFIED_INSTALLED;
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

    layout.nav_footer =
        reach_settings_rect(nav_padding_x, layout.nav.y + layout.nav.height - 32.0f * scale,
                            layout.nav.width - nav_padding_x * 2.0f, 20.0f * scale);

    layout.content_title =
        reach_settings_rect(layout.content.x + 28.0f * scale, layout.content.y + 22.0f * scale,
                            layout.content.width - 124.0f * scale, 42.0f * scale);
    layout.content_placeholder =
        reach_settings_rect(layout.content_title.x, layout.content_title.y + 56.0f * scale,
                            layout.content_title.width, 34.0f * scale);

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_POWER_SLEEP)
    {
        float area_x = layout.content_title.x;
        float area_y = layout.content_title.y + layout.content_title.height + 14.0f * scale;
        float area_width = layout.content.x + layout.content.width - 28.0f * scale - area_x;
        float area_bottom = layout.content.y + layout.content.height - 22.0f * scale;
        float apply_height = 34.0f * scale;
        float apply_gap = 12.0f * scale;
        float apply_width = 104.0f * scale;
        layout.power_apply_button =
            reach_settings_rect(area_x + area_width - apply_width, area_bottom - apply_height,
                                apply_width, apply_height);
        area_bottom -= apply_height + apply_gap;
        float card_gap = 10.0f * scale;
        float card_height = 104.0f * scale;
        float needed = card_height * (float)REACH_SETTINGS_POWER_TIMER_COUNT +
                       card_gap * (float)(REACH_SETTINGS_POWER_TIMER_COUNT - 1);
        if (needed > area_bottom - area_y)
        {
            card_height =
                (area_bottom - area_y - card_gap * (float)(REACH_SETTINGS_POWER_TIMER_COUNT - 1)) /
                (float)REACH_SETTINGS_POWER_TIMER_COUNT;
            if (card_height < 74.0f * scale)
            {
                card_height = 74.0f * scale;
            }
        }
        int32_t show_subtitle = card_height >= 76.0f * scale;
        float icon_box = (show_subtitle ? 30.0f : 22.0f) * scale;
        float pill_height = 26.0f * scale;
        float pill_gap = 8.0f * scale;
        float card_y = area_y;
        for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
        {
            layout.power_cards[timer] =
                reach_settings_rect(area_x, card_y, area_width, card_height);
            layout.power_icon_boxes[timer] = reach_settings_rect(
                area_x + 16.0f * scale, card_y + (show_subtitle ? 12.0f : 7.0f) * scale, icon_box,
                icon_box);
            float text_x = area_x + 16.0f * scale + icon_box + 12.0f * scale;
            float text_width = area_x + area_width - text_x - 16.0f * scale;
            if (reach_settings_power_timer_supports_wait(timer))
            {
                float toggle_width = 34.0f * scale;
                float toggle_height = 18.0f * scale;
                float toggle_x = area_x + area_width - 16.0f * scale - toggle_width;
                float toggle_y = card_y + (show_subtitle ? 12.0f : 7.0f) * scale;
                layout.power_wait_toggles[timer] =
                    reach_settings_rect(toggle_x, toggle_y, toggle_width, toggle_height);
                float label_width = 216.0f * scale;
                layout.power_wait_labels[timer] = reach_settings_rect(
                    toggle_x - 8.0f * scale - label_width, toggle_y, label_width, toggle_height);
                float title_max = layout.power_wait_labels[timer].x - text_x - 12.0f * scale;
                if (text_width > title_max)
                {
                    text_width = title_max;
                }
            }
            layout.power_titles[timer] = reach_settings_rect(
                text_x, card_y + (show_subtitle ? 10.0f : 7.0f) * scale, text_width, 18.0f * scale);
            if (show_subtitle)
            {
                layout.power_subtitles[timer] =
                    reach_settings_rect(text_x, card_y + 29.0f * scale, text_width, 14.0f * scale);
            }
            float pills_x = area_x + 16.0f * scale;
            float pills_width = area_width - 32.0f * scale;
            float pill_width =
                (pills_width - pill_gap * (float)(REACH_SETTINGS_POWER_OPTION_COUNT - 1)) /
                (float)(REACH_SETTINGS_POWER_OPTION_COUNT + 1);
            float pill_y =
                card_y + card_height - pill_height - (show_subtitle ? 10.0f : 7.0f) * scale;
            for (size_t option = 0; option < REACH_SETTINGS_POWER_OPTION_COUNT; ++option)
            {
                layout.power_options[timer][option] = reach_settings_rect(
                    pills_x + (float)option * (pill_width + pill_gap), pill_y,
                    option == REACH_SETTINGS_POWER_CUSTOM_OPTION ? pill_width * 2.0f : pill_width,
                    pill_height);
            }
            reach_rect_f32 custom = layout.power_options[timer][REACH_SETTINGS_POWER_CUSTOM_OPTION];
            float field_pad = 8.0f * scale;
            float field_gap = 10.0f * scale;
            float field_width = (custom.width - field_pad * 2.0f - field_gap) * 0.5f;
            layout.power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_HOURS] =
                reach_settings_rect(custom.x + field_pad, custom.y, field_width, custom.height);
            layout.power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_MINUTES] =
                reach_settings_rect(custom.x + field_pad + field_width + field_gap, custom.y,
                                    field_width, custom.height);
            card_y += card_height + card_gap;
        }
    }

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_ACCOUNT)
    {
        float area_x = layout.content_title.x;
        float area_y = layout.content_title.y + layout.content_title.height + 14.0f * scale;
        float area_width = layout.content.x + layout.content.width - 28.0f * scale - area_x;

        float card_height = 128.0f * scale;
        layout.account_card = reach_settings_rect(area_x, area_y, area_width, card_height);

        float avatar = 84.0f * scale;
        layout.account_avatar = reach_settings_rect(
            area_x + 22.0f * scale, area_y + (card_height - avatar) * 0.5f, avatar, avatar);

        float text_x = layout.account_avatar.x + avatar + 20.0f * scale;
        float text_width = area_x + area_width - text_x - 22.0f * scale;
        layout.account_name =
            reach_settings_rect(text_x, area_y + 26.0f * scale, text_width, 26.0f * scale);
        layout.account_user =
            reach_settings_rect(text_x, area_y + 54.0f * scale, text_width, 16.0f * scale);
        layout.account_type_badge =
            reach_settings_rect(text_x, area_y + 78.0f * scale, 118.0f * scale, 24.0f * scale);

        float password_y = area_y + card_height + 12.0f * scale;
        float password_height = 156.0f * scale;
        layout.account_password_card =
            reach_settings_rect(area_x, password_y, area_width, password_height);
        float icon_box = 30.0f * scale;
        layout.account_password_icon = reach_settings_rect(
            area_x + 18.0f * scale, password_y + 16.0f * scale, icon_box, icon_box);
        float password_text_x = layout.account_password_icon.x + icon_box + 14.0f * scale;
        float password_right = area_x + area_width - 18.0f * scale;
        layout.account_password_title =
            reach_settings_rect(password_text_x, password_y + 14.0f * scale,
                                password_right - password_text_x, 18.0f * scale);
        layout.account_password_subtitle =
            reach_settings_rect(password_text_x, password_y + 34.0f * scale,
                                password_right - password_text_x, 14.0f * scale);
        layout.account_password_status = layout.account_password_subtitle;

        float row_y = password_y + 66.0f * scale;
        float row_height = 32.0f * scale;
        float field_gap = 10.0f * scale;
        float fields_x = area_x + 18.0f * scale;
        float fields_width = password_right - fields_x;
        float field_width =
            (fields_width - field_gap * (float)(REACH_SETTINGS_ACCOUNT_FIELD_COUNT - 1)) /
            (float)REACH_SETTINGS_ACCOUNT_FIELD_COUNT;
        for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
        {
            layout.account_password_fields[field] =
                reach_settings_rect(fields_x + (float)field * (field_width + field_gap), row_y,
                                    field_width, row_height);
        }
        float button_width = 96.0f * scale;
        layout.account_password_button =
            reach_settings_rect(password_right - button_width, row_y + row_height + 12.0f * scale,
                                button_width, row_height);
    }

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_STARTUP_APPS)
    {
        float scrollbar_width = 5.0f * scale;
        float area_x = layout.content_title.x;
        float area_y = layout.content_title.y + layout.content_title.height + 12.0f * scale;
        float area_width = layout.content.width - 64.0f * scale - scrollbar_width;

        layout.startup_summary = reach_settings_rect(area_x, area_y, area_width, 16.0f * scale);

        float viewport_y = area_y + 16.0f * scale + 12.0f * scale;
        float viewport_bottom = layout.content.y + layout.content.height - 22.0f * scale;
        layout.startup_viewport =
            reach_settings_rect(area_x, viewport_y, area_width, viewport_bottom - viewport_y);
        layout.startup_scrollbar_track = reach_settings_rect(
            layout.startup_viewport.x + layout.startup_viewport.width + 11.0f * scale,
            layout.startup_viewport.y, scrollbar_width, layout.startup_viewport.height);

        float row_height = 62.0f * scale;
        float row_gap = 8.0f * scale;
        float toggle_width = 40.0f * scale;
        float toggle_height = 22.0f * scale;
        float content_y = 0.0f;
        layout.startup_row_count = model->startup_apps.count < REACH_STARTUP_APP_MAX_ENTRIES
                                       ? model->startup_apps.count
                                       : REACH_STARTUP_APP_MAX_ENTRIES;
        for (size_t index = 0; index < layout.startup_row_count; ++index)
        {
            layout.startup_rows[index] = reach_settings_rect(
                layout.startup_viewport.x,
                layout.startup_viewport.y + content_y - model->startup_scrollbar.offset,
                layout.startup_viewport.width, row_height);
            layout.startup_toggles[index] = reach_settings_rect(
                layout.startup_rows[index].x + layout.startup_rows[index].width - 18.0f * scale -
                    toggle_width,
                layout.startup_rows[index].y + (row_height - toggle_height) * 0.5f, toggle_width,
                toggle_height);
            content_y += row_height + row_gap;
        }

        float base_content_height = content_y > 0.0f ? content_y - row_gap : 0.0f;
        layout.startup_content_height = base_content_height > layout.startup_viewport.height
                                            ? base_content_height + 20.0f * scale
                                            : base_content_height;
        reach_scrollbar_set_extents(&model->startup_scrollbar, layout.startup_content_height,
                                    layout.startup_viewport.height);
        if (layout.startup_content_height > layout.startup_viewport.height)
        {
            reach_scrollbar_layout scrollbar = reach_scrollbar_compute_layout(
                &model->startup_scrollbar, layout.startup_scrollbar_track,
                layout.startup_viewport.height, layout.startup_content_height, 34.0f * scale);
            layout.startup_scrollbar_track = scrollbar.track;
            layout.startup_scrollbar_thumb = scrollbar.thumb;
        }
    }

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_DISPLAY)
    {
        float area_x = layout.content_title.x;
        float area_y = layout.content_title.y + layout.content_title.height + 14.0f * scale;
        float area_width = layout.content.x + layout.content.width - 28.0f * scale - area_x;

        float card_height = 72.0f * scale;
        float card_spacing = 12.0f * scale;
        reach_settings_toggle_card_rects cards[] = {
            {&layout.display_fps_card, &layout.display_fps_icon, &layout.display_fps_title,
             &layout.display_fps_subtitle, &layout.display_fps_toggle},
            {&layout.display_font_card, &layout.display_font_icon, &layout.display_font_title,
             &layout.display_font_subtitle, &layout.display_font_toggle},
            {&layout.display_theme_card, &layout.display_theme_icon, &layout.display_theme_title,
             &layout.display_theme_subtitle, &layout.display_theme_toggle},
        };
        for (size_t index = 0; index < sizeof(cards) / sizeof(cards[0]); ++index)
        {
            reach_settings_layout_toggle_card(&cards[index], area_x,
                                              area_y + (float)index * (card_height + card_spacing),
                                              area_width, card_height, scale);
        }

        float section_y = area_y + 3.0f * card_height + 2.0f * card_spacing + 18.0f * scale;
        layout.display_windows_section_title =
            reach_settings_rect(area_x, section_y, area_width, 18.0f * scale);
        float appearance_y = section_y + 26.0f * scale;
        float appearance_card_height =
            area_width >= REACH_SETTINGS_THEME_CHOICE_WIDE_WIDTH * scale ? 76.0f * scale
                                                                        : 104.0f * scale;
        reach_settings_layout_theme_choice_card(
            &layout.display_windows_system_card, &layout.display_windows_system_title,
            &layout.display_windows_system_subtitle, layout.display_windows_system_options, area_x,
            appearance_y, area_width, appearance_card_height, scale);
        reach_settings_layout_theme_choice_card(
            &layout.display_windows_app_card, &layout.display_windows_app_title,
            &layout.display_windows_app_subtitle, layout.display_windows_app_options, area_x,
            appearance_y + appearance_card_height + card_spacing, area_width,
            appearance_card_height, scale);
    }

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_WIFI)
    {
        reach_settings_layout_wifi(&layout, model, scale);
        return layout;
    }

    if (model != nullptr && model->selected_page == REACH_SETTINGS_PAGE_BLUETOOTH)
    {
        reach_settings_layout_bluetooth(&layout, model, scale);
        return layout;
    }

    if (model != nullptr && model->selected_page != REACH_SETTINGS_PAGE_UPDATE)
    {
        return layout;
    }

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

    float scrollbar_width = 5.0f * scale;
    float section_header_height = 18.0f * scale;
    float content_width = layout.content.width - 64.0f * scale - scrollbar_width;

    float reach_title_y = button_y + button_height + 16.0f * scale;
    layout.reach_section_title = reach_settings_rect(layout.content_title.x, reach_title_y,
                                                     content_width, section_header_height);
    float reach_row_y = reach_title_y + section_header_height + 6.0f * scale;
    float reach_row_height = 64.0f * scale;
    layout.reach_update_row =
        reach_settings_rect(layout.content_title.x, reach_row_y, content_width, reach_row_height);
    float reach_button_width = 128.0f * scale;
    float reach_button_height = 32.0f * scale;
    layout.reach_update_button =
        reach_settings_rect(layout.reach_update_row.x + layout.reach_update_row.width -
                                14.0f * scale - reach_button_width,
                            reach_row_y + (reach_row_height - reach_button_height) * 0.5f,
                            reach_button_width, reach_button_height);

    float windows_title_y = reach_row_y + reach_row_height + 16.0f * scale;
    layout.windows_section_title = reach_settings_rect(layout.content_title.x, windows_title_y,
                                                       content_width, section_header_height);

    float viewport_y = windows_title_y + section_header_height + 6.0f * scale;
    float viewport_bottom = layout.content.y + layout.content.height - 22.0f * scale;
    layout.update_viewport = reach_settings_rect(layout.content_title.x, viewport_y, content_width,
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

        if (content_y > 0.0f)
            content_y += section_gap;
        if (section != 0)
        {
            size_t section_index = layout.update_section_count++;
            layout.update_section_ids[section_index] = section;
            layout.update_section_titles[section_index] =
                reach_settings_rect(layout.update_viewport.x,
                                    layout.update_viewport.y + content_y -
                                        (model != nullptr ? model->update_scrollbar.offset : 0.0f),
                                    layout.update_viewport.width, section_title_height);
            content_y += section_title_height + 5.0f * scale;
        }

        for (size_t update_index = 0; model != nullptr && update_index < model->update_list.count;
             ++update_index)
        {
            if (!matchers[section](model->update_list.updates[update_index].state))
                continue;
            size_t row_index = layout.update_row_count++;
            layout.update_indices[row_index] = update_index;
            layout.update_rows[row_index] = reach_settings_rect(
                layout.update_viewport.x,
                layout.update_viewport.y + content_y - model->update_scrollbar.offset,
                layout.update_viewport.width, row_height);
            float checkbox_size = 18.0f * scale;
            layout.update_checkboxes[row_index] = reach_settings_rect(
                layout.update_rows[row_index].x + 12.0f * scale,
                layout.update_rows[row_index].y + (row_height - checkbox_size) * 0.5f,
                checkbox_size, checkbox_size);
            content_y += row_height + row_gap;
        }
    }
    float base_content_height = content_y > 0.0f ? content_y - row_gap : 0.0f;
    float bottom_pad = 20.0f * scale;
    layout.update_content_height = base_content_height > layout.update_viewport.height
                                       ? base_content_height + bottom_pad
                                       : base_content_height;
    float scroll_max = layout.update_content_height > layout.update_viewport.height
                           ? layout.update_content_height - layout.update_viewport.height
                           : 0.0f;
    if (model != nullptr)
    {
        reach_scrollbar_set_extents(&model->update_scrollbar, layout.update_content_height,
                                    layout.update_viewport.height);
    }
    if (scroll_max > 0.0f && model != nullptr)
    {
        reach_scrollbar_layout scrollbar = reach_scrollbar_compute_layout(
            &model->update_scrollbar, layout.update_scrollbar_track, layout.update_viewport.height,
            layout.update_content_height, 34.0f * scale);
        layout.update_scrollbar_track = scrollbar.track;
        layout.update_scrollbar_thumb = scrollbar.thumb;
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
    if (layout->reach_update_button.width > 0.0f &&
        reach_settings_rect_contains(layout->reach_update_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_REACH_UPDATE;
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
    if (reach_settings_rect_contains(layout->update_viewport, x, y))
    {
        for (size_t index = 0; index < layout->update_row_count; ++index)
        {
            if (reach_settings_rect_contains(layout->update_checkboxes[index], x, y) ||
                reach_settings_rect_contains(layout->update_rows[index], x, y))
            {
                result.type = REACH_SETTINGS_HIT_UPDATE_CHECKBOX;
                result.update_index = layout->update_indices[index];
                return result;
            }
        }
    }
    if (layout->power_apply_button.width > 0.0f &&
        reach_settings_rect_contains(layout->power_apply_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_POWER_APPLY;
        return result;
    }
    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        const reach_rect_f32 toggle = layout->power_wait_toggles[timer];
        const reach_rect_f32 toggle_label = layout->power_wait_labels[timer];
        if (toggle.width > 0.0f && (reach_settings_rect_contains(toggle, x, y) ||
                                    reach_settings_rect_contains(toggle_label, x, y)))
        {
            result.type = REACH_SETTINGS_HIT_POWER_WAIT_TOGGLE;
            result.power_timer = timer;
            return result;
        }
    }
    for (size_t timer = 0; timer < REACH_SETTINGS_POWER_TIMER_COUNT; ++timer)
    {
        for (size_t option = 0; option < REACH_SETTINGS_POWER_OPTION_COUNT; ++option)
        {
            const reach_rect_f32 pill = layout->power_options[timer][option];
            if (pill.width > 0.0f && reach_settings_rect_contains(pill, x, y))
            {
                result.type = REACH_SETTINGS_HIT_POWER_OPTION;
                result.power_timer = timer;
                result.power_option = option;
                if (option == REACH_SETTINGS_POWER_CUSTOM_OPTION)
                {
                    result.power_custom_field =
                        x >= layout->power_custom_fields[timer][REACH_SETTINGS_POWER_FIELD_MINUTES]
                                    .x
                            ? REACH_SETTINGS_POWER_FIELD_MINUTES
                            : REACH_SETTINGS_POWER_FIELD_HOURS;
                }
                return result;
            }
        }
    }
    const struct
    {
        reach_rect_f32 card;
        reach_rect_f32 toggle;
        reach_settings_hit_type type;
    } display_cards[] = {
        {layout->display_fps_card, layout->display_fps_toggle,
         REACH_SETTINGS_HIT_DISPLAY_FPS_TOGGLE},
        {layout->display_font_card, layout->display_font_toggle,
         REACH_SETTINGS_HIT_DISPLAY_FONT_TOGGLE},
        {layout->display_theme_card, layout->display_theme_toggle,
         REACH_SETTINGS_HIT_DISPLAY_THEME_TOGGLE},
    };
    for (size_t index = 0; index < sizeof(display_cards) / sizeof(display_cards[0]); ++index)
    {
        if (display_cards[index].toggle.width > 0.0f &&
            (reach_settings_rect_contains(display_cards[index].toggle, x, y) ||
             reach_settings_rect_contains(display_cards[index].card, x, y)))
        {
            result.type = display_cards[index].type;
            return result;
        }
    }
    for (size_t option = 0; option < REACH_SETTINGS_THEME_OPTION_COUNT; ++option)
    {
        if (layout->display_windows_system_options[option].width > 0.0f &&
            reach_settings_rect_contains(layout->display_windows_system_options[option], x, y))
        {
            result.type = REACH_SETTINGS_HIT_DISPLAY_WINDOWS_SYSTEM_THEME;
            result.display_theme_preference = (reach_config_theme_preference)option;
            return result;
        }
        if (layout->display_windows_app_options[option].width > 0.0f &&
            reach_settings_rect_contains(layout->display_windows_app_options[option], x, y))
        {
            result.type = REACH_SETTINGS_HIT_DISPLAY_WINDOWS_APP_THEME;
            result.display_theme_preference = (reach_config_theme_preference)option;
            return result;
        }
    }
    if (layout->account_password_button.width > 0.0f &&
        reach_settings_rect_contains(layout->account_password_button, x, y))
    {
        result.type = REACH_SETTINGS_HIT_ACCOUNT_PASSWORD;
        return result;
    }
    for (size_t field = 0; field < REACH_SETTINGS_ACCOUNT_FIELD_COUNT; ++field)
    {
        const reach_rect_f32 field_rect = layout->account_password_fields[field];
        if (field_rect.width > 0.0f && reach_settings_rect_contains(field_rect, x, y))
        {
            result.type = REACH_SETTINGS_HIT_ACCOUNT_PASSWORD_FIELD;
            result.account_field = field;
            return result;
        }
    }
    if (layout->startup_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->startup_scrollbar_thumb, x, y))
    {
        result.type = REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_THUMB;
        return result;
    }
    if (layout->startup_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->startup_scrollbar_track, x, y))
    {
        result.type = REACH_SETTINGS_HIT_STARTUP_SCROLLBAR_TRACK;
        return result;
    }
    if (reach_settings_rect_contains(layout->startup_viewport, x, y))
    {
        for (size_t index = 0; index < layout->startup_row_count; ++index)
        {
            if (reach_settings_rect_contains(layout->startup_rows[index], x, y))
            {
                result.type = REACH_SETTINGS_HIT_STARTUP_TOGGLE;
                result.startup_index = index;
                return result;
            }
        }
    }

    const struct
    {
        reach_rect_f32 rect;
        reach_settings_hit_type type;
    } wifi_controls[] = {
        {layout->wifi_radio_toggle, REACH_SETTINGS_HIT_WIFI_RADIO_TOGGLE},
        {layout->wifi_scan_button, REACH_SETTINGS_HIT_WIFI_SCAN},
        {layout->wifi_add_button, REACH_SETTINGS_HIT_WIFI_ADD},
        {layout->wifi_known_button, REACH_SETTINGS_HIT_WIFI_KNOWN},
        {layout->wifi_back_button, REACH_SETTINGS_HIT_WIFI_BACK},
        {layout->wifi_key_field, REACH_SETTINGS_HIT_WIFI_KEY_FIELD},
        {layout->wifi_show_button, REACH_SETTINGS_HIT_WIFI_SHOW_KEY},
        {layout->wifi_auto_toggle, REACH_SETTINGS_HIT_WIFI_AUTO_TOGGLE},
        {layout->wifi_connect_button, REACH_SETTINGS_HIT_WIFI_CONNECT},
        {layout->wifi_disconnect_button, REACH_SETTINGS_HIT_WIFI_DISCONNECT},
        {layout->wifi_forget_button, REACH_SETTINGS_HIT_WIFI_FORGET},
        {layout->wifi_add_name_field, REACH_SETTINGS_HIT_WIFI_ADD_NAME_FIELD},
        {layout->wifi_add_key_field, REACH_SETTINGS_HIT_WIFI_ADD_KEY_FIELD},
        {layout->wifi_add_show_button, REACH_SETTINGS_HIT_WIFI_ADD_SHOW_KEY},
        {layout->wifi_add_auto_toggle, REACH_SETTINGS_HIT_WIFI_ADD_AUTO_TOGGLE},
        {layout->wifi_add_submit_button, REACH_SETTINGS_HIT_WIFI_ADD_SUBMIT},
    };
    for (size_t index = 0; index < sizeof(wifi_controls) / sizeof(wifi_controls[0]); ++index)
    {
        if (wifi_controls[index].rect.width > 0.0f &&
            reach_settings_rect_contains(wifi_controls[index].rect, x, y))
        {
            result.type = wifi_controls[index].type;
            return result;
        }
    }
    for (size_t option = 0; option < REACH_SETTINGS_WIFI_SECURITY_OPTION_COUNT; ++option)
    {
        if (layout->wifi_add_security_options[option].width > 0.0f &&
            reach_settings_rect_contains(layout->wifi_add_security_options[option], x, y))
        {
            result.type = REACH_SETTINGS_HIT_WIFI_ADD_SECURITY;
            result.wifi_security_option = option;
            return result;
        }
    }
    if (layout->wifi_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->wifi_scrollbar_thumb, x, y))
    {
        result.type = REACH_SETTINGS_HIT_WIFI_SCROLLBAR_THUMB;
        return result;
    }
    if (layout->wifi_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->wifi_scrollbar_track, x, y))
    {
        result.type = REACH_SETTINGS_HIT_WIFI_SCROLLBAR_TRACK;
        return result;
    }
    if (layout->wifi_viewport.width > 0.0f &&
        reach_settings_rect_contains(layout->wifi_viewport, x, y))
    {
        if (layout->wifi_add_row.width > 0.0f &&
            reach_settings_rect_contains(layout->wifi_add_row, x, y))
        {
            result.type = REACH_SETTINGS_HIT_WIFI_ADD;
            return result;
        }
        for (size_t index = 0; index < layout->wifi_row_count; ++index)
        {
            if (reach_settings_rect_contains(layout->wifi_rows[index], x, y))
            {
                result.type = REACH_SETTINGS_HIT_WIFI_ROW;
                result.wifi_index = layout->wifi_row_indices[index];
                return result;
            }
        }
    }

    const struct
    {
        reach_rect_f32 rect;
        reach_settings_hit_type type;
    } bluetooth_controls[] = {
        {layout->bluetooth_radio_toggle, REACH_SETTINGS_HIT_BLUETOOTH_RADIO_TOGGLE},
        {layout->bluetooth_scan_button, REACH_SETTINGS_HIT_BLUETOOTH_SCAN},
        {layout->bluetooth_action_button, REACH_SETTINGS_HIT_BLUETOOTH_ACTION},
        {layout->bluetooth_pin_accept_button, REACH_SETTINGS_HIT_BLUETOOTH_PIN_ACCEPT},
        {layout->bluetooth_pin_reject_button, REACH_SETTINGS_HIT_BLUETOOTH_PIN_REJECT},
    };
    for (size_t index = 0; index < sizeof(bluetooth_controls) / sizeof(bluetooth_controls[0]);
         ++index)
    {
        if (bluetooth_controls[index].rect.width > 0.0f &&
            reach_settings_rect_contains(bluetooth_controls[index].rect, x, y))
        {
            result.type = bluetooth_controls[index].type;
            return result;
        }
    }
    if (layout->bluetooth_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->bluetooth_scrollbar_thumb, x, y))
    {
        result.type = REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_THUMB;
        return result;
    }
    if (layout->bluetooth_scrollbar_thumb.height > 0.0f &&
        reach_settings_rect_contains(layout->bluetooth_scrollbar_track, x, y))
    {
        result.type = REACH_SETTINGS_HIT_BLUETOOTH_SCROLLBAR_TRACK;
        return result;
    }
    if (layout->bluetooth_viewport.width > 0.0f &&
        reach_settings_rect_contains(layout->bluetooth_viewport, x, y))
    {
        for (size_t index = 0; index < layout->bluetooth_row_count; ++index)
        {
            if (reach_settings_rect_contains(layout->bluetooth_rows[index], x, y))
            {
                result.type = REACH_SETTINGS_HIT_BLUETOOTH_ROW;
                result.bluetooth_index = layout->bluetooth_row_indices[index];
                return result;
            }
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
