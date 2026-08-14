#include "reach/features/stage.h"

#include "stage_common.h"

#define REACH_STAGE_MAX_SECTIONS 8
#define REACH_STAGE_BOX_LONG 16.0f
#define REACH_STAGE_BOX_SHORT 9.0f

typedef struct reach_stage_section
{
    int32_t portrait;
    size_t indices[REACH_STAGE_MAX_TILES];
    size_t count;
    size_t columns;
    size_t rows;
} reach_stage_section;

typedef struct reach_stage_sections
{
    reach_stage_section entries[REACH_STAGE_MAX_SECTIONS];
    size_t count;
} reach_stage_sections;

static float reach_stage_box_width(const reach_stage_section *section)
{
    return section->portrait ? REACH_STAGE_BOX_SHORT : REACH_STAGE_BOX_LONG;
}

static float reach_stage_box_height(const reach_stage_section *section)
{
    return section->portrait ? REACH_STAGE_BOX_LONG : REACH_STAGE_BOX_SHORT;
}

static reach_rect_f32 reach_stage_content_area(reach_rect_f32 monitor_bounds, float dpi_scale)
{
    float inset_x = monitor_bounds.width * 0.06f + 24.0f * dpi_scale;
    float inset_y = monitor_bounds.height * 0.08f + 24.0f * dpi_scale;

    reach_rect_f32 area = {};
    area.x = monitor_bounds.x + inset_x;
    area.y = monitor_bounds.y + inset_y;
    area.width = monitor_bounds.width - inset_x * 2.0f;
    area.height = monitor_bounds.height - inset_y * 2.0f;
    return area;
}

static void reach_stage_collect_sections(const reach_stage_state *state,
                                         reach_stage_sections *sections)
{
    reach_stage_section buckets[REACH_STAGE_MAX_SECTIONS] = {};

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        const reach_stage_tile *tile = &state->tiles[index];
        size_t rank = (size_t)tile->monitor_index;
        if (rank >= REACH_STAGE_MAX_SECTIONS)
        {
            rank = REACH_STAGE_MAX_SECTIONS - 1;
        }

        reach_stage_section *section = &buckets[rank];
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
}

static float reach_stage_solve_box_scale(const reach_stage_sections *sections, reach_rect_f32 area,
                                         float gap)
{
    float width_per_scale = 0.0f;
    float fixed_width = gap * (float)(sections->count - 1);

    for (size_t index = 0; index < sections->count; ++index)
    {
        const reach_stage_section *section = &sections->entries[index];
        width_per_scale += (float)section->columns * reach_stage_box_width(section);
        fixed_width += gap * (float)(section->columns - 1);
    }

    float scale = (area.width - fixed_width) / width_per_scale;

    for (size_t index = 0; index < sections->count; ++index)
    {
        const reach_stage_section *section = &sections->entries[index];
        float limit = (area.height - gap * (float)(section->rows - 1)) /
                      ((float)section->rows * reach_stage_box_height(section));
        if (limit < scale)
        {
            scale = limit;
        }
    }

    return scale > 0.0f ? scale : 0.0f;
}

static float reach_stage_section_width(const reach_stage_section *section, float scale, float gap)
{
    return (float)section->columns * reach_stage_box_width(section) * scale +
           gap * (float)(section->columns - 1);
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

static void reach_stage_place_section(reach_stage_state *state, const reach_stage_section *section,
                                      float scale, float gap, float origin_x, reach_rect_f32 area)
{
    float box_width = reach_stage_box_width(section) * scale;
    float box_height = reach_stage_box_height(section) * scale;
    float section_width = reach_stage_section_width(section, scale, gap);
    float section_height = (float)section->rows * box_height + gap * (float)(section->rows - 1);
    float origin_y = area.y + (area.height - section_height) * 0.5f;

    for (size_t index = 0; index < section->count; ++index)
    {
        size_t column = index % section->columns;
        size_t row = index / section->columns;

        size_t remaining = section->count - row * section->columns;
        size_t row_items = remaining < section->columns ? remaining : section->columns;
        float row_width = (float)row_items * box_width + gap * (float)(row_items - 1);

        reach_rect_f32 box = {};
        box.x = origin_x + (section_width - row_width) * 0.5f + (float)column * (box_width + gap);
        box.y = origin_y + (float)row * (box_height + gap);
        box.width = box_width;
        box.height = box_height;

        reach_stage_tile *tile = &state->tiles[section->indices[index]];
        tile->target_rect = reach_stage_fit_into_box(box, tile->source_rect);
    }
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
    float gap = 56.0f * dpi_scale;
    reach_rect_f32 area = reach_stage_content_area(state->bounds, dpi_scale);

    reach_stage_sections sections = {};
    reach_stage_collect_sections(state, &sections);

    float scale = reach_stage_solve_box_scale(&sections, area, gap);

    float total_width = gap * (float)(sections.count - 1);
    for (size_t index = 0; index < sections.count; ++index)
    {
        total_width += reach_stage_section_width(&sections.entries[index], scale, gap);
    }

    float x = area.x + (area.width - total_width) * 0.5f;
    for (size_t index = 0; index < sections.count; ++index)
    {
        const reach_stage_section *section = &sections.entries[index];
        reach_stage_place_section(state, section, scale, gap, x, area);
        x += reach_stage_section_width(section, scale, gap) + gap;
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
        tile->current_rect =
            reach_stage_interpolate_rect(tile->source_rect, tile->target_rect, state->progress);
    }
}
