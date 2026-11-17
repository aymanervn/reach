# Interaction

## Pressables

- Every discrete pointer control is a pressable: its click is decided on release over the original pressed target.
- Leaving the target disarms it but keeps its feedback; returning rearms it. No other target becomes eligible during the sequence. Release elsewhere, system cancel or a started drag ends feedback without activation.
- Sliders, scrollbars and drag handles are not pressables. A pressable that can become a drag rearms only until dragging starts.
- Pointer capture is event-driven and held only while the sequence needs it. Never poll the pointer or treat captured movement outside the surface as hover.

## Bar-owned popups

- Top Bar dropdowns and Dock popups share one owner-activation contract keyed by `reach_feature_layout_anchor`. Direction (down or up) never forks toggle, dismissal, replacement or transitions.
- Same owner toggled again closes the popup; a different owner replaces it; a repeated passive hover changes nothing. Payload-backed surfaces such as Context Menu set content only after activation allows presentation.
- A control that can open a popup publishes `reach_feature_control.popup_trigger` on pointer down. Composition keeps that press alive until release; other outside presses dismiss without click-through.

## Edge reveals

- `screen_hotspot` is the invisible-trigger capability; composition exposes it as a descriptor-declared edge reveal (anchor, DP size, layer, visibility, callback). Dynamic features publish bounds through the same runtime. Adding one adds no named composition code.
- Edge reveals stay available during ordinary app activation, subject to game-mode and window-manipulation suppression.

## Bar visibility

- Dock and Top Bar share one visibility policy (`bar_reveal_policy`) with independent state and reveal sessions. A tracked app entering a bar's protected band makes an active auto-hide policy hide that bar at rest.
- Dynamic Top Bar keeps the policy active. Static Top Bar activates it only while the frontmost tracked window on that monitor (ranked by foreground and focus history) is a visible, unminimized, non-game fullscreen window (`reach_window_tracking_front_non_game_fullscreen`). A normal window in front of it shows the bar again. Static config and the work-area reservation do not change.
- Fullscreen means monitor-sized extent plus monitor-center membership, so the bar's own reveal motion cannot revoke it.
- Protected clearance is descriptor data: Top Bar 18dp past its edge; Dock its visible shadow extent (`reach_theme_shadow_visible_extent`). A hidden bar parks beyond the larger of its clearance and full shadow (`reach_bar_visibility_request.hidden_clearance`).
- An intentional edge reveal shows the bar above the app. The Top Bar pushes windows from its animated position (`reach_bar_reveal_ops.position_frame`) without a second animation.
- During an interactive move or resize with the pointer inside Reach's primary monitor: both bars hide, edge reveals are suppressed, bar popups close and the manipulated window is excluded from push. Leaving restores normal policy; the end re-evaluates both bars. Reach, shell and system windows do not trigger this.
- Reach-owned Win+Arrow snapping uses an explicit async start/completion lifecycle. Stage force-show outranks auto-hide. Game mode's immediate cut is a separate policy.
