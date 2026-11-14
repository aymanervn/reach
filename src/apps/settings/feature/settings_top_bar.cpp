#include "settings_pages_internal.h"

#include <math.h>

void reach_settings_model_set_top_bar_style(reach_settings_model *model,
                                            reach_config_top_bar_style style)
{
    if (model == nullptr || style < REACH_CONFIG_TOP_BAR_STYLE_SPLIT ||
        style > REACH_CONFIG_TOP_BAR_STYLE_SIMPLE)
    {
        return;
    }
    model->top_bar_style = style;
    reach_motion_set(&model->top_bar_style_selection, (float)style);
}

reach_config_top_bar_style reach_settings_model_top_bar_style(const reach_settings_model *model)
{
    return model != nullptr ? model->top_bar_style : REACH_CONFIG_TOP_BAR_STYLE_SPLIT;
}

int32_t reach_settings_model_select_top_bar_style(reach_settings_model *model,
                                                  reach_config_top_bar_style style)
{
    if (model == nullptr || style < REACH_CONFIG_TOP_BAR_STYLE_SPLIT ||
        style > REACH_CONFIG_TOP_BAR_STYLE_SIMPLE || model->top_bar_style == style)
    {
        return 0;
    }
    model->top_bar_style = style;
    reach_settings_animate(&model->top_bar_style_selection, (float)style,
                           REACH_MOTION_PRESET_FADE_IN);
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
    reach_motion_set(&model->top_bar_mode_toggle,
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
    reach_settings_animate(&model->top_bar_mode_toggle,
                           model->top_bar_mode == REACH_CONFIG_TOP_BAR_MODE_DYNAMIC ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

int32_t reach_settings_model_set_top_bar_opacity(reach_settings_model *model, int32_t opacity)
{
    if (model == nullptr || opacity < 0 || opacity > REACH_CONFIG_TOP_BAR_OPACITY_MAX ||
        model->top_bar_opacity == opacity)
    {
        return 0;
    }
    model->top_bar_opacity = opacity;
    return 1;
}

int32_t reach_settings_model_top_bar_opacity(const reach_settings_model *model)
{
    return model != nullptr ? model->top_bar_opacity : REACH_CONFIG_TOP_BAR_OPACITY_MAX;
}

int32_t reach_settings_model_top_bar_opacity_enabled(const reach_settings_model *model)
{
    return model != nullptr && model->top_bar_style == REACH_CONFIG_TOP_BAR_STYLE_SIMPLE;
}

int32_t reach_settings_top_bar_opacity_at(const reach_settings_layout *layout, float x)
{
    if (layout == nullptr || layout->top_bar_opacity_track.width <= 0.0f)
    {
        return REACH_CONFIG_TOP_BAR_OPACITY_MAX;
    }
    float ratio = (x - layout->top_bar_opacity_track.x) / layout->top_bar_opacity_track.width;
    ratio = ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    return (int32_t)lroundf(ratio * (float)REACH_CONFIG_TOP_BAR_OPACITY_MAX);
}
