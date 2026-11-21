#include "settings_pages_internal.h"

int32_t reach_settings_model_set_dock_size(reach_settings_model *model, int32_t size)
{
    if (model == nullptr || size < REACH_CONFIG_DOCK_SIZE_MIN ||
        size > REACH_CONFIG_DOCK_SIZE_MAX || model->dock_size == size)
    {
        return 0;
    }
    model->dock_size = size;
    return 1;
}

int32_t reach_settings_model_dock_size(const reach_settings_model *model)
{
    return model != nullptr ? model->dock_size : REACH_CONFIG_DOCK_SIZE_MAX;
}

void reach_settings_model_set_dock_auto_hide(reach_settings_model *model, int32_t auto_hide)
{
    if (model == nullptr)
    {
        return;
    }
    model->dock_auto_hide = auto_hide ? 1 : 0;
    reach_motion_set(&model->dock_auto_hide_toggle, model->dock_auto_hide ? 1.0f : 0.0f);
}

int32_t reach_settings_model_dock_auto_hide(const reach_settings_model *model)
{
    return model == nullptr || model->dock_auto_hide;
}

int32_t reach_settings_model_toggle_dock_auto_hide(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->dock_auto_hide = model->dock_auto_hide ? 0 : 1;
    reach_settings_animate(&model->dock_auto_hide_toggle, model->dock_auto_hide ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}
