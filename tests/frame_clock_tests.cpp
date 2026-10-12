#include "reach/core/frame_clock.h"
#include "reach/core/blink.h"
#include "reach/support/animation.h"

#include <cmath>
#include <initializer_list>
#include <stdio.h>

static int failures;

static void expect_true(int condition, const char *message)
{
    if (!condition)
    {
        ++failures;
        printf("FAIL: %s\n", message);
    }
}

static void test_frame_rate_policy(void)
{
    expect_true(reach_frame_rate_resolve(1, 240, 0) == 240, "adaptive has no 120 Hz cap");
    expect_true(reach_frame_rate_resolve(1, 240, 1) == 60,
                "battery saver overrides adaptive refresh");
    expect_true(reach_frame_rate_resolve(1, 240, 0) == 240,
                "leaving battery saver restores adaptive refresh");
    expect_true(reach_frame_rate_resolve(0, 240, 0) == 60 &&
                    reach_frame_rate_resolve(0, 240, 1) == 60,
                "adaptive off is always 60 Hz");
    expect_true(reach_frame_rate_resolve(1, 0, 0) == 60 && reach_frame_rate_resolve(1, -1, 0) == 60,
                "missing or invalid display refresh falls back to 60 Hz");
    expect_true(reach_frame_rate_resolve(1, 30, 0) == 30, "adaptive follows slower displays too");
}

static void test_deadlines_do_not_drift(void)
{
    const int32_t rates[] = {60, 144, 240};
    for (int32_t rate : rates)
    {
        reach_frame_clock clock = {};
        double now = 0.0;
        expect_true(reach_frame_clock_wait_ms(&clock, now, rate, 1) == 0,
                    "the first active frame is immediate");
        for (int frame = 0; frame < rate * 10; ++frame)
        {
            reach_frame_clock_presented(&clock, now);
            uint32_t wait = reach_frame_clock_wait_ms(&clock, now, rate, 1);
            expect_true(wait > 0, "animation pacing never spins at zero milliseconds");
            now += (double)wait / 1000.0;
        }
        expect_true(std::fabs(now - 10.0) < 0.002,
                    "rounded waits preserve the long-term target rate");
    }
}

static void test_rate_changes_stalls_and_idle(void)
{
    reach_frame_clock clock = {};
    reach_frame_clock_wait_ms(&clock, 0.0, 240, 1);
    reach_frame_clock_presented(&clock, 0.0);
    expect_true(reach_frame_clock_wait_ms(&clock, 0.001, 60, 1) == 17,
                "saver transition replaces the high-refresh deadline");
    double deadline = clock.next_frame_seconds;
    reach_frame_clock_presented(&clock, 0.002);
    expect_true(clock.next_frame_seconds == deadline,
                "early input-driven frames do not postpone animation deadlines");
    reach_frame_clock_presented(&clock, 5.0);
    expect_true(clock.next_frame_seconds > 5.0 && clock.next_frame_seconds < 5.017,
                "missed frames are skipped without a catch-up burst");
    expect_true(reach_frame_clock_wait_ms(&clock, 5.0, 240, 1) == 5,
                "leaving saver restores the display-rate deadline");
    expect_true(reach_frame_clock_wait_ms(&clock, 5.0, 240, 0) == UINT32_MAX && !clock.active,
                "idle resets the clock and permits indefinite sleep");
    expect_true(reach_frame_clock_wait_ms(&clock, 1000.0, 144, 1) == 0,
                "waking from idle starts a fresh immediate frame");
}

static void test_blink_deadlines_and_duty_cycle(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    expect_true(blink.visible && blink.deadline_seconds == 100.55,
                "focus starts with the original visible interval");
    expect_true(!reach_blink_update_at(&blink, 100.54) && blink.visible,
                "unrelated wakes before the deadline do not change the caret");
    expect_true(reach_blink_update_at(&blink, blink.deadline_seconds) && !blink.visible &&
                    blink.deadline_seconds == 101.0,
                "the exact boundary starts the original hidden interval");
    expect_true(reach_blink_update_at(&blink, 101.0) && blink.visible &&
                    blink.deadline_seconds == 101.55,
                "the cycle stays aligned to its focus epoch");
    reach_blink_reset_at(&blink, 0.53, 0.53, 100.0);
    expect_true(reach_blink_update_at(&blink, blink.deadline_seconds) && !blink.visible &&
                    std::fabs(blink.deadline_seconds - 101.06) < 0.000001,
                "symmetric Wi-Fi and Launcher timing is preserved");
}

static void test_blink_delayed_wake_reset_and_stop(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    expect_true(!reach_blink_update_at(&blink, 150.2) && blink.visible &&
                    blink.deadline_seconds == 150.55,
                "missed whole cycles need no catch-up frames");
    expect_true(reach_blink_update_at(&blink, 1000.8) && !blink.visible &&
                    blink.deadline_seconds == 1001.0,
                "a late wake samples the correct phase in constant time");
    reach_blink_reset_at(&blink, 0.55, 0.45, 1000.9);
    expect_true(blink.visible && std::fabs(blink.deadline_seconds - 1001.45) < 0.000001,
                "typing resets visibility and the epoch immediately");
    reach_blink_stop(&blink);
    expect_true(!blink.visible && blink.deadline_seconds == 0 &&
                    !reach_blink_update_at(&blink, 2000.0),
                "blur or hide retires the deadline");
    reach_blink_reset_at(&blink, blink.visible_seconds, blink.hidden_seconds, 2000.0);
    expect_true(blink.visible && blink.deadline_seconds == 2000.55,
                "resuming a visible editor reuses its configured duty cycle");
}

static void test_blink_invalid_or_unrepresentable_deadlines(void)
{
    reach_blink blink = {};
    reach_blink_reset_at(&blink, 0.55, 0.45, 100.0);
    double deadline = blink.deadline_seconds;
    expect_true(!reach_blink_update_at(&blink, NAN) && blink.deadline_seconds == deadline,
                "invalid samples do not change a live clock");
    reach_blink_reset_at(&blink, 0.0, 0.45, 100.0);
    expect_true(blink.deadline_seconds == 0, "invalid duration retires the clock");
    reach_blink_reset_at(&blink, 1e-30, 1e-30, 100.0);
    expect_true(blink.deadline_seconds == 0,
                "unrepresentable boundaries cannot create a busy deadline loop");
}

static void test_animation_absolute_sampling_and_final_update(void)
{
    reach_animation_track tracks[2] = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, tracks, 2);
    reach_animation_manager_start_at(&manager, 0, 0, 100, 2, REACH_EASING_EASE_OUT, 10);
    reach_animation_manager_start_at(&manager, 1, 0, 1, 4, REACH_EASING_EASE_IN_OUT, 10);
    reach_animation_manager_sample_at(&manager, 11);
    expect_true(std::fabs(reach_animation_manager_value(&manager, 0) - 87.5f) < 0.0001f,
                "scalar tracks sample elapsed absolute time with canonical easing");
    expect_true(std::fabs(reach_animation_manager_value(&manager, 1) - 0.0625f) < 0.0001f,
                "one event time samples all tracks independently of frame deltas");
    uint64_t generation = tracks[0].motion.generation;
    reach_animation_manager_sample_at(&manager, 11);
    expect_true(tracks[0].motion.generation == generation && tracks[0].value == 87.5f,
                "sampling does not restart the physical timeline");
    expect_true(reach_animation_manager_any_active(&manager),
                "cached activity retains the required final update after a delayed wake");
    reach_animation_manager_sample_at(&manager, 20);
    expect_true(tracks[0].value == 100 && tracks[1].value == 1 &&
                    !reach_animation_manager_any_active(&manager),
                "a late wake lands at both endpoints and retires frame demand");
}

static void test_animation_event_time_retarget_and_descriptor(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    expect_true(track.motion.generation != 0 && reach_motion_value(&track.motion) == 0,
                "initialized zero is explicit compositor intent rather than an absent property");
    reach_animation_manager_start_at(&manager, 0, 0, 1, 1, REACH_EASING_EASE_OUT, 1000);
    reach_animation_manager_sample_at(&manager, 1000.1);
    const reach_motion previous = *reach_animation_manager_motion(&manager, 0);
    float at_event = reach_motion_value_at(&previous, 1000.5);
    reach_animation_manager_animate_to_at(&manager, 0, 0, 0.5, REACH_EASING_EASE_IN, 1000.5);
    const reach_motion *motion = reach_animation_manager_motion(&manager, 0);
    expect_true(motion->from == at_event && track.value == at_event &&
                    motion->started_seconds == 1000.5 && motion->generation != previous.generation,
                "retarget samples the event instead of the last rendered frame");
    reach_animation_manager_sample_at(&manager, 1000.75);
    expect_true(track.value == reach_motion_value_at(motion, 1000.75),
                "published compositor intent and CPU sampling share one curve");
    reach_animation_manager_sample_at(&manager, 1001);
    expect_true(track.value == 0 && !track.active,
                "reversal reaches its exact endpoint without accumulated deltas");
}

static void test_animation_immediate_reset_and_invalid_sample(void)
{
    reach_animation_track track = {};
    reach_animation_manager manager = {};
    reach_animation_manager_init(&manager, &track, 1);
    reach_animation_manager_start_at(&manager, 0, 3, 3, 1, REACH_EASING_EASE_OUT, 10);
    expect_true(track.value == 3 && !track.active, "equal targets do not request animation frames");
    reach_animation_manager_start_at(&manager, 0, 3, 9, 0, REACH_EASING_EASE_OUT, 10);
    expect_true(track.value == 9 && !track.active, "zero duration sets the endpoint immediately");
    reach_animation_manager_start_at(&manager, 0, 0, 1, 1, REACH_EASING_EASE_OUT, 10);
    reach_animation_manager_sample_at(&manager, NAN);
    expect_true(track.value == 0 && track.active, "invalid sampling cannot poison cached geometry");
    uint64_t generation = track.motion.generation;
    reach_animation_manager_set(&manager, 0, 4);
    expect_true(track.value == 4 && !track.active && track.motion.generation != generation,
                "setting a value cancels old intent with a new generation");
    generation = track.motion.generation;
    reach_animation_manager_reset(&manager, 0);
    expect_true(track.value == 0 && !track.active && track.motion.generation != 0 &&
                    track.motion.generation != generation,
                "reset retires old ownership and publishes an explicit zero endpoint");
}

static void test_repeating_motion_compilation_and_limits(void)
{
    for (reach_easing easing :
         {REACH_EASING_EASE_IN, REACH_EASING_EASE_OUT, REACH_EASING_EASE_IN_OUT})
    {
        for (double hold : {0.0, 0.3, 1.4})
        {
            reach_motion motion = {};
            reach_motion_repeat_at(&motion, -0.4f, 1.6f, 0.7, hold, hold * 0.5, easing, 100);
            motion = reach_motion_clamp(&motion, 0, 1);
            double cycle = reach_motion_cycle_seconds(&motion);
            expect_true(cycle > 0 && reach_motion_deadline(&motion) == 0 &&
                            reach_motion_active_at(&motion, 1000000),
                        "repetition stays active without a finite completion deadline");
            for (double offset : {0.0, 0.15, 0.6, 1.1, 3.0, 20.07})
            {
                double now = 100 + offset;
                reach_motion_segment pieces[REACH_MOTION_MAX_REPEAT_SEGMENTS] = {};
                size_t count = reach_motion_compile_repeat_at(&motion, now, pieces);
                expect_true(count > 0 && pieces[0].start == 0 &&
                                std::fabs(pieces[count - 1].end - cycle) < 1e-9,
                            "rotated compilation covers exactly one complete cycle");
                if (count == 0)
                    continue;
                for (int sample = 0; sample <= 64; ++sample)
                {
                    double time = cycle * sample / 64;
                    for (size_t index = 0; index < count; ++index)
                    {
                        const auto &piece = pieces[index];
                        if (time > piece.end && index + 1 < count)
                            continue;
                        double dt = time - piece.start;
                        double compiled = piece.value + piece.linear * dt +
                                          piece.quadratic * dt * dt + piece.cubic * dt * dt * dt;
                        expect_true(std::fabs(compiled -
                                              reach_motion_value_at(&motion, now + time)) < 0.0002,
                                    "periodic native pieces match canonical CPU sampling");
                        break;
                    }
                }
            }
        }
    }
}

static void test_repeating_motion_affine_crossing_and_retirement(void)
{
    reach_motion motion = {};
    reach_motion_repeat_at(&motion, 0, 1, 1, 1, 2, REACH_EASING_EASE_IN_OUT, 10);
    expect_true(reach_motion_value_at(&motion, 10.5) == 0 &&
                    reach_motion_value_at(&motion, 13) == 1 &&
                    reach_motion_value_at(&motion, 15) == 0,
                "asymmetric holds preserve the complete forward and backward schedule");
    expect_true(std::fabs(reach_motion_time_to_value_at(&motion, 0.5f, 12.5) - 2) < 1e-6,
                "periodic crossing finds the next return leg instead of a past crossing");
    reach_motion mapped = reach_motion_affine(&motion, -3, 5);
    expect_true(mapped.repeating && mapped.started_seconds == motion.started_seconds &&
                    mapped.generation == motion.generation &&
                    std::fabs(reach_motion_value_at(&mapped, 14.5) - 3.5f) < 1e-6,
                "affine repetition preserves epoch, phase and ownership");
    uint64_t generation = motion.generation;
    float value = reach_motion_value_at(&motion, 14.5);
    reach_motion_animate_to_at(&motion, 0, 0.2, REACH_EASING_EASE_OUT, 14.5);
    expect_true(!motion.repeating && motion.from == value && motion.generation != generation,
                "retarget cancels repetition continuously using the same intent contract");
    reach_motion_repeat_at(&motion, 0, 1, 1, -1, 0, REACH_EASING_EASE_OUT, 20);
    expect_true(!motion.repeating && !reach_motion_active_at(&motion, 20),
                "invalid repetition retires to an immediate endpoint");
}

int main(void)
{
    test_frame_rate_policy();
    test_deadlines_do_not_drift();
    test_rate_changes_stalls_and_idle();
    test_blink_deadlines_and_duty_cycle();
    test_blink_delayed_wake_reset_and_stop();
    test_blink_invalid_or_unrepresentable_deadlines();
    test_animation_absolute_sampling_and_final_update();
    test_animation_event_time_retarget_and_descriptor();
    test_animation_immediate_reset_and_invalid_sample();
    test_repeating_motion_compilation_and_limits();
    test_repeating_motion_affine_crossing_and_retirement();
    return failures != 0;
}
