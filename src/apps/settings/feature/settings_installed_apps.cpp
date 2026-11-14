#include "settings_pages_internal.h"

#include <string.h>

const uint16_t *reach_settings_installed_apps_page_title(void)
{
    return (const uint16_t *)L"Applications";
}

const uint16_t *reach_settings_installed_apps_page_placeholder(void)
{
    return (const uint16_t *)L"Open and manage packaged applications installed for your account";
}

void reach_settings_model_apply_installed_apps(reach_settings_model *model,
                                               const reach_installed_app_list *list)
{
    if (model == nullptr)
    {
        return;
    }
    memset(&model->installed_apps, 0, sizeof(model->installed_apps));
    memset(model->installed_app_icons, 0, sizeof(model->installed_app_icons));
    if (list != nullptr)
    {
        model->installed_apps = *list;
        if (model->installed_apps.count > REACH_INSTALLED_APP_MAX_ENTRIES)
        {
            model->installed_apps.count = REACH_INSTALLED_APP_MAX_ENTRIES;
        }
    }
    reach_settings_scroll_to_top(&model->installed_apps_scroll);
    model->installed_apps_loaded = 1;
    model->installed_apps_busy = 0;
    model->installed_apps_status = REACH_SETTINGS_APPLICATIONS_STATUS_NONE;
}

void reach_settings_model_set_installed_apps_busy(reach_settings_model *model, int32_t busy)
{
    if (model != nullptr)
    {
        model->installed_apps_busy = busy ? 1 : 0;
        if (model->installed_apps_busy)
        {
            reach_loader_model_reset(&model->installed_apps_loader);
        }
    }
}

void reach_settings_model_set_installed_apps_status(reach_settings_model *model, int32_t status)
{
    if (model != nullptr)
    {
        model->installed_apps_status = status;
    }
}

const uint16_t *reach_settings_installed_apps_status_message(int32_t status)
{
    switch (status)
    {
    case REACH_SETTINGS_APPLICATIONS_STATUS_LOADING:
        return (const uint16_t *)L"Reading applications...";
    case REACH_SETTINGS_APPLICATIONS_STATUS_FAILED:
        return (const uint16_t *)L"Windows could not provide the application list.";
    case REACH_SETTINGS_APPLICATIONS_STATUS_ACTION_FAILED:
        return (const uint16_t *)L"The requested application action failed.";
    default:
        return (const uint16_t *)L"";
    }
}

void reach_settings_model_scroll_installed_apps(reach_settings_model *model, float delta)
{
    if (model != nullptr)
    {
        (void)reach_scroll_view_scroll_by(&model->installed_apps_scroll, delta,
                                          reach_motion_now_seconds());
    }
}
