#include "settings_pages_internal.h"

const uint16_t *reach_settings_display_page_title(void)
{
    return (const uint16_t *)L"Display";
}

const uint16_t *reach_settings_display_page_placeholder(void)
{
    return (const uint16_t *)L"Display settings page";
}

void reach_settings_model_set_high_refresh_rate(reach_settings_model *model, int32_t enabled)
{
    if (model == nullptr)
    {
        return;
    }
    model->display_high_refresh_rate = enabled ? 1 : 0;
    reach_animation_manager_set(&model->display_fps_animation, 0, enabled ? 1.0f : 0.0f);
}

int32_t reach_settings_model_high_refresh_rate(const reach_settings_model *model)
{
    return model != nullptr && model->display_high_refresh_rate;
}

int32_t reach_settings_model_toggle_high_refresh_rate(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->display_high_refresh_rate = model->display_high_refresh_rate ? 0 : 1;
    float current = reach_animation_manager_value(&model->display_fps_animation, 0);
    reach_animation_manager_start(&model->display_fps_animation, 0, current,
                                  model->display_high_refresh_rate ? 1.0f : 0.0f, 0.18,
                                  REACH_EASING_EASE_OUT);
    return 1;
}

int32_t reach_settings_model_tick_display_animations(reach_settings_model *model,
                                                     double delta_seconds)
{
    if (model == nullptr || !reach_animation_manager_any_active(&model->display_fps_animation))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->display_fps_animation, delta_seconds);
    return 1;
}

int32_t reach_settings_model_display_animations_active(const reach_settings_model *model)
{
    return model != nullptr && reach_animation_manager_any_active(&model->display_fps_animation);
}
