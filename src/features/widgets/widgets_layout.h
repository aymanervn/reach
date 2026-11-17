#ifndef REACH_WIDGETS_LAYOUT_H
#define REACH_WIDGETS_LAYOUT_H

#include "reach/core/geometry.h"

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <vector>

struct reach_widget_layout_item
{
    reach_rect_f32 preferred;
    reach_point_f32 minimum;
    bool enabled;
    bool custom;
};

static inline bool reach_widgets_overlap(reach_rect_f32 a, reach_rect_f32 b, float gap)
{
    return a.width > 0 && a.height > 0 && b.width > 0 && b.height > 0 &&
           a.x < b.x + b.width + gap && b.x < a.x + a.width + gap && a.y < b.y + b.height + gap &&
           b.y < a.y + a.height + gap;
}

static inline bool reach_widgets_find_space(reach_rect_f32 area, reach_rect_f32 wanted,
                                            const std::vector<reach_rect_f32> &occupied, float gap,
                                            reach_rect_f32 *out, float grid = 0.0f)
{
    *out = {};
    if (wanted.width <= 0 || wanted.height <= 0 || wanted.width > area.width ||
        wanted.height > area.height)
        return false;
    wanted.x = std::clamp(wanted.x, area.x, area.x + area.width - wanted.width);
    wanted.y = std::clamp(wanted.y, area.y, area.y + area.height - wanted.height);
    std::vector<float> xs = {wanted.x, area.x, area.x + area.width - wanted.width};
    std::vector<float> ys = {wanted.y, area.y, area.y + area.height - wanted.height};
    for (const auto &rect : occupied)
    {
        xs.push_back(rect.x - gap - wanted.width);
        xs.push_back(rect.x + rect.width + gap);
        ys.push_back(rect.y - gap - wanted.height);
        ys.push_back(rect.y + rect.height + gap);
    }
    if (grid > 0.0f)
    {
        const auto align = [grid](std::vector<float> &points)
        {
            const size_t count = points.size();
            for (size_t index = 0; index < count; ++index)
            {
                const float units = points[index] / grid;
                points[index] = std::floor(units) * grid;
                points.push_back(std::ceil(units) * grid);
            }
        };
        align(xs);
        align(ys);
    }
    double best = std::numeric_limits<double>::infinity();
    for (float x : xs)
    {
        for (float y : ys)
        {
            reach_rect_f32 candidate = {x, y, wanted.width, wanted.height};
            if (x < area.x || y < area.y || x + wanted.width > area.x + area.width ||
                y + wanted.height > area.y + area.height)
                continue;
            bool collision = false;
            for (const auto &rect : occupied)
                collision |= reach_widgets_overlap(candidate, rect, gap);
            if (collision)
                continue;
            double dx = x - wanted.x;
            double dy = y - wanted.y;
            double distance = dx * dx + dy * dy;
            if (distance < best)
            {
                *out = candidate;
                best = distance;
            }
        }
    }
    return std::isfinite(best);
}

static inline bool reach_widgets_find_drop(reach_rect_f32 desktop, reach_rect_f32 wanted,
                                           const std::vector<reach_rect_f32> &occupied,
                                           bool resizing, float gap, float grid,
                                           reach_rect_f32 *out)
{
    *out = {};
    wanted.width = std::round(wanted.width / grid) * grid;
    wanted.height = std::round(wanted.height / grid) * grid;
    if (wanted.width <= 0.0f || wanted.height <= 0.0f)
    {
        return false;
    }
    if (!resizing)
    {
        return reach_widgets_find_space(desktop, wanted, occupied, gap, out, grid);
    }
    wanted.x = std::round(wanted.x / grid) * grid;
    wanted.y = std::round(wanted.y / grid) * grid;
    if (wanted.x < desktop.x || wanted.y < desktop.y ||
        wanted.x + wanted.width > desktop.x + desktop.width ||
        wanted.y + wanted.height > desktop.y + desktop.height)
    {
        return false;
    }
    for (const auto &card : occupied)
    {
        if (reach_widgets_overlap(wanted, card, gap))
        {
            return false;
        }
    }
    *out = wanted;
    return true;
}

static inline bool reach_widgets_pack(reach_rect_f32 desktop, reach_rect_f32 automatic_area,
                                      const reach_widget_layout_item *items, size_t count,
                                      float gap, reach_rect_f32 *out)
{
    std::vector<reach_rect_f32> occupied;
    for (size_t index = 0; index < count; ++index)
        out[index] = {};
    bool complete = true;
    float right = automatic_area.x + automatic_area.width;
    float column_left = right;
    float y = automatic_area.y;
    for (int phase = 0; phase < 2; ++phase)
    {
        for (size_t index = 0; index < count; ++index)
        {
            const auto &item = items[index];
            if (!item.enabled || item.custom != (phase == 0))
                continue;
            reach_rect_f32 wanted = item.preferred;
            reach_rect_f32 area = item.custom ? desktop : automatic_area;
            if (!item.custom)
            {
                if (y + wanted.height > area.y + area.height && y > area.y)
                {
                    right = column_left - gap;
                    column_left = right;
                    y = area.y;
                }
                wanted.x = right - wanted.width;
                wanted.y = y;
            }
            bool placed = reach_widgets_find_space(area, wanted, occupied, gap, &out[index]);
            if (!placed)
            {
                wanted.width = std::min(wanted.width, item.minimum.x);
                wanted.height = std::min(wanted.height, item.minimum.y);
                placed = reach_widgets_find_space(area, wanted, occupied, gap, &out[index]);
            }
            complete &= placed;
            if (placed)
            {
                occupied.push_back(out[index]);
                if (!item.custom)
                {
                    column_left = std::min(column_left, out[index].x);
                    y = out[index].y + out[index].height + gap;
                }
            }
        }
    }
    return complete;
}

#endif
