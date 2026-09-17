#include "reach/features/stage.h"

#include "stage_common.h"

#define REACH_STAGE_MAX_SECTIONS 8
#define REACH_STAGE_BOX_LONG 16.0f
#define REACH_STAGE_BOX_SHORT 9.0f
#define REACH_STAGE_BAR_HEIGHT 32.0f
#define REACH_STAGE_BAR_MAX_BOX_RATIO 0.20f
#define REACH_STAGE_PRESENCE_MIN_SCALE 0.88f
#define REACH_STAGE_DESKTOP_EDGE_GAP 20.0f
#define REACH_STAGE_CONTENT_INSET 24.0f
#define REACH_STAGE_TILE_GAP 20.0f
#define REACH_STAGE_SECTION_GAP 28.0f
#define REACH_STAGE_PORTRAIT_SCALE_WEIGHT 1.30f

typedef struct reach_stage_section
{
    size_t rank;
    int32_t portrait;
    size_t indices[REACH_STAGE_MAX_TILES];
    size_t count;
    size_t columns;
    size_t rows;
    float scale;
} reach_stage_section;

typedef struct reach_stage_sections
{
    reach_stage_section entries[REACH_STAGE_MAX_SECTIONS];
    size_t count;
} reach_stage_sections;

static void reach_stage_resolve_section_grid(reach_stage_section *section)
{
    if (section->portrait)
    {
        section->columns = 1;
        section->rows = section->count;
        return;
    }

    section->columns = 1;
    while (section->columns * section->columns < section->count)
    {
        section->columns++;
    }
    section->rows = section->count / section->columns;
    if (section->count % section->columns != 0)
    {
        section->rows++;
    }
}

static float reach_stage_box_height(float scale)
{
    return REACH_STAGE_BOX_SHORT * scale;
}

static float reach_stage_box_bar(float box_height, float bar_height)
{
    float limit = box_height * REACH_STAGE_BAR_MAX_BOX_RATIO;
    return bar_height < limit ? bar_height : limit;
}

static float reach_stage_tile_aspect(const reach_stage_tile *tile)
{
    return tile->source_rect.width > 0.0f && tile->source_rect.height > 0.0f
               ? tile->source_rect.width / tile->source_rect.height
               : REACH_STAGE_BOX_LONG / REACH_STAGE_BOX_SHORT;
}

static float reach_stage_tile_layout_width(const reach_stage_tile *tile, float box_height,
                                           float bar_height, float border_thickness)
{
    float height =
        box_height - reach_stage_box_bar(box_height, bar_height) - border_thickness * 2.0f;
    return height > 0.0f ? height * reach_stage_tile_aspect(tile) : 0.0f;
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

static void reach_stage_collect_sections(const reach_stage_state *state,
                                         reach_stage_sections *sections)
{
    reach_stage_section buckets[REACH_STAGE_MAX_SECTIONS] = {};

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        if (tile->departing || tile->desktop)
        {
            continue;
        }

        size_t rank = (size_t)tile->monitor_index;
        if (rank >= REACH_STAGE_MAX_SECTIONS)
        {
            rank = REACH_STAGE_MAX_SECTIONS - 1;
        }

        reach_stage_section *section = &buckets[rank];
        section->rank = rank;
        section->portrait = tile->monitor_portrait;
        section->indices[section->count++] = index;
    }

    sections->count = 0;
    for (size_t rank = 0; rank < REACH_STAGE_MAX_SECTIONS; ++rank)
    {
        if (buckets[rank].count == 0)
        {
            continue;
        }

        reach_stage_section *section = &sections->entries[sections->count++];
        *section = buckets[rank];

        for (size_t index = 1; index < section->count; ++index)
        {
            size_t current = section->indices[index];
            size_t at = index;
            while (at > 0)
            {
                size_t previous = section->indices[at - 1];
                const reach_rect_f32 *current_rect = &state->tiles[current].source_rect;
                const reach_rect_f32 *previous_rect = &state->tiles[previous].source_rect;
                if (previous_rect->y < current_rect->y ||
                    (previous_rect->y == current_rect->y && previous_rect->x <= current_rect->x))
                {
                    break;
                }
                section->indices[at] = previous;
                --at;
            }
            section->indices[at] = current;
        }

        reach_stage_resolve_section_grid(section);
    }
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

static float reach_stage_row_width(const reach_stage_state *state,
                                   const reach_stage_section *section, size_t row, float scale,
                                   float tile_gap, float bar_height, float border_thickness)
{
    size_t begin = row * section->columns;
    size_t remaining = section->count - begin;
    size_t count = remaining < section->columns ? remaining : section->columns;
    float width = tile_gap * (float)(count - 1);
    float box_height = reach_stage_box_height(scale);
    for (size_t index = 0; index < count; ++index)
    {
        width += reach_stage_tile_layout_width(&state->tiles[section->indices[begin + index]],
                                               box_height, bar_height, border_thickness);
    }
    return width;
}

static float reach_stage_section_width(const reach_stage_state *state,
                                       const reach_stage_section *section, float scale,
                                       float tile_gap, float bar_height, float border_thickness)
{
    float width = 0.0f;
    for (size_t row = 0; row < section->rows; ++row)
    {
        float row_width = reach_stage_row_width(state, section, row, scale, tile_gap, bar_height,
                                                border_thickness);
        if (row_width > width)
        {
            width = row_width;
        }
    }
    return width;
}

static float reach_stage_section_scale(const reach_stage_section *section, float factor)
{
    float weighted = factor * (section->portrait ? REACH_STAGE_PORTRAIT_SCALE_WEIGHT : 1.0f);
    return section->scale * (weighted < 1.0f ? weighted : 1.0f);
}

static float reach_stage_sections_width(const reach_stage_state *state,
                                        const reach_stage_sections *sections, float factor,
                                        float tile_gap, float section_gap, float bar_height,
                                        float border_thickness)
{
    float width = section_gap * (float)(sections->count - 1);
    for (size_t index = 0; index < sections->count; ++index)
    {
        const reach_stage_section *section = &sections->entries[index];
        width +=
            reach_stage_section_width(state, section, reach_stage_section_scale(section, factor),
                                      tile_gap, bar_height, border_thickness);
    }
    return width;
}

static void reach_stage_resolve_section_scales(const reach_stage_state *state,
                                               reach_stage_sections *sections, reach_rect_f32 area,
                                               float tile_gap, float section_gap, float bar_height,
                                               float border_thickness)
{
    for (size_t index = 0; index < sections->count; ++index)
    {
        reach_stage_section *section = &sections->entries[index];
        float scalable_height = (float)section->rows * REACH_STAGE_BOX_SHORT;
        float fixed_height = tile_gap * (float)(section->rows - 1);
        section->scale = scalable_height > 0.0f && area.height > fixed_height
                             ? (area.height - fixed_height) / scalable_height
                             : 0.0f;
    }

    if (reach_stage_sections_width(state, sections, 1.0f, tile_gap, section_gap, bar_height,
                                   border_thickness) <= area.width)
    {
        return;
    }

    float low = 0.0f;
    float high = 1.0f;
    for (size_t iteration = 0; iteration < 24; ++iteration)
    {
        float middle = (low + high) * 0.5f;
        if (reach_stage_sections_width(state, sections, middle, tile_gap, section_gap, bar_height,
                                       border_thickness) <= area.width)
        {
            low = middle;
        }
        else
        {
            high = middle;
        }
    }
    for (size_t index = 0; index < sections->count; ++index)
    {
        sections->entries[index].scale = reach_stage_section_scale(&sections->entries[index], low);
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

static void reach_stage_place_section(reach_stage_state *state, const reach_stage_section *section,
                                      float tile_gap, float origin_x, reach_rect_f32 area,
                                      float bar_height, float border_thickness)
{
    float box_height = reach_stage_box_height(section->scale);
    float section_width = reach_stage_section_width(state, section, section->scale, tile_gap,
                                                    bar_height, border_thickness);
    float section_height =
        (float)section->rows * box_height + tile_gap * (float)(section->rows - 1);
    float origin_y = area.y + (area.height - section_height) * 0.5f;

    for (size_t row = 0; row < section->rows; ++row)
    {
        size_t begin = row * section->columns;
        size_t remaining = section->count - begin;
        size_t count = remaining < section->columns ? remaining : section->columns;
        float row_width = reach_stage_row_width(state, section, row, section->scale, tile_gap,
                                                bar_height, border_thickness);
        float x = origin_x + (section_width - row_width) * 0.5f;
        float y = origin_y + (float)row * (box_height + tile_gap);
        float bar = reach_stage_box_bar(box_height, bar_height);
        float height = box_height - bar - border_thickness * 2.0f;
        if (height < 0.0f)
        {
            height = 0.0f;
        }

        for (size_t index = 0; index < count; ++index)
        {
            reach_stage_tile *tile = &state->tiles[section->indices[begin + index]];
            tile->bar_height = bar;
            tile->target_rect.x = x;
            tile->target_rect.y = y + bar + border_thickness;
            tile->target_rect.width = height * reach_stage_tile_aspect(tile);
            tile->target_rect.height = height;
            x += tile->target_rect.width + tile_gap;
        }
    }
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
    reach_rect_f32 out = {};
    out.x = from.x + (to.x - from.x) * factor;
    out.y = from.y + (to.y - from.y) * factor;
    out.width = from.width + (to.width - from.width) * factor;
    out.height = from.height + (to.height - from.height) * factor;
    return out;
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
    float tile_gap = REACH_STAGE_TILE_GAP * dpi_scale;
    float section_gap = REACH_STAGE_SECTION_GAP * dpi_scale;
    float border_thickness = reach_theme_border_thickness(reach_theme_default(), dpi_scale);
    reach_stage_tile *desktop = reach_stage_desktop_tile(state);
    reach_stage_place_desktop(desktop, state->desktop_bounds, border_thickness, dpi_scale);
    reach_rect_f32 app_bounds = desktop != nullptr && desktop->target_rect.width > 0.0f &&
                                        desktop->target_rect.height > 0.0f
                                    ? desktop->target_rect
                                    : state->desktop_bounds;
    reach_rect_f32 area = reach_stage_content_area(app_bounds, dpi_scale);

    reach_stage_sections sections = {};
    reach_stage_collect_sections(state, &sections);
    if (sections.count == 0)
    {
        return;
    }

    float bar_height = reach_stage_tile_bar_height(state);
    reach_stage_resolve_section_scales(state, &sections, area, tile_gap, section_gap, bar_height,
                                       border_thickness);

    float total_width = reach_stage_sections_width(state, &sections, 1.0f, tile_gap, section_gap,
                                                   bar_height, border_thickness);

    float x = area.x + (area.width - total_width) * 0.5f;
    for (size_t index = 0; index < sections.count; ++index)
    {
        const reach_stage_section *section = &sections.entries[index];
        reach_stage_place_section(state, section, tile_gap, x, area, bar_height, border_thickness);
        x += reach_stage_section_width(state, section, section->scale, tile_gap, bar_height,
                                       border_thickness) +
             section_gap;
    }
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
        tile->presence =
            tile->presence_from + (presence_target - tile->presence_from) * state->reflow;

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
                tile->source_rect, tile->close_from_rect, state->retarget_progress);
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
                tile->source_rect, tile->close_from_rect, close_progress);
        }
        else
        {
            tile->current_rect =
                reach_stage_interpolate_rect(tile->source_rect, resolved, progress);
        }

        float bar = tile->bar_height * progress * tile->presence;
        tile->current_bar.x = tile->current_rect.x;
        tile->current_bar.y = tile->current_rect.y - bar;
        tile->current_bar.width = tile->current_rect.width;
        tile->current_bar.height = bar;
    }
}
