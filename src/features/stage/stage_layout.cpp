#include "reach/features/stage.h"

#include "stage_common.h"

#include <math.h>

#define REACH_STAGE_BOX_LONG 16.0f
#define REACH_STAGE_BOX_SHORT 9.0f
#define REACH_STAGE_BAR_HEIGHT 32.0f
#define REACH_STAGE_BAR_MAX_THUMBNAIL_RATIO 0.25f
#define REACH_STAGE_DESKTOP_EDGE_GAP 20.0f
#define REACH_STAGE_CONTENT_INSET 24.0f
#define REACH_STAGE_TILE_GAP 20.0f
// A tile's displayed area grows with its window's real area raised to this power, so larger
// windows stay larger without dominating.
#define REACH_STAGE_AREA_EXPONENT 0.5f
// Smallest longer side of a tile's thumbnail, in DP, unless its window is smaller or the whole
// set cannot otherwise fit.
#define REACH_STAGE_MIN_LONG_SIDE 144.0f
// How far each tile moves from its row's centred packing toward its window's real position.
#define REACH_STAGE_ANCHOR_PULL 0.5f
#define REACH_STAGE_FIT_ITERATIONS 24

typedef struct reach_stage_layout_item
{
    size_t tile;
    reach_point_f32 anchor;
    float width;
    float height;
    // Linear scale per unit of the fit factor, before the real-size cap and the floor.
    float scale_per_fit;
    float floor_scale;
    float thumbnail_width;
    float thumbnail_height;
    float bar;
    float box_height;
} reach_stage_layout_item;

typedef struct reach_stage_layout_row
{
    size_t begin;
    size_t count;
    float width;
    float height;
} reach_stage_layout_row;

typedef struct reach_stage_layout
{
    reach_stage_layout_item items[REACH_STAGE_MAX_TILES];
    size_t count;
    // Item indices in reading order of their anchors: top to bottom, then left to right.
    size_t order[REACH_STAGE_MAX_TILES];
    reach_stage_layout_row rows[REACH_STAGE_MAX_TILES];
    size_t row_count;
    float bar_height;
    float border;
    float gap;
} reach_stage_layout;

static float reach_stage_tile_aspect(const reach_stage_tile *tile)
{
    return tile->source_rect.width > 0.0f && tile->source_rect.height > 0.0f
               ? tile->source_rect.width / tile->source_rect.height
               : REACH_STAGE_BOX_LONG / REACH_STAGE_BOX_SHORT;
}

static reach_rect_f32 reach_stage_content_area(reach_rect_f32 monitor_bounds, float dpi_scale)
{
    float inset = REACH_STAGE_CONTENT_INSET * dpi_scale;

    reach_rect_f32 area = {};
    area.x = monitor_bounds.x + inset;
    area.y = monitor_bounds.y + inset;
    area.width = monitor_bounds.width - inset * 2.0f;
    area.height = monitor_bounds.height - inset * 2.0f;
    return area;
}

float reach_stage_tile_bar_height(const reach_stage_state *state)
{
    float dpi_scale = state != nullptr && state->dpi_scale > 0.0f ? state->dpi_scale : 1.0f;
    return REACH_STAGE_BAR_HEIGHT * dpi_scale;
}

static reach_stage_tile *reach_stage_desktop_tile(reach_stage_state *state)
{
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        if (!state->tiles[index].departing && state->tiles[index].desktop)
        {
            return &state->tiles[index];
        }
    }
    return nullptr;
}

static void reach_stage_collect_layout_items(const reach_stage_state *state, float dpi_scale,
                                             reach_stage_layout *layout)
{
    layout->count = 0;
    float min_long_side = REACH_STAGE_MIN_LONG_SIDE * dpi_scale;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        if (tile->departing || tile->desktop)
        {
            continue;
        }

        reach_stage_layout_item *item = &layout->items[layout->count];
        *item = {};
        item->tile = index;
        item->anchor = tile->anchor;
        item->height = tile->source_rect.height > 0.0f ? tile->source_rect.height : 1.0f;
        item->width = item->height * reach_stage_tile_aspect(tile);
        item->scale_per_fit =
            powf(item->width * item->height, (REACH_STAGE_AREA_EXPONENT - 1.0f) * 0.5f);
        float long_side = item->width > item->height ? item->width : item->height;
        item->floor_scale = long_side > min_long_side ? min_long_side / long_side : 1.0f;
        layout->order[layout->count] = layout->count;
        layout->count++;
    }

    // Ties keep tile order, which runs front to back.
    for (size_t at = 1; at < layout->count; ++at)
    {
        size_t current = layout->order[at];
        size_t to = at;
        while (to > 0)
        {
            const reach_stage_layout_item *previous = &layout->items[layout->order[to - 1]];
            const reach_stage_layout_item *next = &layout->items[current];
            if (previous->anchor.y < next->anchor.y ||
                (previous->anchor.y == next->anchor.y && previous->anchor.x <= next->anchor.x))
            {
                break;
            }
            layout->order[to] = layout->order[to - 1];
            --to;
        }
        layout->order[to] = current;
    }
}

// The real size always caps a tile; the floor only holds a tile up to it.
static void reach_stage_scale_layout_items(reach_stage_layout *layout, float fit, float floor)
{
    for (size_t index = 0; index < layout->count; ++index)
    {
        reach_stage_layout_item *item = &layout->items[index];
        float scale = fit * item->scale_per_fit;
        float minimum = item->floor_scale * floor;
        scale = scale < minimum ? minimum : scale;
        scale = scale > 1.0f ? 1.0f : scale;
        item->thumbnail_width = item->width * scale;
        item->thumbnail_height = item->height * scale;
        float bar_limit = item->thumbnail_height * REACH_STAGE_BAR_MAX_THUMBNAIL_RATIO;
        item->bar = layout->bar_height < bar_limit ? layout->bar_height : bar_limit;
        item->box_height = item->bar + layout->border * 2.0f + item->thumbnail_height;
    }
}

// Splits the reading order into row_count consecutive, non-empty rows of similar width.
static void reach_stage_split_layout_rows(reach_stage_layout *layout, size_t row_count)
{
    float total = layout->gap * (float)(layout->count - row_count);
    for (size_t index = 0; index < layout->count; ++index)
    {
        total += layout->items[index].thumbnail_width;
    }

    layout->row_count = 0;
    size_t begin = 0;
    float consumed = 0.0f;
    for (size_t row = 0; row < row_count; ++row)
    {
        size_t rows_after = row_count - row - 1;
        float target = total * (float)(row + 1) / (float)row_count;
        size_t end = begin + 1;
        consumed += layout->items[layout->order[begin]].thumbnail_width;
        while (end < layout->count - rows_after)
        {
            float width = layout->items[layout->order[end]].thumbnail_width + layout->gap;
            if (rows_after != 0 && consumed + width * 0.5f > target)
            {
                break;
            }
            consumed += width;
            ++end;
        }

        reach_stage_layout_row *line = &layout->rows[layout->row_count++];
        line->begin = begin;
        line->count = end - begin;
        line->width = layout->gap * (float)(line->count - 1);
        line->height = 0.0f;
        for (size_t at = begin; at < end; ++at)
        {
            const reach_stage_layout_item *item = &layout->items[layout->order[at]];
            line->width += item->thumbnail_width;
            line->height = item->box_height > line->height ? item->box_height : line->height;
        }
        begin = end;
    }
}

static float reach_stage_layout_block_height(const reach_stage_layout *layout)
{
    float height = layout->gap * (float)(layout->row_count - 1);
    for (size_t row = 0; row < layout->row_count; ++row)
    {
        height += layout->rows[row].height;
    }
    return height;
}

static int32_t reach_stage_layout_fits(reach_stage_layout *layout, size_t row_count, float fit,
                                       float floor, reach_rect_f32 area)
{
    reach_stage_scale_layout_items(layout, fit, floor);
    reach_stage_split_layout_rows(layout, row_count);
    for (size_t row = 0; row < layout->row_count; ++row)
    {
        if (layout->rows[row].width > area.width)
        {
            return 0;
        }
    }
    return reach_stage_layout_block_height(layout) <= area.height;
}

// Largest factor in [low, high] that fits; low itself is assumed to fit.
static float reach_stage_search_fit(reach_stage_layout *layout, size_t row_count, float low,
                                    float high, int32_t floor_factor, reach_rect_f32 area)
{
    for (size_t iteration = 0; iteration < REACH_STAGE_FIT_ITERATIONS; ++iteration)
    {
        float middle = (low + high) * 0.5f;
        int32_t fits = floor_factor
                           ? reach_stage_layout_fits(layout, row_count, 0.0f, middle, area)
                           : reach_stage_layout_fits(layout, row_count, middle, 1.0f, area);
        if (fits)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    return low;
}

// How far the block's shape is from the area's, so equal-sized candidates prefer filling it.
static float reach_stage_layout_shape_error(const reach_stage_layout *layout, reach_rect_f32 area)
{
    float width = 0.0f;
    for (size_t row = 0; row < layout->row_count; ++row)
    {
        width = layout->rows[row].width > width ? layout->rows[row].width : width;
    }
    float height = reach_stage_layout_block_height(layout);
    if (width <= 0.0f || height <= 0.0f)
    {
        return 0.0f;
    }
    return fabsf(logf((width / height) / (area.width / area.height)));
}

// Leaves the layout scaled and split for the row count that gives the largest tiles. Tiles keep
// their floor while some row count fits it; otherwise the floor shrinks for every tile.
static void reach_stage_resolve_layout(reach_stage_layout *layout, reach_rect_f32 area)
{
    float fit_high = 0.0f;
    for (size_t index = 0; index < layout->count; ++index)
    {
        float full = 1.0f / layout->items[index].scale_per_fit;
        fit_high = full > fit_high ? full : fit_high;
    }

    size_t best_rows = 1;
    float best_fit = 0.0f;
    float best_floor = 0.0f;
    float best_size = -1.0f;
    float best_error = 0.0f;
    for (int32_t floor_factor = 0; floor_factor < 2 && best_size < 0.0f; ++floor_factor)
    {
        for (size_t rows = 1; rows <= layout->count; ++rows)
        {
            float fit = 0.0f;
            float floor = 1.0f;
            if (floor_factor)
            {
                floor = reach_stage_search_fit(layout, rows, 0.0f, 1.0f, 1, area);
            }
            else if (!reach_stage_layout_fits(layout, rows, 0.0f, 1.0f, area))
            {
                continue;
            }
            else if (reach_stage_layout_fits(layout, rows, fit_high, 1.0f, area))
            {
                fit = fit_high;
            }
            else
            {
                fit = reach_stage_search_fit(layout, rows, 0.0f, fit_high, 0, area);
            }
            float size = floor_factor ? floor : fit;
            (void)reach_stage_layout_fits(layout, rows, fit, floor, area);
            float error = reach_stage_layout_shape_error(layout, area);
            if (best_size < 0.0f || size > best_size * 1.0001f ||
                (size >= best_size * 0.9999f && error < best_error))
            {
                best_rows = rows;
                best_fit = fit;
                best_floor = floor;
                best_size = size;
                best_error = error;
            }
        }
    }
    (void)reach_stage_layout_fits(layout, best_rows, best_fit, best_floor, area);
}

// Moves a coordinate from its packed position toward its anchored one, kept inside [low, high].
static float reach_stage_pull(float packed, float anchored, float low, float high)
{
    float value = packed + (anchored - packed) * REACH_STAGE_ANCHOR_PULL;
    value = value > high ? high : value;
    return value < low ? low : value;
}

static void reach_stage_place_layout(reach_stage_state *state, reach_stage_layout *layout,
                                     reach_rect_f32 area)
{
    float block_height = reach_stage_layout_block_height(layout);
    float row_top = (area.height - block_height) * 0.5f;
    size_t ordered[REACH_STAGE_MAX_TILES] = {};
    for (size_t row = 0; row < layout->row_count; ++row)
    {
        const reach_stage_layout_row *line = &layout->rows[row];
        for (size_t at = 0; at < line->count; ++at)
        {
            size_t current = layout->order[line->begin + at];
            size_t to = at;
            while (to > 0 &&
                   layout->items[ordered[to - 1]].anchor.x > layout->items[current].anchor.x)
            {
                ordered[to] = ordered[to - 1];
                --to;
            }
            ordered[to] = current;
        }

        // A tile's shift from the left-packed row is wanted at its pulled target; shifts must
        // not decrease left to right (no overlap) and stay within the row's slack. Pooling
        // adjacent violators gives the closest such shifts, so equal anchors stay centred.
        float slack = area.width - line->width;
        float shift_sum[REACH_STAGE_MAX_TILES] = {};
        size_t shift_count[REACH_STAGE_MAX_TILES] = {};
        size_t blocks = 0;
        float offset = 0.0f;
        for (size_t at = 0; at < line->count; ++at)
        {
            const reach_stage_layout_item *item = &layout->items[ordered[at]];
            float anchored = item->anchor.x * area.width - item->thumbnail_width * 0.5f - offset;
            shift_sum[blocks] = reach_stage_pull(slack * 0.5f, anchored, -1.0e9f, 1.0e9f);
            shift_count[blocks] = 1;
            ++blocks;
            while (blocks > 1 && shift_sum[blocks - 2] * (float)shift_count[blocks - 1] >
                                     shift_sum[blocks - 1] * (float)shift_count[blocks - 2])
            {
                shift_sum[blocks - 2] += shift_sum[blocks - 1];
                shift_count[blocks - 2] += shift_count[blocks - 1];
                --blocks;
            }
            offset += item->thumbnail_width + layout->gap;
        }

        offset = 0.0f;
        size_t at = 0;
        for (size_t block = 0; block < blocks; ++block)
        {
            float shift = shift_sum[block] / (float)shift_count[block];
            shift = shift > slack ? slack : (shift < 0.0f ? 0.0f : shift);
            for (size_t member = 0; member < shift_count[block]; ++member, ++at)
            {
                reach_stage_layout_item *item = &layout->items[ordered[at]];
                reach_stage_tile *tile = &state->tiles[item->tile];
                tile->target_rect.x = area.x + offset + shift;
                offset += item->thumbnail_width + layout->gap;
            }
        }

        for (size_t member = 0; member < line->count; ++member)
        {
            reach_stage_layout_item *item = &layout->items[ordered[member]];
            reach_stage_tile *tile = &state->tiles[item->tile];

            float spare = line->height - item->box_height;
            float anchored_y = item->anchor.y * area.height - row_top - item->box_height * 0.5f;
            float y = reach_stage_pull(spare * 0.5f, anchored_y, 0.0f, spare);

            tile->bar_height = item->bar;
            tile->target_rect.y = area.y + row_top + y + item->bar + layout->border;
            tile->target_rect.width = item->thumbnail_width;
            tile->target_rect.height = item->thumbnail_height;
        }
        row_top += line->height + layout->gap;
    }
}

static reach_rect_f32 reach_stage_fit_into_box(reach_rect_f32 box, reach_rect_f32 source)
{
    if (source.width <= 0.0f || source.height <= 0.0f)
    {
        return box;
    }

    float scale_x = box.width / source.width;
    float scale_y = box.height / source.height;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    reach_rect_f32 fitted = {};
    fitted.width = source.width * scale;
    fitted.height = source.height * scale;
    fitted.x = box.x + (box.width - fitted.width) * 0.5f;
    fitted.y = box.y + (box.height - fitted.height) * 0.5f;
    return fitted;
}

static void reach_stage_place_desktop(reach_stage_tile *tile, reach_rect_f32 area,
                                      float border_thickness, float dpi_scale)
{
    if (tile == nullptr)
    {
        return;
    }

    float edge_gap = REACH_STAGE_DESKTOP_EDGE_GAP * dpi_scale;
    area.y += edge_gap;
    area.height -= edge_gap * 2.0f;
    if (area.width <= 0.0f || area.height <= 0.0f)
    {
        tile->target_rect = {};
        return;
    }

    float box_width = tile->monitor_portrait ? REACH_STAGE_BOX_SHORT : REACH_STAGE_BOX_LONG;
    float box_height = tile->monitor_portrait ? REACH_STAGE_BOX_LONG : REACH_STAGE_BOX_SHORT;
    float scale_x = area.width / box_width;
    float scale_y = area.height / box_height;
    float scale = scale_x < scale_y ? scale_x : scale_y;

    reach_rect_f32 box = {};
    box.width = box_width * scale;
    box.height = box_height * scale;
    box.x = area.x + (area.width - box.width) * 0.5f;
    box.y = area.y + (area.height - box.height) * 0.5f;

    reach_rect_f32 inner = box;
    inner.x += border_thickness;
    inner.y += border_thickness;
    inner.width -= border_thickness * 2.0f;
    inner.height -= border_thickness * 2.0f;

    tile->bar_height = 0.0f;
    tile->target_rect = reach_stage_fit_into_box(inner, tile->source_rect);
}

static reach_rect_f32 reach_stage_scale_rect(reach_rect_f32 rect, float scale)
{
    reach_rect_f32 out = {};
    out.width = rect.width * scale;
    out.height = rect.height * scale;
    out.x = rect.x + (rect.width - out.width) * 0.5f;
    out.y = rect.y + (rect.height - out.height) * 0.5f;
    return out;
}

reach_rect_f32 reach_stage_interpolate_rect(reach_rect_f32 from, reach_rect_f32 to, float factor)
{
    if (factor >= 1.0f)
    {
        return to;
    }
    if (factor <= 0.0f)
    {
        return from;
    }
    reach_rect_f32 out = {};
    out.x = from.x + (to.x - from.x) * factor;
    out.y = from.y + (to.y - from.y) * factor;
    out.width = from.width + (to.width - from.width) * factor;
    out.height = from.height + (to.height - from.height) * factor;
    return out;
}

// Minimized windows rise from and sink back below the bottom edge, centred under the Dock.
reach_rect_f32 reach_stage_minimized_offscreen_rect(const reach_stage_state *state,
                                                    reach_rect_f32 rect)
{
    rect.x = state->bounds.x + (state->bounds.width - rect.width) * 0.5f;
    rect.y = state->bounds.y + state->bounds.height;
    return rect;
}

void reach_stage_rebuild_layout(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    if (state->tile_count == 0)
    {
        return;
    }

    float dpi_scale = state->dpi_scale > 0.0f ? state->dpi_scale : 1.0f;
    float border_thickness = reach_theme_border_thickness(reach_theme_default(), dpi_scale);
    reach_stage_tile *desktop = reach_stage_desktop_tile(state);
    reach_stage_place_desktop(desktop, state->desktop_bounds, border_thickness, dpi_scale);
    reach_rect_f32 app_bounds = desktop != nullptr && desktop->target_rect.width > 0.0f &&
                                        desktop->target_rect.height > 0.0f
                                    ? desktop->target_rect
                                    : state->desktop_bounds;
    reach_rect_f32 area = reach_stage_content_area(app_bounds, dpi_scale);
    if (area.width <= 0.0f || area.height <= 0.0f)
    {
        return;
    }

    static reach_stage_layout layout;
    layout = {};
    layout.bar_height = reach_stage_tile_bar_height(state);
    layout.border = border_thickness;
    layout.gap = REACH_STAGE_TILE_GAP * dpi_scale;
    reach_stage_collect_layout_items(state, dpi_scale, &layout);
    if (layout.count == 0)
    {
        return;
    }
    reach_stage_resolve_layout(&layout, area);
    reach_stage_place_layout(state, &layout, area);
}

void reach_stage_apply_progress(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];

        float presence_target = tile->departing ? 0.0f : 1.0f;
        float presence_progress = tile->departing ? state->departure_progress : state->reflow;
        tile->presence =
            tile->presence_from + (presence_target - tile->presence_from) * presence_progress;

        reach_rect_f32 resolved =
            reach_stage_interpolate_rect(tile->reflow_from, tile->target_rect, state->reflow);
        if (tile->presence < 1.0f)
        {
            resolved = reach_stage_scale_rect(
                resolved, REACH_STAGE_PRESENCE_MIN_SCALE +
                              (1.0f - REACH_STAGE_PRESENCE_MIN_SCALE) * tile->presence);
        }

        float progress = tile->desktop ? state->desktop_progress : state->progress;
        if (state->closing && tile->close_retargeting)
        {
            tile->current_rect = reach_stage_interpolate_rect(
                tile->close_destination_rect, tile->close_from_rect, state->retarget_progress);
        }
        else if (state->closing)
        {
            float close_progress =
                tile->close_from_progress > 0.0f ? progress / tile->close_from_progress : 0.0f;
            if (close_progress > 1.0f)
            {
                close_progress = 1.0f;
            }
            tile->current_rect = reach_stage_interpolate_rect(
                tile->close_destination_rect, tile->close_from_rect, close_progress);
        }
        else if (tile->reopening)
        {
            float span = 1.0f - tile->open_from_progress;
            float open_progress =
                span > 0.0001f ? (progress - tile->open_from_progress) / span : 1.0f;
            tile->current_rect =
                reach_stage_interpolate_rect(tile->open_from_rect, resolved, open_progress);
        }
        else
        {
            reach_rect_f32 from = tile->minimized && !tile->desktop
                                      ? reach_stage_minimized_offscreen_rect(state, resolved)
                                      : tile->source_rect;
            tile->current_rect = reach_stage_interpolate_rect(from, resolved, progress);
        }

        float bar = tile->bar_height * progress * tile->presence;
        tile->current_bar.x = tile->current_rect.x;
        tile->current_bar.y = tile->current_rect.y - bar;
        tile->current_bar.width = tile->current_rect.width;
        tile->current_bar.height = bar;
    }
}
