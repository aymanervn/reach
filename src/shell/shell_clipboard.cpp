#include "shell_internal.h"

void reach_shell_release_clipboard_item(reach_shell *shell, const reach_clipboard_item *item)
{
    if (shell == nullptr || item == nullptr || item->id == 0)
    {
        return;
    }
    if (item->thumbnail_id != 0 && shell->clipboard_surface.renderer.ops.release_icon != nullptr)
    {
        shell->clipboard_surface.renderer.ops.release_icon(
            shell->clipboard_surface.renderer.backend, item->thumbnail_id);
    }
    if (shell->clipboard.ops.release != nullptr)
    {
        shell->clipboard.ops.release(shell->clipboard.provider, item->id);
    }
}

void reach_shell_release_clipboard_items(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }
    for (size_t index = 0; index < shell->clipboard_model.count; ++index)
    {
        reach_shell_release_clipboard_item(shell, &shell->clipboard_model.items[index]);
    }
    reach_clipboard_model_init(&shell->clipboard_model);
}

void reach_shell_clear_clipboard(reach_shell *shell)
{
    if (shell == nullptr)
    {
        return;
    }

    for (size_t index = 0; index < shell->clipboard_model.count; ++index)
    {
        reach_shell_release_clipboard_item(shell, &shell->clipboard_model.items[index]);
    }

    reach_clipboard_model_clear_items(&shell->clipboard_model);
    reach_scrollbar_end_drag(&shell->clipboard_scrollbar_drag);
    shell->clipboard_surface.dirty_flags = 1;
    shell->dirty.layout = 1;
    reach_shell_request_update(shell);
}

void reach_shell_set_clipboard_open(reach_shell *shell, int32_t open)
{
    if (shell == nullptr)
    {
        return;
    }
    int32_t next = open ? 1 : 0;
    if (shell->clipboard_model.open == next)
    {
        return;
    }
    shell->clipboard_model.open = next;
    shell->clipboard_model.hovered_index = REACH_CLIPBOARD_MAX_ITEMS;
    reach_clipboard_model_clear_press(&shell->clipboard_model);
    reach_scrollbar_end_drag(&shell->clipboard_scrollbar_drag);
    for (size_t index = 0; index < REACH_CLIPBOARD_MAX_ITEMS; ++index)
    {
        reach_animation_manager_animate_to(&shell->animations,
                                           reach_shell_clipboard_hover_animation_id(index), 0.0f,
                                           0.10, REACH_EASING_EASE_OUT);
    }
    reach_shell_surface_transition_set(shell, &shell->clipboard_transition, next);
    reach_shell_sync_pointer_move_subscriptions(shell);
    reach_shell_sync_popup_mouse_hook(shell);
    shell->clipboard_surface.dirty_flags = 1;
    shell->dirty.layout = 1;
}

void reach_shell_toggle_clipboard(reach_shell *shell)
{
    if (shell != nullptr)
    {
        reach_shell_set_clipboard_open(shell, !shell->clipboard_model.open);
    }
}

void reach_shell_process_clipboard_refresh(reach_shell *shell)
{
    if (shell == nullptr || shell->clipboard_refresh_requested.exchange(0) == 0 ||
        shell->clipboard.ops.capture_current == nullptr)
    {
        return;
    }
    reach_clipboard_item item = {};
    if (shell->clipboard.ops.capture_current(shell->clipboard.provider, &item) != REACH_OK ||
        item.id == 0)
    {
        return;
    }

    reach_clipboard_item evicted = {};
    if (shell->clipboard_model.count == REACH_CLIPBOARD_MAX_ITEMS)
    {
        evicted = shell->clipboard_model.items[REACH_CLIPBOARD_MAX_ITEMS - 1];
    }
    reach_clipboard_insert_result insertion =
        reach_clipboard_model_insert(&shell->clipboard_model, item);
    if (insertion.rejected_id != 0)
    {
        reach_shell_release_clipboard_item(shell, &item);
    }
    if (insertion.evicted_id != 0)
    {
        reach_shell_release_clipboard_item(shell, &evicted);
    }
    if (insertion.inserted || insertion.promoted_existing)
    {
        reach_scrollbar_set_target(&shell->clipboard_model.scrollbar, 0.0f);
        shell->clipboard_model.scrollbar.offset = 0.0f;
        shell->clipboard_surface.dirty_flags = 1;
        shell->dirty.layout = 1;
        reach_shell_request_update(shell);
    }
}
