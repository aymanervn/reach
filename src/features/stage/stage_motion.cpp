#include "reach/features/stage.h"

#include "stage_common.h"

#include <math.h>

// Stage tile geometry is written once (reach_stage_tile_geometry) over two kinds of arithmetic:
// plain numbers sampled from the tracks for the CPU path, and compositor motions so the
// compositor plays exactly what the CPU path would sample. A tile is expressible as motion only
// while a single track drives it; reach_motion composes motions of one timeline only.

namespace
{
constexpr float reach_stage_unbounded = 1.0e9f;

// The sampled state as plain numbers. Each operation keeps the exact float arithmetic of the
// sampled path, so settled rects land on the same values the layout produced.
struct reach_stage_scalar_math
{
    using value = float;
    using rect = reach_rect_f32;

    const reach_stage_state *state;

    value constant(float number) const
    {
        return number;
    }
    rect constant_rect(reach_rect_f32 source) const
    {
        return source;
    }
    value track(size_t track) const
    {
        switch (track)
        {
        case REACH_STAGE_ANIMATION_PROGRESS:
            return state->progress;
        case REACH_STAGE_ANIMATION_REFLOW:
            return state->reflow;
        case REACH_STAGE_ANIMATION_DEPARTURE:
            return state->departure_progress;
        default:
            return state->retarget_progress;
        }
    }
    value affine(value input, float scale, float offset) const
    {
        return input * scale + offset;
    }
    value product(value first, value second) const
    {
        return first * second;
    }
    value at_most(value input, float maximum) const
    {
        return input > maximum ? maximum : input;
    }
    value divide(value input, float divisor) const
    {
        return input / divisor;
    }
    value normalize(value input, float from, float span) const
    {
        return (input - from) / span;
    }
    rect lerp_rect(rect from, rect to, value factor) const
    {
        return reach_stage_interpolate_rect(from, to, factor);
    }
    // Presence below one shrinks the rect about its centre; at one it is unchanged.
    rect shrink(rect source, value presence) const
    {
        if (presence >= 1.0f)
            return source;
        float scale =
            REACH_STAGE_PRESENCE_MIN_SCALE + (1.0f - REACH_STAGE_PRESENCE_MIN_SCALE) * presence;
        rect out = {};
        out.width = source.width * scale;
        out.height = source.height * scale;
        out.x = source.x + (source.width - out.width) * 0.5f;
        out.y = source.y + (source.height - out.height) * 0.5f;
        return out;
    }
    rect minimized_from(rect resolved) const
    {
        return reach_stage_minimized_offscreen_rect(state, resolved);
    }
};

// The same geometry as compositor motions, recording which tracks it depends on. Any
// composition the motion system cannot express marks the whole expression unusable.
struct reach_stage_motion_math
{
    using value = reach_motion;
    using rect = reach_rect_motion;

    const reach_stage *stage;
    double now;
    bool ok = true;
    uint32_t tracks = 0;

    value constant(float number) const
    {
        reach_motion motion = {};
        reach_motion_set(&motion, number);
        return motion;
    }
    rect constant_rect(reach_rect_f32 source) const
    {
        return {constant(source.x), constant(source.y), constant(source.width),
                constant(source.height)};
    }
    value track(size_t track)
    {
        const reach_motion &motion = stage->motions[track];
        if (!reach_motion_active_at(&motion, now))
            return constant(reach_motion_value_at(&motion, now));
        tracks |= 1u << track;
        return motion;
    }
    value affine(const value &input, float scale, float offset) const
    {
        return reach_motion_affine(&input, scale, offset);
    }
    value sum(const value &first, float first_factor, const value &second, float second_factor,
              float offset = 0.0f)
    {
        reach_motion out = {};
        if (ok && reach_motion_combine(&first, first_factor, &second, second_factor, offset,
                                       &out) != REACH_OK)
            ok = false;
        return out;
    }
    value product(const value &first, const value &second)
    {
        reach_motion out = {};
        if (ok && reach_motion_multiply(&first, &second, &out) != REACH_OK)
            ok = false;
        return out;
    }
    value at_most(const value &input, float maximum) const
    {
        return reach_motion_clamp(&input, -reach_stage_unbounded, maximum);
    }
    value divide(const value &input, float divisor) const
    {
        return affine(input, 1.0f / divisor, 0.0f);
    }
    value normalize(const value &input, float from, float span) const
    {
        return affine(input, 1.0f / span, -from / span);
    }
    value lerp(const value &from, const value &to, const value &factor)
    {
        value span = sum(to, 1.0f, from, -1.0f);
        value travelled = product(span, factor);
        return sum(from, 1.0f, travelled, 1.0f);
    }
    // Like reach_stage_interpolate_rect, the factor is clamped to [0, 1].
    rect lerp_rect(const rect &from, const rect &to, const value &factor)
    {
        reach_motion clamped = reach_motion_clamp(&factor, 0.0f, 1.0f);
        return {lerp(from.x, to.x, clamped), lerp(from.y, to.y, clamped),
                lerp(from.width, to.width, clamped), lerp(from.height, to.height, clamped)};
    }
    rect shrink(const rect &source, const value &presence)
    {
        reach_motion bounded = reach_motion_clamp(&presence, -reach_stage_unbounded, 1.0f);
        reach_motion scale = reach_motion_affine(&bounded, 1.0f - REACH_STAGE_PRESENCE_MIN_SCALE,
                                                 REACH_STAGE_PRESENCE_MIN_SCALE);
        reach_motion width = product(source.width, scale);
        reach_motion height = product(source.height, scale);
        return {sum(source.x, 1.0f, sum(source.width, 0.5f, width, -0.5f), 1.0f),
                sum(source.y, 1.0f, sum(source.height, 0.5f, height, -0.5f), 1.0f), width, height};
    }
    rect minimized_from(const rect &resolved) const
    {
        const reach_stage_state *state = &stage->state;
        return {reach_motion_affine(&resolved.width, -0.5f,
                                    state->bounds.x + state->bounds.width * 0.5f),
                constant(state->bounds.y + state->bounds.height), resolved.width, resolved.height};
    }
};

template <typename Math> struct reach_stage_tile_shape
{
    typename Math::rect rect;
    typename Math::value bar;
    typename Math::value presence;
};

// Where a tile is at the current moment of its tracks: the layout target blended by reflow and
// presence, then the open, close, retarget or reopen transition from or to its window.
template <typename Math>
reach_stage_tile_shape<Math> reach_stage_tile_geometry(Math &math, const reach_stage_state *state,
                                                       const reach_stage_tile *tile)
{
    using value = typename Math::value;
    using rect = typename Math::rect;

    value presence_driver = math.track(tile->departing ? REACH_STAGE_ANIMATION_DEPARTURE
                                                       : REACH_STAGE_ANIMATION_REFLOW);
    float presence_target = tile->departing ? 0.0f : 1.0f;
    value presence =
        math.affine(presence_driver, presence_target - tile->presence_from, tile->presence_from);
    // The wallpaper stays exactly where the user sees it; only app tiles move.
    if (tile->desktop)
        return {math.constant_rect(tile->source_rect), math.constant(0.0f), presence};

    value progress = math.track(REACH_STAGE_ANIMATION_PROGRESS);

    // A tile whose layout did not move does not depend on the reflow track.
    value reflow = reach_rect_equal(tile->reflow_from, tile->target_rect)
                       ? math.constant(1.0f)
                       : math.track(REACH_STAGE_ANIMATION_REFLOW);
    rect resolved = math.shrink(math.lerp_rect(math.constant_rect(tile->reflow_from),
                                               math.constant_rect(tile->target_rect), reflow),
                                presence);

    rect current;
    if (state->closing && tile->close_retargeting)
    {
        current = math.lerp_rect(math.constant_rect(tile->close_destination_rect),
                                 math.constant_rect(tile->close_from_rect),
                                 math.track(REACH_STAGE_ANIMATION_RETARGET));
    }
    else if (state->closing)
    {
        value close = tile->close_from_progress > 0.0f
                          ? math.at_most(math.divide(progress, tile->close_from_progress), 1.0f)
                          : math.constant(0.0f);
        current = math.lerp_rect(math.constant_rect(tile->close_destination_rect),
                                 math.constant_rect(tile->close_from_rect), close);
    }
    else if (tile->reopening)
    {
        float span = 1.0f - tile->open_from_progress;
        value open = span > 0.0001f ? math.normalize(progress, tile->open_from_progress, span)
                                    : math.constant(1.0f);
        current = math.lerp_rect(math.constant_rect(tile->open_from_rect), resolved, open);
    }
    else
    {
        rect from = tile->minimized && !tile->desktop ? math.minimized_from(resolved)
                                                      : math.constant_rect(tile->source_rect);
        current = math.lerp_rect(from, resolved, progress);
    }

    return {current, math.product(math.affine(progress, tile->bar_height, 0.0f), presence),
            presence};
}

} // namespace

void reach_stage_apply_progress(reach_stage *stage)
{
    REACH_ASSERT(stage != nullptr);
    if (stage == nullptr)
        return;

    reach_stage_state *state = &stage->state;
    reach_stage_scalar_math math = {state};
    for (size_t index = 0; index < state->tile_count; ++index)
    {
        reach_stage_tile *tile = &state->tiles[index];
        reach_stage_tile_shape<reach_stage_scalar_math> shape =
            reach_stage_tile_geometry(math, state, tile);
        tile->presence = shape.presence;
        tile->current_rect = shape.rect;
        tile->current_bar.x = shape.rect.x;
        tile->current_bar.y = shape.rect.y - shape.bar;
        tile->current_bar.width = shape.rect.width;
        tile->current_bar.height = shape.bar;
    }
}

int32_t reach_stage_tile_motion_at(const reach_stage *stage, size_t index, double now,
                                   reach_stage_tile_motion *out)
{
    if (stage == nullptr || out == nullptr || index >= stage->state.tile_count)
        return 0;
    const reach_stage_state *state = &stage->state;
    const reach_stage_tile *tile = &state->tiles[index];
    reach_stage_motion_math math = {stage, now};

    reach_stage_tile_shape<reach_stage_motion_math> shape =
        reach_stage_tile_geometry(math, state, tile);
    reach_motion alpha = math.product(math.track(REACH_STAGE_ANIMATION_PROGRESS), shape.presence);
    if (!math.ok)
        return 0;

    out->rect = shape.rect;
    out->bar = shape.bar;
    out->alpha = alpha;
    out->presence = shape.presence;
    out->tracks = math.tracks;

    // The compositor must reproduce the CPU sample exactly; anything else falls back to sampling.
    reach_rect_f32 sampled = reach_rect_motion_value_at(&shape.rect, now);
    return reach_stage_rect_within(sampled, tile->current_rect, 0.5f) &&
                   fabsf(reach_motion_value_at(&shape.bar, now) - tile->current_bar.height) <= 0.5f
               ? 1
               : 0;
}

double reach_stage_motion_event_seconds(const reach_stage *stage,
                                        const reach_stage_tile_motion *motions, double now)
{
    static const size_t springs[] = {REACH_STAGE_ANIMATION_PROGRESS, REACH_STAGE_ANIMATION_REFLOW};
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
            if (!reach_stage_rect_within(now_rect, end_rect, 1.999f) ||
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
