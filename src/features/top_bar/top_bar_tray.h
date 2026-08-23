#ifndef REACH_FEATURES_TOP_BAR_TRAY_H
#define REACH_FEATURES_TOP_BAR_TRAY_H

#include "reach/features/top_bar.h"

typedef struct reach_top_bar_tray_popup reach_top_bar_tray_popup;

reach_result reach_top_bar_tray_popup_create(reach_top_bar_tray_popup **out_popup);
void reach_top_bar_tray_popup_destroy(reach_top_bar_tray_popup *popup);
void reach_top_bar_tray_set_overflow_start(reach_top_bar *top_bar, size_t overflow_start);

#endif
