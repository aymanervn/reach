#ifndef REACH_FEATURES_CONTEXT_MENU_METRICS_H
#define REACH_FEATURES_CONTEXT_MENU_METRICS_H

#include <stdint.h>

struct reach_context_menu_metrics
{
    float item_height;
    float padding;
    float screen_margin;

    float text_size;
    float text_leading_inset;
    float text_trailing_inset;
    float text_trailing_inset_with_close;

    float icon_size;
    float icon_inset;
    float icon_text_inset;

    float close_button_size;
    float close_button_inset;
    float close_glyph_inset_ratio;

    float glyph_advance_ratio;
    float window_list_max_width;
};

static constexpr float reach_context_menu_hover_text_size = 11.0f;

static constexpr reach_context_menu_metrics reach_context_menu_make_large_metrics()
{
    reach_context_menu_metrics metrics = {};

    metrics.item_height = 34.0f;
    metrics.padding = 8.0f;
    metrics.screen_margin = 8.0f;

    metrics.text_size = 14.0f;
    metrics.text_leading_inset = 14.0f;
    metrics.text_trailing_inset = 14.0f;
    metrics.text_trailing_inset_with_close = 14.0f;

    metrics.icon_size = 16.0f;
    metrics.icon_inset = 13.0f;
    metrics.icon_text_inset = 40.0f;

    metrics.close_button_size = 20.0f;
    metrics.close_button_inset = 7.0f;
    metrics.close_glyph_inset_ratio = 0.30f;

    metrics.glyph_advance_ratio = 0.62f;
    metrics.window_list_max_width = 320.0f;

    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_make_small_metrics()
{
    reach_context_menu_metrics metrics = reach_context_menu_make_large_metrics();

    metrics.item_height = 19.0f;
    metrics.padding = 4.0f;

    metrics.text_size = reach_context_menu_hover_text_size;
    metrics.text_leading_inset = 6.0f;
    metrics.text_trailing_inset = 10.0f;
    metrics.text_trailing_inset_with_close = 24.0f;

    metrics.close_button_size = 15.0f;
    metrics.close_button_inset = 5.0f;

    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_make_item_metrics()
{
    reach_context_menu_metrics metrics = reach_context_menu_make_large_metrics();
    metrics.text_size = reach_context_menu_hover_text_size;
    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_large_metrics =
    reach_context_menu_make_large_metrics();
static constexpr reach_context_menu_metrics reach_context_menu_small_metrics =
    reach_context_menu_make_small_metrics();
static constexpr reach_context_menu_metrics reach_context_menu_item_metrics =
    reach_context_menu_make_item_metrics();

static constexpr float reach_context_menu_power_popup_width = 176.0f;
static constexpr float reach_context_menu_power_anchor_ratio = 0.72f;
static constexpr float reach_context_menu_item_popup_width = 208.0f;
static constexpr float reach_context_menu_item_anchor_ratio = 0.30f;
static constexpr float reach_context_menu_window_list_anchor_ratio = 0.5f;

static inline const reach_context_menu_metrics *reach_context_menu_metrics_for(int32_t power_menu,
                                                                               int32_t window_list)
{
    if (window_list)
    {
        return &reach_context_menu_small_metrics;
    }
    return power_menu ? &reach_context_menu_large_metrics : &reach_context_menu_item_metrics;
}

#endif
