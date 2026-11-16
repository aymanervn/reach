#include "settings_pages_internal.h"

void reach_settings_model_set_top_bar_style(reach_settings_model *model,
                                            reach_config_top_bar_style style)
{
    if (model == nullptr || style < REACH_CONFIG_TOP_BAR_STYLE_SPLIT ||
        style > REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT)
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
        style > REACH_CONFIG_TOP_BAR_STYLE_TRANSPARENT || model->top_bar_style == style)
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

void reach_settings_model_set_top_bar_icon_only(reach_settings_model *model, int32_t icon_only)
{
    if (model == nullptr)
    {
        return;
    }
    model->top_bar_icon_only = icon_only ? 1 : 0;
    reach_motion_set(&model->top_bar_icon_only_toggle, model->top_bar_icon_only ? 1.0f : 0.0f);
}

int32_t reach_settings_model_top_bar_icon_only(const reach_settings_model *model)
{
    return model != nullptr && model->top_bar_icon_only;
}

int32_t reach_settings_model_toggle_top_bar_icon_only(reach_settings_model *model)
{
    if (model == nullptr)
    {
        return 0;
    }
    model->top_bar_icon_only = model->top_bar_icon_only ? 0 : 1;
    reach_settings_animate(&model->top_bar_icon_only_toggle, model->top_bar_icon_only ? 1.0f : 0.0f,
                           REACH_MOTION_PRESET_BOUNCY);
    return 1;
}

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

float reach_settings_slider_ratio_at(const reach_settings_slider_card_layout *slider, float x)
{
    if (slider == nullptr || slider->track.width <= 0.0f)
    {
        return 1.0f;
    }
    float ratio = (x - slider->track.x) / slider->track.width;
    return ratio < 0.0f ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
}
