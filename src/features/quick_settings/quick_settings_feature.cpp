#include "reach/features/quick_settings.h"

static float reach_quick_settings_clamp01(float value)
{
    if (value < 0.0f) {
        return 0.0f;
    }
    if (value > 1.0f) {
        return 1.0f;
    }
    return value;
}

void reach_quick_settings_model_init(
    reach_quick_settings_model *model
)
{
    if (model == nullptr) {
        return;
    }

    model->main_volume_level = 0.0f;
    model->main_muted = 0;
    model->expanded = 0;
}

void reach_quick_settings_model_set_main_volume(
    reach_quick_settings_model *model,
    float volume_level,
    int32_t muted
)
{
    if (model == nullptr) {
        return;
    }

    model->main_volume_level = reach_quick_settings_clamp01(volume_level);
    model->main_muted = muted ? 1 : 0;
}

reach_quick_settings_layout reach_quick_settings_layout_for_content_bounds(
    reach_rect_f32 content_bounds,
    const reach_theme *theme
)
{
    (void)theme;

    reach_quick_settings_layout layout = {};
    layout.content_bounds = content_bounds;

    const float padding = 12.0f;
    const float slider_height = 18.0f;
    const float expand_gap = 10.0f;
    const float expand_height = 34.0f;
    const float icon_size = 16.0f;

    layout.main_slider_track.x = content_bounds.x + padding;
    layout.main_slider_track.y = content_bounds.y + padding;
    layout.main_slider_track.width = content_bounds.width - padding * 2.0f;
    layout.main_slider_track.height = slider_height;

    if (layout.main_slider_track.width < 0.0f) {
        layout.main_slider_track.width = 0.0f;
    }

    layout.main_slider_fill = layout.main_slider_track;

    layout.expand_button.x = content_bounds.x + padding;
    layout.expand_button.y = layout.main_slider_track.y + slider_height + expand_gap;
    layout.expand_button.width = content_bounds.width - padding * 2.0f;
    layout.expand_button.height = expand_height;

    if (layout.expand_button.width < 0.0f) {
        layout.expand_button.width = 0.0f;
    }

    layout.expand_button_icon.width = icon_size;
    layout.expand_button_icon.height = icon_size;
    layout.expand_button_icon.x =
        layout.expand_button.x + layout.expand_button.width - padding - icon_size;
    layout.expand_button_icon.y =
        layout.expand_button.y + (layout.expand_button.height - icon_size) * 0.5f;

    layout.expand_button_label.x = layout.expand_button.x + padding;
    layout.expand_button_label.y = layout.expand_button.y;
    layout.expand_button_label.width =
        layout.expand_button_icon.x - layout.expand_button_label.x - padding;
    layout.expand_button_label.height = layout.expand_button.height;

    if (layout.expand_button_label.width < 0.0f) {
        layout.expand_button_label.width = 0.0f;
    }

    return layout;
}
