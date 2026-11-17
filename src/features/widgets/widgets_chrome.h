#ifndef REACH_WIDGETS_CHROME_H
#define REACH_WIDGETS_CHROME_H

#include "reach/core/geometry.h"

struct reach_widget_chrome
{
    reach_rect_f32 header;
    reach_rect_f32 body;
    reach_rect_f32 footer;
    reach_rect_f32 resize;
};

static inline reach_widget_chrome reach_widgets_chrome(reach_rect_f32 card, float scale,
                                                       bool has_footer)
{
    const float inset = 16.0f * scale;
    const float gap = 8.0f * scale;
    reach_widget_chrome chrome = {};
    chrome.header = {card.x + inset, card.y + 14.0f * scale, card.width - 2.0f * inset,
                     25.0f * scale};
    const float footer_height = 30.0f * scale;
    const float grip_size = 22.0f * scale;
    chrome.footer = {chrome.header.x, card.y + card.height - grip_size - footer_height,
                     chrome.header.width, footer_height};
    const float body_y = chrome.header.y + chrome.header.height + gap;
    const float body_bottom = has_footer ? chrome.footer.y - gap
                                         : card.y + card.height - 18.0f * scale;
    chrome.body = {chrome.header.x, body_y, chrome.header.width, body_bottom - body_y};
    chrome.resize = {card.x + card.width - grip_size, card.y + card.height - grip_size,
                     grip_size, grip_size};
    return chrome;
}

#endif
