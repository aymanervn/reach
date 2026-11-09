#include "reach/support/animation.h"
#include "reach/support/workload.h"

#include <cmath>

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

float reach_easing_apply(float value, reach_easing easing)
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
    case REACH_EASING_EXPONENTIAL_OUT:
        return (float)(-std::expm1(-12.0 * t) / -std::expm1(-12.0));
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
        reach_motion_set(&manager->tracks[index].motion, 0);
    }
}

void reach_animation_manager_sample_at(reach_animation_manager *manager, double now_seconds)
{
    if (manager == nullptr || manager->tracks == nullptr || !std::isfinite(now_seconds))
        return;
    for (size_t index = 0; index < manager->track_count; ++index)
    {
        reach_animation_track *track = &manager->tracks[index];
        if (!track->active)
            continue;
        reach_workload_add(REACH_WORKLOAD_SCALAR_ADVANCES, 1);
        track->value = reach_motion_value_at(&track->motion, now_seconds);
        track->active = reach_motion_active_at(&track->motion, now_seconds);
    }
}

void reach_animation_manager_set(reach_animation_manager *manager, size_t track_id, float value)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track != nullptr)
    {
        reach_motion_set(&track->motion, value);
        track->value = value;
        track->active = 0;
    }
}

void reach_animation_manager_start(reach_animation_manager *manager, size_t track_id, float from,
                                   float to, double duration_seconds, reach_easing easing)
{
    reach_animation_manager_start_at(manager, track_id, from, to, duration_seconds, easing,
                                     reach_motion_now_seconds());
}

void reach_animation_manager_start_at(reach_animation_manager *manager, size_t track_id, float from,
                                      float to, double duration_seconds, reach_easing easing,
                                      double now_seconds)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track == nullptr)
        return;
    reach_motion_start_at(&track->motion, from, to, from == to ? 0.0 : duration_seconds, easing,
                          now_seconds);
    track->value = reach_motion_value_at(&track->motion, now_seconds);
    track->active = reach_motion_active_at(&track->motion, now_seconds);
}

void reach_animation_manager_animate_to(reach_animation_manager *manager, size_t track_id, float to,
                                        double duration_seconds, reach_easing easing)
{
    reach_animation_manager_animate_to_at(manager, track_id, to, duration_seconds, easing,
                                          reach_motion_now_seconds());
}

void reach_animation_manager_animate_to_at(reach_animation_manager *manager, size_t track_id,
                                           float to, double duration_seconds, reach_easing easing,
                                           double now_seconds)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track == nullptr)
        return;
    reach_animation_manager_start_at(manager, track_id,
                                     reach_motion_value_at(&track->motion, now_seconds), to,
                                     duration_seconds, easing, now_seconds);
}

void reach_animation_manager_spring_to_at(reach_animation_manager *manager, size_t track_id,
                                          float to, float response_seconds, float damping,
                                          double now_seconds)
{
    reach_animation_track *track = reach_animation_manager_track(manager, track_id);
    if (track == nullptr)
        return;
    reach_motion_spring_to_at(&track->motion, to, response_seconds, damping, now_seconds);
    track->value = reach_motion_value_at(&track->motion, now_seconds);
    track->active = reach_motion_active_at(&track->motion, now_seconds);
}

void reach_animation_manager_reset(reach_animation_manager *manager, size_t track_id)
{
    reach_animation_manager_set(manager, track_id, 0);
}

float reach_animation_manager_value(const reach_animation_manager *manager, size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr ? track->value : 0.0f;
}

float reach_animation_manager_target(const reach_animation_manager *manager, size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr ? track->motion.to : 0.0f;
}

const reach_motion *reach_animation_manager_motion(const reach_animation_manager *manager,
                                                   size_t track_id)
{
    const reach_animation_track *track = reach_animation_manager_const_track(manager, track_id);
    return track != nullptr ? &track->motion : nullptr;
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
