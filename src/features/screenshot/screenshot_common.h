#ifndef REACH_FEATURES_SCREENSHOT_COMMON_H
#define REACH_FEATURES_SCREENSHOT_COMMON_H

#include "reach/features/screenshot.h"

#include <vector>

struct reach_screenshot
{
    int32_t open;
    reach_display_environment display;
    int32_t has_open_point;
    reach_point_f32 open_point;
    reach_rect_f32 bounds;
    reach_screen_image images[REACH_DISPLAY_MAX_MONITORS];
    size_t image_count;
    size_t active_image;
    int32_t pointer_captured;
    int32_t selecting;
    reach_point_f32 anchor;
    reach_point_f32 current;
    std::vector<uint64_t> retired_images;
    reach_screen_capture_port capture;
    reach_clipboard_port clipboard;
    reach_screenshot_routes routes;
};

const reach_screen_image *reach_screenshot_active(const reach_screenshot *screenshot);
int32_t reach_screenshot_selection_rect(const reach_screenshot *screenshot, reach_rect_f32 *out);

#endif
