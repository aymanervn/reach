#include "shell_internal.h"

static void reach_shell_start_dock_click_feedback(reach_shell *shell, size_t index,
                                                  float target_opacity)
{
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON)
    {
        return;
    }

    shell->feedback.dock_index = index;

    reach_float_animation_start(&shell->feedback.dock_opacity, shell->feedback.dock_opacity.value,
                                target_opacity, 0.055);

    shell->feedback.dock_animating = 1;
    shell->dock.dirty_flags = 1;
}

void reach_shell_press_dock_item(reach_shell *shell, size_t index)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->feedback.dock_pressed = 1;
    shell->feedback.dock_sticky = 0;

    reach_shell_start_dock_click_feedback(shell, index, 0.50f);
}

void reach_shell_set_dock_click_feedback_immediate(reach_shell *shell, size_t index, float opacity)
{
    if (shell == nullptr || index > REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON)
    {
        return;
    }

    shell->feedback.dock_index = index;
    shell->feedback.dock_opacity = {};
    shell->feedback.dock_opacity.from = opacity;
    shell->feedback.dock_opacity.to = opacity;
    shell->feedback.dock_opacity.value = opacity;
    shell->feedback.dock_animating = 0;
    shell->dock.dirty_flags = 1;
}

void reach_shell_stick_dock_item(reach_shell *shell)
{
    if (shell == nullptr || (!shell->feedback.dock_pressed &&
                             shell->feedback.dock_index == REACH_SHELL_DOCK_FEEDBACK_NONE))
    {
        return;
    }

    shell->feedback.dock_pressed = 0;
    shell->feedback.dock_sticky = shell->feedback.dock_index != REACH_SHELL_DOCK_FEEDBACK_NONE;

    if (shell->feedback.dock_sticky)
    {
        reach_shell_set_dock_click_feedback_immediate(shell, shell->feedback.dock_index, 0.50f);
    }
}

void reach_shell_release_dock_item(reach_shell *shell)
{
    if (shell == nullptr || (!shell->feedback.dock_pressed &&
                             shell->feedback.dock_index == REACH_SHELL_DOCK_FEEDBACK_NONE))
    {
        return;
    }

    shell->feedback.dock_pressed = 0;
    shell->feedback.dock_sticky = 0;

    if (shell->feedback.dock_index != REACH_SHELL_DOCK_FEEDBACK_NONE)
    {
        reach_shell_start_dock_click_feedback(shell, shell->feedback.dock_index, 0.0f);
    }
}

void reach_shell_clear_sticky_dock_feedback(reach_shell *shell)
{
    if (shell != nullptr && shell->feedback.dock_sticky)
    {
        reach_shell_release_dock_item(shell);
    }
}

void reach_shell_press_tray_button(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->feedback.dock_pressed = 1;
    shell->feedback.dock_sticky = 0;

    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_TRAY_BUTTON, 0.50f);
}

void reach_shell_press_quick_settings_button(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->feedback.dock_pressed = 1;
    shell->feedback.dock_sticky = 0;

    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_QUICK_SETTINGS_BUTTON,
                                          0.50f);
}

void reach_shell_press_power_button(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->feedback.dock_pressed = 1;
    shell->feedback.dock_sticky = 0;

    reach_shell_start_dock_click_feedback(shell, REACH_SHELL_DOCK_FEEDBACK_POWER_BUTTON, 0.50f);
}

static void reach_shell_start_tray_click_feedback(reach_shell *shell, size_t index,
                                                  float target_opacity)
{
    if (shell == nullptr || index >= REACH_MAX_TRAY_ITEMS)
    {
        return;
    }

    shell->feedback.tray_index = index;

    reach_float_animation_start(&shell->feedback.tray_opacity, shell->feedback.tray_opacity.value,
                                target_opacity, 0.055);

    shell->feedback.tray_animating = 1;
    shell->tray.dirty_flags = 1;
}

void reach_shell_press_tray_item(reach_shell *shell, size_t index)
{
    if (shell == nullptr)
    {
        return;
    }

    shell->feedback.tray_pressed = 1;
    reach_shell_start_tray_click_feedback(shell, index, 0.50f);
}

void reach_shell_release_tray_item(reach_shell *shell)
{
    if (shell == nullptr ||
        (!shell->feedback.tray_pressed && shell->feedback.tray_index == REACH_MAX_TRAY_ITEMS))
    {
        return;
    }

    shell->feedback.tray_pressed = 0;

    if (shell->feedback.tray_index != REACH_MAX_TRAY_ITEMS)
    {
        reach_shell_start_tray_click_feedback(shell, shell->feedback.tray_index, 0.0f);
    }
}
