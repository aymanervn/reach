#include "settings_pages_internal.h"

void reach_settings_model_set_top_bar_style(reach_settings_model *model,
                                            reach_config_top_bar_style style)
{
    if (model == nullptr || style < REACH_CONFIG_TOP_BAR_STYLE_SEGMENTED ||
        style > REACH_CONFIG_TOP_BAR_STYLE_UNIFIED)
    {
        return;
    }
    model->top_bar_style = style;
    reach_animation_manager_set(&model->top_bar_style_animation, 0,
                                style == REACH_CONFIG_TOP_BAR_STYLE_UNIFIED ? 1.0f : 0.0f);
}

reach_config_top_bar_style reach_settings_model_top_bar_style(const reach_settings_model *model)
{
    return model != nullptr ? model->top_bar_style : REACH_CONFIG_TOP_BAR_STYLE_SEGMENTED;
}

int32_t reach_settings_model_toggle_top_bar_style(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->top_bar_style = model->top_bar_style == REACH_CONFIG_TOP_BAR_STYLE_UNIFIED
                               ? REACH_CONFIG_TOP_BAR_STYLE_SEGMENTED
                               : REACH_CONFIG_TOP_BAR_STYLE_UNIFIED;
    float current = reach_animation_manager_value(&model->top_bar_style_animation, 0);
    reach_animation_manager_start(&model->top_bar_style_animation, 0, current,
                                  model->top_bar_style == REACH_CONFIG_TOP_BAR_STYLE_UNIFIED ? 1.0f
                                                                                             : 0.0f,
                                  0.18, REACH_EASING_EASE_OUT);
    return 1;
}

void reach_settings_model_set_top_bar_mode(reach_settings_model *model,
                                           reach_config_top_bar_mode mode)
{
    if (model == nullptr || mode < REACH_CONFIG_TOP_BAR_MODE_DYNAMIC ||
        mode > REACH_CONFIG_TOP_BAR_MODE_STATIC)
    {
        return;
    }
    model->top_bar_mode = mode;
    reach_animation_manager_set(&model->top_bar_mode_animation, 0,
                                mode == REACH_CONFIG_TOP_BAR_MODE_DYNAMIC ? 1.0f : 0.0f);
}

reach_config_top_bar_mode reach_settings_model_top_bar_mode(const reach_settings_model *model)
{
    return model != nullptr ? model->top_bar_mode : REACH_CONFIG_TOP_BAR_MODE_DYNAMIC;
}

int32_t reach_settings_model_toggle_top_bar_mode(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->top_bar_mode = model->top_bar_mode == REACH_CONFIG_TOP_BAR_MODE_STATIC
                              ? REACH_CONFIG_TOP_BAR_MODE_DYNAMIC
                              : REACH_CONFIG_TOP_BAR_MODE_STATIC;
    float current = reach_animation_manager_value(&model->top_bar_mode_animation, 0);
    reach_animation_manager_start(&model->top_bar_mode_animation, 0, current,
                                  model->top_bar_mode == REACH_CONFIG_TOP_BAR_MODE_DYNAMIC ? 1.0f
                                                                                           : 0.0f,
                                  0.18, REACH_EASING_EASE_OUT);
    return 1;
}

int32_t reach_settings_model_tick_top_bar_animations(reach_settings_model *model,
                                                     double delta_seconds)
{
    if (model == nullptr || !reach_settings_model_top_bar_animations_active(model))
    {
        return 0;
    }
    reach_animation_manager_tick(&model->top_bar_style_animation, delta_seconds);
    reach_animation_manager_tick(&model->top_bar_mode_animation, delta_seconds);
    return 1;
}

int32_t reach_settings_model_top_bar_animations_active(const reach_settings_model *model)
{
    return model != nullptr &&
           (reach_animation_manager_any_active(&model->top_bar_style_animation) ||
            reach_animation_manager_any_active(&model->top_bar_mode_animation));
}
