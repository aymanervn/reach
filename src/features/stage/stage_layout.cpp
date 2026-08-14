#include "reach/features/stage.h"

#include "stage_common.h"

#include <math.h>

static reach_stage_grid reach_stage_grid_for_count(size_t tile_count)
{
    reach_stage_grid grid = {};
    if (tile_count == 0)
    {
        return grid;
    }

    size_t columns = 1;
    while (columns * columns < tile_count)
    {
        columns++;
    }

    size_t rows = tile_count / columns;
    if (tile_count % columns != 0)
    {
        rows++;
    }

    grid.columns = columns;
    grid.rows = rows;
    return grid;
}

static reach_rect_f32 reach_stage_cell_bounds(reach_rect_f32 area, reach_stage_grid grid, size_t index,
                                       float gap)
{
    reach_rect_f32 cell = {};
    if (grid.columns == 0 || grid.rows == 0)
    {
        return cell;
    }

    size_t column = index % grid.columns;
    size_t row = index / grid.columns;
    if (row >= grid.rows)
    {
        return cell;
    }

    float cell_width = area.width / (float)grid.columns;
    float cell_height = area.height / (float)grid.rows;

    cell.x = area.x + (float)column * cell_width + gap * 0.5f;
    cell.y = area.y + (float)row * cell_height + gap * 0.5f;
    cell.width = cell_width - gap;
    cell.height = cell_height - gap;

    if (cell.width < 0.0f)
    {
        cell.width = 0.0f;
    }
    if (cell.height < 0.0f)
    {
        cell.height = 0.0f;
    }
    return cell;
}

static reach_rect_f32 reach_stage_fit_into_cell(reach_rect_f32 cell, reach_rect_f32 source)
{
    reach_rect_f32 fitted = {};
    if (source.width <= 0.0f || source.height <= 0.0f || cell.width <= 0.0f || cell.height <= 0.0f)
    {
        return fitted;
    }

    float scale_x = cell.width / source.width;
    float scale_y = cell.height / source.height;
    float scale = scale_x < scale_y ? scale_x : scale_y;
    if (scale > 1.0f)
    {
        scale = 1.0f;
    }

    fitted.width = source.width * scale;
    fitted.height = source.height * scale;
    fitted.x = cell.x + (cell.width - fitted.width) * 0.5f;
    fitted.y = cell.y + (cell.height - fitted.height) * 0.5f;
    return fitted;
}

static reach_rect_f32 reach_stage_content_area(reach_rect_f32 monitor_bounds, float dpi_scale)
{
    float scale = dpi_scale > 0.0f ? dpi_scale : 1.0f;
    float inset_x = monitor_bounds.width * 0.06f + 24.0f * scale;
    float inset_y = monitor_bounds.height * 0.08f + 24.0f * scale;

    reach_rect_f32 area = {};
    area.x = monitor_bounds.x + inset_x;
    area.y = monitor_bounds.y + inset_y;
    area.width = monitor_bounds.width - inset_x * 2.0f;
    area.height = monitor_bounds.height - inset_y * 2.0f;

    if (area.width < 0.0f)
    {
        area.width = 0.0f;
    }
    if (area.height < 0.0f)
    {
        area.height = 0.0f;
    }
    return area;
}

static reach_rect_f32 reach_stage_interpolate_rect(reach_rect_f32 from, reach_rect_f32 to, float progress)
{
    reach_rect_f32 out = {};
    out.x = from.x + (to.x - from.x) * progress;
    out.y = from.y + (to.y - from.y) * progress;
    out.width = from.width + (to.width - from.width) * progress;
    out.height = from.height + (to.height - from.height) * progress;
    return out;
}

void reach_stage_rebuild_layout(reach_stage *stage)
{
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    reach_stage_grid grid = reach_stage_grid_for_count(state->tile_count);
    reach_rect_f32 area = reach_stage_content_area(state->bounds, state->dpi_scale);
    float gap = 56.0f * (state->dpi_scale > 0.0f ? state->dpi_scale : 1.0f);

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        reach_rect_f32 cell = reach_stage_cell_bounds(area, grid, index, gap);
        tile->target_rect = reach_stage_fit_into_cell(cell, tile->source_rect);
    }
}

void reach_stage_apply_progress(reach_stage *stage)
{
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        tile->current_rect =
            reach_stage_interpolate_rect(tile->source_rect, tile->target_rect, state->progress);
    }
}
