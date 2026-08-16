#include "reach/features/top_bar.h"

#include "top_bar_common.h"

static int32_t reach_top_bar_rect_contains(reach_rect_f32 rect, int32_t x, int32_t y)
{
    return rect.width > 0.0f && rect.height > 0.0f && (float)x >= rect.x &&
           (float)x <= rect.x + rect.width && (float)y >= rect.y &&
           (float)y <= rect.y + rect.height;
}

reach_top_bar_pointer_region reach_top_bar_hit_test(const reach_top_bar_layout *layout,
                                                    int32_t local_x, int32_t local_y)
{
    if (layout == nullptr)
    {
        return REACH_TOP_BAR_POINTER_REGION_NONE;
    }
    if (reach_top_bar_rect_contains(layout->power_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_POWER_BUTTON;
    }
    if (reach_top_bar_tray_icon_at(layout, local_x, local_y) < layout->tray_icon_count)
    {
        return REACH_TOP_BAR_POINTER_REGION_TRAY_ICON;
    }
    if (reach_top_bar_rect_contains(layout->tray_overflow_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_TRAY_OVERFLOW;
    }
    if (reach_top_bar_rect_contains(layout->quick_settings_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_QUICK_SETTINGS_BUTTON;
    }
    if (reach_top_bar_rect_contains(layout->language_button, local_x, local_y))
    {
        return REACH_TOP_BAR_POINTER_REGION_LANGUAGE_BUTTON;
    }
    return REACH_TOP_BAR_POINTER_REGION_NONE;
}

size_t reach_top_bar_tray_icon_at(const reach_top_bar_layout *layout, int32_t local_x,
                                  int32_t local_y)
{
    if (layout == nullptr)
    {
        return REACH_TOP_BAR_MAX_TRAY_ICONS;
    }
    for (size_t index = 0; index < layout->tray_icon_count; ++index)
    {
        if (reach_top_bar_rect_contains(layout->tray_icons[index], local_x, local_y))
        {
            return index;
        }
    }
    return REACH_TOP_BAR_MAX_TRAY_ICONS;
}

static int32_t reach_top_bar_feedback_start(reach_top_bar *top_bar, size_t slot,
                                            float target_opacity)
{
    if (top_bar == nullptr || slot >= REACH_TOP_BAR_FEEDBACK_NONE)
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_index = slot;
    reach_animation_manager_animate_to(reach_top_bar_manager(top_bar),
                                       REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY, target_opacity, 0.055,
                                       REACH_EASING_EASE_IN_OUT);
    return 1;
}

int32_t reach_top_bar_feedback_press(reach_top_bar *top_bar, size_t slot)
{
    if (top_bar == nullptr)
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_pressed = 1;
    return reach_top_bar_feedback_start(top_bar, slot, 0.50f);
}

int32_t reach_top_bar_feedback_release(reach_top_bar *top_bar)
{
    if (top_bar == nullptr ||
        (!reach_top_bar_state_mut(top_bar)->feedback_pressed &&
         reach_top_bar_state_mut(top_bar)->feedback_index == REACH_TOP_BAR_FEEDBACK_NONE))
    {
        return 0;
    }

    reach_top_bar_state_mut(top_bar)->feedback_pressed = 0;
    if (reach_top_bar_state_mut(top_bar)->feedback_index != REACH_TOP_BAR_FEEDBACK_NONE)
    {
        return reach_top_bar_feedback_start(
            top_bar, reach_top_bar_state_mut(top_bar)->feedback_index, 0.0f);
    }
    return 0;
}
