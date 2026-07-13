#include "reach/support/animation.h"

static float reach_animation_clamp01(float value)
{
    if (value < 0.0f)
    {
        return 0.0f;
    }
    if (value > 1.0f)
    {
        return 1.0f;
    }
    return value;
}

static float reach_animation_ease(float value, reach_easing easing)
{
    float t = reach_animation_clamp01(value);
    switch (easing)
    {
    case REACH_EASING_EASE_IN:
        return t * t * t;
    case REACH_EASING_EASE_OUT:
    {
        float inverse = 1.0f - t;
        return 1.0f - inverse * inverse * inverse;
    }
    case REACH_EASING_EASE_IN_OUT:
    default:
        return t < 0.5f
                   ? 4.0f * t * t * t
                   : 1.0f - ((-2.0f * t + 2.0f) * (-2.0f * t + 2.0f) * (-2.0f * t + 2.0f)) / 2.0f;
    }
}

static reach_animation_track *reach_animation_manager_track(reach_animation_manager *manager,
                                                            size_t track_id)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(manager == nullptr || track_id < manager->track_count);
    return manager != nullptr && manager->tracks != nullptr && track_id < manager->track_count
               ? &manager->tracks[track_id]
               : nullptr;
}

static const reach_animation_track *
reach_animation_manager_const_track(const reach_animation_manager *manager, size_t track_id)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(manager == nullptr || track_id < manager->track_count);
    return manager != nullptr && manager->tracks != nullptr && track_id < manager->track_count
               ? &manager->tracks[track_id]
               : nullptr;
}

void reach_animation_manager_init(reach_animation_manager *manager, reach_animation_track *tracks,
                                  size_t track_count)
{
    REACH_ASSERT(manager != nullptr);
    REACH_ASSERT(track_count == 0 || tracks != nullptr);
    if (manager == nullptr)
    {
        return;
    }

    manager->tracks = tracks;
    manager->track_count = tracks != nullptr ? track_count : 0;
    for (size_t index = 0; index < manager->track_count; ++index)
    {
        manager->tracks[index] = {};
    }
}

void reach_animation_manager_tick(reach_animation_manager *manager, double delta_seconds)
{
    if (manager == nullptr || manager->tracks == nullptr)
    {
        return;
    }
    if (delta_seconds < 0.0)
    {
        delta_seconds = 0.0;
    }

    for (size_t index = 0; index < manager->track_count; ++index)
    {
        reach_animation_track *track = &manager->tracks[index];
        if (!track->active)
        {
            continue;
        }

        track->elapsed_seconds += delta_seconds;
        float progress = track->duration_seconds > 0.0
                             ? (float)(track->elapsed_seconds / track->duration_seconds)
                             : 1.0f;
        if (progress >= 1.0f)
        {
            track->value = track->to;
            track->active = 0;
            continue;
        }

        float eased = reach_animation_ease(progress, track->easing);
        track->value = track->from + (track->to - track->from) * eased;
    }
}

double reach_animation_track_time_to_value(const reach_animation_track *track, float target_value)
{
    if (track == nullptr || !track->active || track->duration_seconds <= 0.0)
    {
        return 0.0;
    }
    const float span = track->to - track->from;
    if (span == 0.0f)
    {
        return 0.0;
    }

    float needed = (target_value - track->from) / span;
    if (needed <= 0.0f)
    {
        return 0.0;
    }
    if (needed >= 1.0f)
    {
        double remaining = track->duration_seconds - track->elapsed_seconds;
        return remaining > 0.0 ? remaining : 0.0;
    }

    float lo = (float)(track->elapsed_seconds / track->duration_seconds);
    float hi = 1.0f;
    if (reach_animation_ease(lo, track->easing) >= needed)
    {
        return 0.0;
    }
    for (int step = 0; step < 32; ++step)
    {
        float mid = (lo + hi) * 0.5f;
        if (reach_animation_ease(mid, track->easing) < needed)
        {
            lo = mid;
        }
        else
        {
            hi = mid;
        }
    }
    double cross_seconds = (double)hi * track->duration_seconds;
    double remaining = cross_seconds - track->elapsed_seconds;
    return remaining > 0.0 ? remaining : 0.0;
}

void reach_animation_manager_set(reach_animation_manager *manager, size_t track_id, float value)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track != nullptr)
    {
        *track = {value, value, value, 0.0, 0.0, REACH_EASING_EASE_IN_OUT, 0};
    }
}

void reach_animation_manager_start(reach_animation_manager *manager, size_t track_id, float from,
                                   float to, double duration_seconds, reach_easing easing)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track == nullptr)
    {
        return;
    }

    if (duration_seconds <= 0.0 || from == to)
    {
        reach_animation_manager_set(manager, track_id, to);
        return;
    }

    *track = {from, to, from, 0.0, duration_seconds, easing, 1};
}

void reach_animation_manager_animate_to(reach_animation_manager *manager, size_t track_id, float to,
                                        double duration_seconds, reach_easing easing)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track != nullptr)
    {
        reach_animation_manager_start(manager, track_id, track->value, to, duration_seconds,
                                      easing);
    }
}

void reach_animation_manager_reset(reach_animation_manager *manager, size_t track_id)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track != nullptr)
    {
        *track = {};
    }
}

float reach_animation_manager_value(const reach_animation_manager *manager, size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr ? track->value : 0.0f;
}

float reach_animation_manager_target(const reach_animation_manager *manager, size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr ? track->to : 0.0f;
}

int32_t reach_animation_manager_active(const reach_animation_manager *manager, size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr && track->active;
}

int32_t reach_animation_manager_any_active(const reach_animation_manager *manager)
{
    if (manager == nullptr || manager->tracks == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; index < manager->track_count; ++index)
    {
        if (manager->tracks[index].active)
        {
            return 1;
        }
    }
    return 0;
}
