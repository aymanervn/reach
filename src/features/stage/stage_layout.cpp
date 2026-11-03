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

typedef struct reach_stage_value
{
    reach_motion motion;
    float value;
    int32_t exact;
} reach_stage_value;

typedef struct reach_stage_rect_value
{
    reach_stage_value x;
    reach_stage_value y;
    reach_stage_value width;
    reach_stage_value height;
} reach_stage_rect_value;

static reach_stage_value reach_stage_constant(float value)
{
    reach_stage_value result = {};
    reach_motion_set(&result.motion, value);
    result.value = value;
    result.exact = 1;
    return result;
}

static reach_stage_value reach_stage_track(const reach_stage *stage, size_t track, float value)
{
    reach_stage_value result = reach_stage_constant(value);
    if (reach_animation_manager_active(&stage->animations, track))
    {
        result.motion = *reach_animation_manager_motion(&stage->animations, track);
    }
    return result;
}

static int32_t reach_stage_varies(const reach_stage_value *input)
{
    float minimum = 0.0f;
    float maximum = 0.0f;
    return input->exact && reach_motion_range(&input->motion, &minimum, &maximum) == REACH_OK &&
           minimum != maximum;
}

static reach_stage_value reach_stage_affine(reach_stage_value input, float multiplier, float offset)
{
    reach_stage_value result = {};
    result.value = input.value * multiplier + offset;
    result.exact = input.exact;
    if (input.exact)
    {
        result.motion = reach_motion_affine(&input.motion, multiplier, offset);
    }
    return result;
}

static reach_stage_value reach_stage_combine(reach_stage_value first, float first_factor,
                                             reach_stage_value second, float second_factor)
{
    reach_stage_value result = {};
    result.value = first_factor * first.value + second_factor * second.value;
    result.exact = first.exact && second.exact &&
                   reach_motion_combine(&first.motion, first_factor, &second.motion, second_factor,
                                        0.0f, &result.motion) == REACH_OK;
    return result;
}

static reach_stage_value reach_stage_product(reach_stage_value first, reach_stage_value second)
{
    reach_stage_value result = {};
    result.value = first.value * second.value;
    result.exact = first.exact && second.exact &&
                   reach_motion_multiply(&first.motion, &second.motion, &result.motion) == REACH_OK;
    return result;
}

static reach_stage_value reach_stage_lerp(reach_stage_value from, reach_stage_value to,
                                          reach_stage_value factor)
{
    reach_stage_value travel =
        reach_stage_product(reach_stage_combine(to, 1.0f, from, -1.0f), factor);
    reach_stage_value result = {};
    result.value = from.value + travel.value;
    result.exact = from.exact && travel.exact &&
                   reach_motion_combine(&from.motion, 1.0f, &travel.motion, 1.0f, 0.0f,
                                        &result.motion) == REACH_OK;
    return result;
}

static reach_stage_rect_value reach_stage_rect_constant(reach_rect_f32 rect)
{
    return {reach_stage_constant(rect.x), reach_stage_constant(rect.y),
            reach_stage_constant(rect.width), reach_stage_constant(rect.height)};
}

static reach_stage_rect_value reach_stage_rect_lerp(reach_stage_rect_value from,
                                                    reach_stage_rect_value to,
                                                    reach_stage_value factor)
{
    return {reach_stage_lerp(from.x, to.x, factor), reach_stage_lerp(from.y, to.y, factor),
            reach_stage_lerp(from.width, to.width, factor),
            reach_stage_lerp(from.height, to.height, factor)};
}

static reach_stage_rect_value reach_stage_rect_scaled(reach_stage_rect_value rect,
                                                      reach_stage_value scale)
{
    reach_stage_rect_value result = {};
    result.width = reach_stage_product(rect.width, scale);
    result.height = reach_stage_product(rect.height, scale);
    reach_stage_value inset_x =
        reach_stage_affine(reach_stage_combine(rect.width, 1.0f, result.width, -1.0f), 0.5f, 0.0f);
    reach_stage_value inset_y = reach_stage_affine(
        reach_stage_combine(rect.height, 1.0f, result.height, -1.0f), 0.5f, 0.0f);
    result.x = reach_stage_combine(rect.x, 1.0f, inset_x, 1.0f);
    result.y = reach_stage_combine(rect.y, 1.0f, inset_y, 1.0f);
    return result;
}

static reach_motion reach_stage_publish(const reach_stage_value *input)
{
    if (reach_stage_varies(input))
    {
        return input->motion;
    }
    reach_motion result = {};
    reach_motion_set(&result, input->value);
    return result;
}

static reach_stage_value reach_stage_close_progress(const reach_stage_tile *tile,
                                                    reach_stage_value progress)
{
    if (tile->close_from_progress <= 0.0f)
    {
        return reach_stage_constant(0.0f);
    }
    reach_stage_value result = {};
    result.value = progress.value / tile->close_from_progress;
    result.exact = progress.exact;
    if (progress.exact)
    {
        result.motion =
            reach_motion_affine(&progress.motion, 1.0f / tile->close_from_progress, 0.0f);
        result.motion = reach_motion_clamp(&result.motion, 0.0f, 1.0f);
    }
    if (result.value > 1.0f)
    {
        result.value = 1.0f;
    }
    return result;
}

void reach_stage_apply_progress(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
    {
        return;
    }

    reach_stage_state *state = &stage->state;
    reach_stage_value apps_progress =
        reach_stage_track(stage, REACH_STAGE_ANIMATION_PROGRESS, state->progress);
    reach_stage_value desktop_progress =
        reach_stage_track(stage, REACH_STAGE_ANIMATION_DESKTOP_PROGRESS, state->desktop_progress);
    reach_stage_value reflow =
        reach_stage_track(stage, REACH_STAGE_ANIMATION_REFLOW, state->reflow);
    reach_stage_value departure =
        reach_stage_track(stage, REACH_STAGE_ANIMATION_DEPARTURE, state->departure_progress);
    reach_stage_value retarget =
        reach_stage_track(stage, REACH_STAGE_ANIMATION_RETARGET, state->retarget_progress);

    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];

        float presence_target = tile->departing ? 0.0f : 1.0f;
        reach_stage_value presence =
            reach_stage_affine(tile->departing ? departure : reflow,
                               presence_target - tile->presence_from, tile->presence_from);
        tile->presence = presence.value;

        reach_stage_rect_value resolved =
            reach_stage_rect_lerp(reach_stage_rect_constant(tile->reflow_from),
                                  reach_stage_rect_constant(tile->target_rect), reflow);
        if (presence.value < 1.0f || reach_stage_varies(&presence))
        {
            resolved = reach_stage_rect_scaled(
                resolved, reach_stage_affine(presence, 1.0f - REACH_STAGE_PRESENCE_MIN_SCALE,
                                             REACH_STAGE_PRESENCE_MIN_SCALE));
        }

        reach_stage_value progress = tile->desktop ? desktop_progress : apps_progress;
        reach_stage_rect_value current = {};
        if (state->closing && tile->close_retargeting)
        {
            current =
                reach_stage_rect_lerp(reach_stage_rect_constant(tile->close_destination_rect),
                                      reach_stage_rect_constant(tile->close_from_rect), retarget);
        }
        else if (state->closing)
        {
            current = reach_stage_rect_lerp(reach_stage_rect_constant(tile->close_destination_rect),
                                            reach_stage_rect_constant(tile->close_from_rect),
                                            reach_stage_close_progress(tile, progress));
        }
        else
        {
            current = reach_stage_rect_lerp(reach_stage_rect_constant(tile->source_rect), resolved,
                                            progress);
        }

        reach_stage_value bar =
            reach_stage_product(reach_stage_affine(progress, tile->bar_height, 0.0f), presence);
        reach_stage_value fade = reach_stage_product(apps_progress, presence);

        tile->current_rect = {current.x.value, current.y.value, current.width.value,
                              current.height.value};
        tile->current_bar.x = tile->current_rect.x;
        tile->current_bar.y = tile->current_rect.y - bar.value;
        tile->current_bar.width = tile->current_rect.width;
        tile->current_bar.height = bar.value;
        tile->rect_motion = {reach_stage_publish(&current.x), reach_stage_publish(&current.y),
                             reach_stage_publish(&current.width),
                             reach_stage_publish(&current.height)};
        tile->bar_motion = reach_stage_publish(&bar);
        tile->fade_motion = reach_stage_publish(&fade);
    }
}
