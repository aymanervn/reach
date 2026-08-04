#include "settings_pages_internal.h"

#include <string.h>

const uint16_t *reach_settings_startup_apps_page_title(void)
{
    return (const uint16_t *)L"Startup Apps";
}

const uint16_t *reach_settings_startup_apps_page_placeholder(void)
{
    return (const uint16_t *)L"Startup Apps settings page";
}

void reach_settings_model_apply_startup_apps(reach_settings_model *model,
                                             const reach_startup_app_list *list)
{
    if (model == nullptr)
    {
        return;
    }

    memset(&model->startup_apps, 0, sizeof(model->startup_apps));
    memset(model->startup_icons, 0, sizeof(model->startup_icons));
    if (list != nullptr)
    {
        model->startup_apps = *list;
        if (model->startup_apps.count > REACH_STARTUP_APP_MAX_ENTRIES)
        {
            model->startup_apps.count = REACH_STARTUP_APP_MAX_ENTRIES;
        }
    }

    for (size_t index = 0; index < REACH_STARTUP_APP_MAX_ENTRIES; ++index)
    {
        float value = index < model->startup_apps.count && model->startup_apps.entries[index].enabled
                          ? 1.0f
                          : 0.0f;
        reach_animation_manager_set(&model->startup_animations, index, value);
    }

    reach_scrollbar_set_target(&model->startup_scrollbar, 0.0f);
    model->startup_scrollbar.offset = 0.0f;
    model->startup_loaded = 1;
    model->startup_status = REACH_SETTINGS_STARTUP_STATUS_NONE;
}

void reach_settings_model_set_startup_icon(reach_settings_model *model, size_t index,
                                           uint64_t icon_id)
{
    if (model == nullptr || index >= REACH_STARTUP_APP_MAX_ENTRIES)
    {
        return;
    }
    model->startup_icons[index] = icon_id;
}

void reach_settings_model_set_startup_enabled(reach_settings_model *model, size_t index,
                                              int32_t enabled)
{
    if (model == nullptr || index >= model->startup_apps.count)
    {
        return;
    }
    model->startup_apps.entries[index].enabled = enabled ? 1 : 0;
    float current = reach_animation_manager_value(&model->startup_animations, index);
    reach_animation_manager_start(&model->startup_animations, index, current,
                                  enabled ? 1.0f : 0.0f, 0.18, REACH_EASING_EASE_OUT);
}

int32_t reach_settings_model_startup_enabled(const reach_settings_model *model, size_t index)
{
    return model != nullptr && index < model->startup_apps.count &&
           model->startup_apps.entries[index].enabled;
}

void reach_settings_model_set_startup_busy(reach_settings_model *model, int32_t busy)
{
    if (model == nullptr)
    {
        return;
    }
    model->startup_busy = busy ? 1 : 0;
}

void reach_settings_model_set_startup_status(reach_settings_model *model, int32_t status)
{
    if (model == nullptr)
    {
        return;
    }
    model->startup_status = status;
}

const uint16_t *reach_settings_startup_status_message(int32_t status)
{
    switch (status)
    {
    case REACH_SETTINGS_STARTUP_STATUS_LOADING:
        return (const uint16_t *)L"Reading startup apps...";
    case REACH_SETTINGS_STARTUP_STATUS_FAILED:
        return (const uint16_t *)L"That change could not be applied";
    default:
        return (const uint16_t *)L"";
    }
}

void reach_settings_model_scroll_startup(reach_settings_model *model, float delta)
{
    if (model == nullptr)
    {
        return;
    }
    reach_scrollbar_scroll(&model->startup_scrollbar, delta);
}

int32_t reach_settings_model_startup_scroll(reach_settings_model *model, double delta_seconds)
{
    if (model == nullptr)
    {
        return 0;
    }
    return reach_scrollbar_update(&model->startup_scrollbar, delta_seconds);
}

int32_t reach_settings_model_tick_startup_animations(reach_settings_model *model,
                                                     double delta_seconds)
{
    if (model == nullptr || !reach_animation_manager_any_active(&model->startup_animations))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->startup_animations, delta_seconds);
    return 1;
}

int32_t reach_settings_model_startup_animations_active(const reach_settings_model *model)
{
    return model != nullptr && reach_animation_manager_any_active(&model->startup_animations);
}
