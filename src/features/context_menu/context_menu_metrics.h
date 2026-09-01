#ifndef REACH_FEATURES_CONTEXT_MENU_METRICS_H
#define REACH_FEATURES_CONTEXT_MENU_METRICS_H

#include <stdint.h>

#include "reach/core/typography.h"

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

static constexpr float reach_context_menu_hover_text_size = REACH_TEXT_SIZE_MEDIUM;
static constexpr float reach_context_menu_popup_scale = 0.9285714f;

static constexpr reach_context_menu_metrics reach_context_menu_make_large_metrics()
{
    reach_context_menu_metrics metrics = {};

    metrics.item_height = 34.0f;
    metrics.padding = 8.0f;
    metrics.screen_margin = 8.0f;

    metrics.text_size = REACH_TEXT_SIZE_MEDIUM;
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

static constexpr reach_context_menu_metrics reach_context_menu_make_popup_metrics()
{
    reach_context_menu_metrics metrics = reach_context_menu_make_large_metrics();
    metrics.text_size = reach_context_menu_hover_text_size;
    metrics.item_height *= reach_context_menu_popup_scale;
    metrics.padding *= reach_context_menu_popup_scale;
    metrics.text_leading_inset *= reach_context_menu_popup_scale;
    metrics.text_trailing_inset *= reach_context_menu_popup_scale;
    metrics.text_trailing_inset_with_close *= reach_context_menu_popup_scale;
    metrics.icon_size *= reach_context_menu_popup_scale;
    metrics.icon_inset *= reach_context_menu_popup_scale;
    metrics.icon_text_inset *= reach_context_menu_popup_scale;
    metrics.close_button_size *= reach_context_menu_popup_scale;
    metrics.close_button_inset *= reach_context_menu_popup_scale;
    return metrics;
}

static constexpr reach_context_menu_metrics reach_context_menu_small_metrics =
    reach_context_menu_make_small_metrics();
static constexpr reach_context_menu_metrics reach_context_menu_popup_metrics =
    reach_context_menu_make_popup_metrics();

static constexpr float reach_context_menu_power_popup_width =
    176.0f * reach_context_menu_popup_scale;
static constexpr float reach_context_menu_power_anchor_ratio = 0.72f;
static constexpr float reach_context_menu_item_popup_width =
    224.0f * reach_context_menu_popup_scale;
static constexpr float reach_context_menu_item_anchor_ratio = 0.30f;
static constexpr float reach_context_menu_window_list_anchor_ratio = 0.5f;

static inline const reach_context_menu_metrics *reach_context_menu_metrics_for(int32_t window_list)
{
    if (window_list)
    {
        return &reach_context_menu_small_metrics;
    }
    return &reach_context_menu_popup_metrics;
}

#endif
