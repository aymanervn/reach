#include "reach/features/common/selection_pill.h"

#include "reach/support/util.h"

#include <math.h>

reach_selection_pill_mode reach_selection_pill_mode_for_distance(size_t from_slot, size_t to_slot)
{
    size_t distance = to_slot > from_slot ? to_slot - from_slot : from_slot - to_slot;
    return distance == 1 ? REACH_SELECTION_PILL_STEP : REACH_SELECTION_PILL_JUMP;
}

void reach_selection_pill_retarget(reach_selection_pill *pill, const reach_theme *theme,
                                   reach_rect_f32 target, size_t slot,
                                   reach_selection_pill_mode mode, double now_seconds)
{
    REACH_ASSERT(pill != nullptr);
    REACH_ASSERT(mode == REACH_SELECTION_PILL_STEP || mode == REACH_SELECTION_PILL_JUMP ||
                 mode == REACH_SELECTION_PILL_SET);
    REACH_ASSERT(isfinite(target.x) && isfinite(target.y) && isfinite(target.width) &&
                 isfinite(target.height));
    if (pill == nullptr)
        return;
    reach_motion *channels[] = {&pill->rect.x, &pill->rect.y, &pill->rect.width,
                                &pill->rect.height};
    const float targets[] = {target.x, target.y, target.width, target.height};
    reach_motion_spec spring = reach_theme_motion_preset(theme, mode == REACH_SELECTION_PILL_STEP
                                                                    ? REACH_MOTION_PRESET_BOUNCY
                                                                    : REACH_MOTION_PRESET_SMOOTH);
    pill->slot = slot;
    for (size_t index = 0; index < 4; ++index)
    {
        if (mode == REACH_SELECTION_PILL_SET)
            reach_motion_set(channels[index], targets[index]);
        else if (channels[index]->to != targets[index])
            reach_motion_animate_spec_to_at(channels[index], targets[index], spring, now_seconds);
    }
}
