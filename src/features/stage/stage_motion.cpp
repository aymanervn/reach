#include "reach/features/stage.h"

#include "stage_common.h"

#include <math.h>

// Stage tile geometry expressed as compositor motions. Every expression mirrors
// reach_stage_apply_progress term by term, so the compositor plays exactly what the CPU path
// would sample. A tile is expressible only while a single track drives it; reach_motion composes
// motions of one timeline only.

namespace
{
constexpr float reach_stage_unbounded = 1.0e9f;

struct reach_stage_expression
{
    bool ok = true;
    uint32_t tracks = 0;
};

reach_motion reach_stage_constant(float value)
{
    reach_motion motion = {};
    reach_motion_set(&motion, value);
    return motion;
}

reach_rect_motion reach_stage_constant_rect(reach_rect_f32 rect)
{
    return {reach_stage_constant(rect.x), reach_stage_constant(rect.y),
            reach_stage_constant(rect.width), reach_stage_constant(rect.height)};
}

reach_motion reach_stage_track_motion(const reach_stage *stage, size_t track, double now,
                                      reach_stage_expression *expression)
{
    const reach_motion &motion = stage->motions[track];
    if (!reach_motion_active_at(&motion, now))
        return reach_stage_constant(reach_motion_value_at(&motion, now));
    expression->tracks |= 1u << track;
    return motion;
}

reach_motion reach_stage_sum(reach_stage_expression *expression, const reach_motion &first,
                             float first_factor, const reach_motion &second, float second_factor,
                             float offset = 0.0f)
{
    reach_motion out = {};
    if (expression->ok && reach_motion_combine(&first, first_factor, &second, second_factor, offset,
                                               &out) != REACH_OK)
        expression->ok = false;
    return out;
}

reach_motion reach_stage_product(reach_stage_expression *expression, const reach_motion &first,
                                 const reach_motion &second)
{
    reach_motion out = {};
    if (expression->ok && reach_motion_multiply(&first, &second, &out) != REACH_OK)
        expression->ok = false;
    return out;
}

reach_motion reach_stage_lerp(reach_stage_expression *expression, const reach_motion &from,
                              const reach_motion &to, const reach_motion &factor)
{
    reach_motion span = reach_stage_sum(expression, to, 1.0f, from, -1.0f);
    reach_motion travelled = reach_stage_product(expression, span, factor);
    return reach_stage_sum(expression, from, 1.0f, travelled, 1.0f);
}

// reach_stage_interpolate_rect clamps its factor to [0, 1].
reach_rect_motion reach_stage_lerp_rect(reach_stage_expression *expression,
                                        const reach_rect_motion &from, const reach_rect_motion &to,
                                        const reach_motion &factor)
{
    reach_motion clamped = reach_motion_clamp(&factor, 0.0f, 1.0f);
    return {reach_stage_lerp(expression, from.x, to.x, clamped),
            reach_stage_lerp(expression, from.y, to.y, clamped),
            reach_stage_lerp(expression, from.width, to.width, clamped),
            reach_stage_lerp(expression, from.height, to.height, clamped)};
}

bool reach_stage_rect_matches(reach_rect_f32 a, reach_rect_f32 b, float tolerance)
{
    return fabsf(a.x - b.x) <= tolerance && fabsf(a.y - b.y) <= tolerance &&
           fabsf(a.width - b.width) <= tolerance && fabsf(a.height - b.height) <= tolerance;
}
} // namespace

int32_t reach_stage_tile_motion_at(const reach_stage *stage, size_t index, double now,
                                   reach_stage_tile_motion *out)
{
    if (stage == nullptr || out == nullptr || index >= stage->state.tile_count)
        return 0;
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    reach_stage_expression expression;

    reach_motion progress = reach_stage_track_motion(
        stage,
        tile->desktop ? REACH_STAGE_ANIMATION_DESKTOP_PROGRESS : REACH_STAGE_ANIMATION_PROGRESS,
        now, &expression);
    reach_motion presence_driver = reach_stage_track_motion(
        stage, tile->departing ? REACH_STAGE_ANIMATION_DEPARTURE : REACH_STAGE_ANIMATION_REFLOW,
        now, &expression);
    float presence_target = tile->departing ? 0.0f : 1.0f;
    reach_motion presence = reach_motion_affine(
        &presence_driver, presence_target - tile->presence_from, tile->presence_from);

    reach_motion reflow =
        reach_rect_equal(tile->reflow_from, tile->target_rect)
            ? reach_stage_constant(1.0f)
            : reach_stage_track_motion(stage, REACH_STAGE_ANIMATION_REFLOW, now, &expression);
    reach_rect_motion resolved =
        reach_stage_lerp_rect(&expression, reach_stage_constant_rect(tile->reflow_from),
                              reach_stage_constant_rect(tile->target_rect), reflow);

    // Presence below one shrinks the tile about its centre; at one the scale is the identity.
    reach_motion bounded_presence = reach_motion_clamp(&presence, -reach_stage_unbounded, 1.0f);
    reach_motion shrink = reach_motion_affine(
        &bounded_presence, 1.0f - REACH_STAGE_PRESENCE_MIN_SCALE, REACH_STAGE_PRESENCE_MIN_SCALE);
    reach_motion width = reach_stage_product(&expression, resolved.width, shrink);
    reach_motion height = reach_stage_product(&expression, resolved.height, shrink);
    resolved = {
        reach_stage_sum(&expression, resolved.x, 1.0f,
                        reach_stage_sum(&expression, resolved.width, 0.5f, width, -0.5f), 1.0f),
        reach_stage_sum(&expression, resolved.y, 1.0f,
                        reach_stage_sum(&expression, resolved.height, 0.5f, height, -0.5f), 1.0f),
        width, height};

    reach_rect_motion rect = {};
    if (state->closing && tile->close_retargeting)
    {
        reach_motion retarget =
            reach_stage_track_motion(stage, REACH_STAGE_ANIMATION_RETARGET, now, &expression);
        rect = reach_stage_lerp_rect(&expression,
                                     reach_stage_constant_rect(tile->close_destination_rect),
                                     reach_stage_constant_rect(tile->close_from_rect), retarget);
    }
    else if (state->closing)
    {
        reach_motion close =
            tile->close_from_progress > 0.0f
                ? reach_motion_affine(&progress, 1.0f / tile->close_from_progress, 0.0f)
                : reach_stage_constant(0.0f);
        rect = reach_stage_lerp_rect(&expression,
                                     reach_stage_constant_rect(tile->close_destination_rect),
                                     reach_stage_constant_rect(tile->close_from_rect),
                                     reach_motion_clamp(&close, -reach_stage_unbounded, 1.0f));
    }
    else if (tile->reopening)
    {
        float span = 1.0f - tile->open_from_progress;
        reach_motion open = span > 0.0001f ? reach_motion_affine(&progress, 1.0f / span,
                                                                 -tile->open_from_progress / span)
                                           : reach_stage_constant(1.0f);
        rect = reach_stage_lerp_rect(&expression, reach_stage_constant_rect(tile->open_from_rect),
                                     resolved, open);
    }
    else
    {
        reach_rect_motion from = reach_stage_constant_rect(tile->source_rect);
        if (tile->minimized && !tile->desktop)
            from = {reach_motion_affine(&resolved.width, -1.0f, state->bounds.x),
                    reach_motion_affine(&resolved.height, -1.0f, state->bounds.y), resolved.width,
                    resolved.height};
        rect = reach_stage_lerp_rect(&expression, from, resolved, progress);
    }

    reach_motion scaled_progress = reach_motion_affine(&progress, tile->bar_height, 0.0f);
    reach_motion bar = reach_stage_product(&expression, scaled_progress, presence);
    reach_motion app_progress =
        reach_stage_track_motion(stage, REACH_STAGE_ANIMATION_PROGRESS, now, &expression);
    reach_motion alpha = reach_stage_product(&expression, app_progress, presence);
    if (!expression.ok)
        return 0;

    out->rect = rect;
    out->bar = bar;
    out->alpha = alpha;
    out->presence = presence;
    out->tracks = expression.tracks;

    // The compositor must reproduce the CPU sample exactly; anything else falls back to sampling.
    reach_rect_f32 sampled = reach_rect_motion_value_at(&rect, now);
    return reach_stage_rect_matches(sampled, tile->current_rect, 0.5f) &&
                   fabsf(reach_motion_value_at(&bar, now) - tile->current_bar.height) <= 0.5f
               ? 1
               : 0;
}

double reach_stage_motion_event_seconds(const reach_stage *stage,
                                        const reach_stage_tile_motion *motions, double now)
{
    static const size_t springs[] = {REACH_STAGE_ANIMATION_PROGRESS,
                                     REACH_STAGE_ANIMATION_DESKTOP_PROGRESS,
                                     REACH_STAGE_ANIMATION_REFLOW};
    uint32_t spring_mask = 0;
    double spring_end = 0.0;
    for (size_t track : springs)
        if (reach_motion_active_at(&stage->motions[track], now))
        {
            spring_mask |= 1u << track;
            spring_end = fmax(spring_end, reach_motion_deadline(&stage->motions[track]));
        }

    double next = 0.0;
    for (size_t track = 0; track < REACH_STAGE_ANIMATION_COUNT; ++track)
        if ((spring_mask & (1u << track)) == 0 &&
            reach_motion_active_at(&stage->motions[track], now))
        {
            double deadline = reach_motion_deadline(&stage->motions[track]);
            next = next <= 0.0 ? deadline : fmin(next, deadline);
        }
    if (spring_mask == 0)
        return next;

    // reach_stage_finish_settled_springs snaps every spring once each spring-driven tile is
    // within 2px of rest. The springs are critically damped, so that distance only shrinks and
    // the first such moment can be bisected.
    auto settled = [&](double at)
    {
        for (size_t index = 0; index < stage->state.tile_count; ++index)
        {
            const reach_stage_tile_motion &motion = motions[index];
            if ((motion.tracks & spring_mask) == 0)
                continue;
            reach_rect_f32 now_rect = reach_rect_motion_value_at(&motion.rect, at);
            reach_rect_f32 end_rect = reach_rect_motion_value_at(&motion.rect, spring_end);
            float now_bar = reach_motion_value_at(&motion.bar, at);
            float end_bar = reach_motion_value_at(&motion.bar, spring_end);
            float now_presence = reach_motion_value_at(&motion.presence, at);
            float end_presence = reach_motion_value_at(&motion.presence, spring_end);
            if (!reach_stage_rect_matches(now_rect, end_rect, 1.999f) ||
                fabsf(now_bar - end_bar) >= 2.0f ||
                fabsf(now_presence - end_presence) >= 1.0f / 512.0f)
                return false;
        }
        return true;
    };
    double low = now, high = spring_end;
    if (!settled(high))
        return next <= 0.0 ? spring_end : fmin(next, spring_end);
    for (int iteration = 0; iteration < 20 && high - low > 0.0005; ++iteration)
    {
        double middle = (low + high) * 0.5;
        if (settled(middle))
            high = middle;
        else
            low = middle;
    }
    return next <= 0.0 ? high : fmin(next, high);
}
