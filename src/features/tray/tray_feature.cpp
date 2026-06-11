#include "reach/features/tray.h"

#include <math.h>

static size_t reach_tray_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

void reach_tray_compute_popup_layout(reach_tray_model *model, const reach_theme *theme,
                                     const reach_dock_layout *dock_layout,
                                     float dpi_scale, reach_rect_f32 *out_bounds)
{
    if (model == nullptr || theme == nullptr || dock_layout == nullptr || out_bounds == nullptr)
    {
        return;
    }

    float slot_size = reach_theme_tray_slot_size(theme, dock_layout->bounds.height);
    float gap = slot_size * 0.22f;
    float padding = slot_size * 0.58f;
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float notch_height = reach_popup_notch_height_scaled(scale);
    size_t visual_count = model->item_count > 0 ? model->item_count : 1;
    size_t columns = reach_tray_min_size(visual_count, 5);
    size_t rows = (visual_count + 4) / 5;
    float content_width = padding * 2.0f + (float)columns * slot_size + (float)(columns - 1) * gap;
    float content_height = padding * 2.0f + (float)rows * slot_size + (float)(rows - 1) * gap;

    out_bounds->width = ceilf(content_width);
    out_bounds->height = ceilf(content_height + notch_height);
    out_bounds->x = dock_layout->tray_button.x + dock_layout->tray_button.width * 0.5f -
                    out_bounds->width * 0.5f;
    out_bounds->y = dock_layout->bounds.y - out_bounds->height - 8.0f * scale;

    float grid_height = (float)rows * slot_size + (float)(rows - 1) * gap;
    float grid_y = out_bounds->y + (content_height - grid_height) * 0.5f;
    for (size_t index = 0; index < model->item_count; ++index)
    {
        size_t row = index / 5;
        size_t column = index % 5;
        size_t row_start = row * 5;
        size_t row_remaining = model->item_count - row_start;
        size_t row_columns = reach_tray_min_size(row_remaining, 5);
        float row_width = (float)row_columns * slot_size + (float)(row_columns - 1) * gap;
        float row_x = out_bounds->x + (out_bounds->width - row_width) * 0.5f;
        model->item_slots[index].x = row_x + (float)column * (slot_size + gap);
        model->item_slots[index].y = grid_y + (float)row * (slot_size + gap);
        model->item_slots[index].width = slot_size;
        model->item_slots[index].height = slot_size;
    }
}
