#ifndef REACH_FEATURES_SWITCHER_COMMON_H
#define REACH_FEATURES_SWITCHER_COMMON_H

#include "reach/features/switcher.h"

typedef struct reach_switcher_card
{
    reach_window_id window;
    reach_rect_f32 preview;
    int32_t preview_visible;
    int32_t selected;
} reach_switcher_card;

typedef struct reach_switcher_layout
{
    reach_switcher_card cards[REACH_SWITCHER_VISIBLE_MAX];
    size_t count;
    float scale;
} reach_switcher_layout;

static const float REACH_SWITCHER_PREVIEW_HEIGHT = 125.0f;
static const float REACH_SWITCHER_MIN_WIDTH = 280.0f;
static const float REACH_SWITCHER_FALLBACK_ICON_SIZE = 88.0f;
static const float REACH_SWITCHER_PADDING = 24.0f;
static const float REACH_SWITCHER_GAP = 24.0f;
static const float REACH_SWITCHER_SELECTION_INSET = 5.0f;
static const float REACH_SWITCHER_LABEL_GAP = 12.0f;
static const float REACH_SWITCHER_LABEL_HEIGHT = 20.0f;

const reach_switcher_layout *reach_switcher_card_layout(const reach_switcher *switcher);
reach_icon_service *reach_switcher_icons(reach_switcher *switcher);
reach_window_tracking *reach_switcher_windows(reach_switcher *switcher);

#endif
