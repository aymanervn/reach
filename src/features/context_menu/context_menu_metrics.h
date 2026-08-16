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
};

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

    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_make_small_metrics()
{
    reach_context_menu_metrics metrics = reach_context_menu_make_large_metrics();

    metrics.item_height = 19.0f;
    metrics.padding = 4.0f;

    metrics.text_size = 11.0f;
    metrics.text_leading_inset = 10.0f;
    metrics.text_trailing_inset = 10.0f;
    metrics.text_trailing_inset_with_close = 24.0f;

    metrics.close_button_size = 15.0f;
    metrics.close_button_inset = 5.0f;

    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_large_metrics =
    reach_context_menu_make_large_metrics();
static constexpr reach_context_menu_metrics reach_context_menu_small_metrics =
    reach_context_menu_make_small_metrics();

static constexpr float reach_context_menu_power_popup_width = 176.0f;
static constexpr float reach_context_menu_power_anchor_ratio = 0.72f;
static constexpr float reach_context_menu_item_popup_width = 208.0f;
static constexpr float reach_context_menu_item_anchor_ratio = 0.30f;
static constexpr float reach_context_menu_window_list_popup_width = 152.0f;
static constexpr float reach_context_menu_window_list_anchor_ratio = 0.5f;

static inline const reach_context_menu_metrics *reach_context_menu_metrics_for(int32_t window_list)
{
    return window_list ? &reach_context_menu_small_metrics : &reach_context_menu_large_metrics;
}

#endif
