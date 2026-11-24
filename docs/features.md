# Features

## Applications and Dock

- `reach_application` is the canonical app record: optional AppUserModelID plus normalized runtime executable aliases. Launch target, shortcut, icon and title are not identity.
- Two identities match when their executable aliases intersect or their AppUserModelIDs match; transitive matches close into one deterministic group (`reach_application_identity_matches`, `reach_window_tracking_refresh`).
- Running-window pins resolve through `reach_application_resolver_port`. Packaged apps keep packaged activation; an unpackaged helper uses its parent executable only when the parent is in the same install tree. Otherwise use the observed executable; never guess from names or publishers.
- `reachctl` imports the Windows taskbar once, only when it creates a missing `reach.ini` (`reachctl_initialize_config`). Runtime never invents pins or overwrites config; an empty pin list is valid.
- Capacity: 96 pins plus 96 running groups, 192 items total; a running pinned app is one item. Constants live in `include/reach/core/limits.h`, enforced by the pin service and Dock model.
- The Dock's native geometry is its maximum. `reach_dock_build_layout` scales everything down uniformly only as far as needed to fit the monitor; there is no minimum.
- Monitor bounds and DPI arrive through generic layout input from `reach_monitor_port`; display, DPI and item changes trigger relayout. The Dock never reads the port.

## Activation and Stage

- Reach replaces the desktop with a simpler model; keep Explorer behavior only where it serves that model.
- Dock, Switcher and Stage share app activation. Dock keeps its foreground-minimize toggle using the actual foreground, including owned dialogs; remembered focus serves Switcher and returning from Reach surfaces.
- The latest pending app selection wins. Close, minimize and snap requests are not selections.
- Preserve verified native activation workarounds until evidence supports removing them.
- Surfaces take pointer input while presented (`reach_host_surface_presented`); capture and outside-press dismissal apply only while open.
- Stage open/close is one reversible transition until the close commits its transparent frame (`reach_stage_set_open`, `reach_host_cancel_close_handoff`).
- Stage has one close choreography for every trigger (`reach_stage_begin_close`). An app tile queues ordinary activation and then requests the same close; Stage stores no closing selection. Movement starts immediately; external window readiness never gates it. Activation completion may only hold the aligned opaque frame. Never write foreground tracking optimistically.
- On an app-click handoff, the activated window is published as a presentation-only front source and stays above the other relationships through teardown, with its tile chrome drawn last. This must not fork close geometry, timing or teardown.
- Stage thumbnails are best-effort during presentation: retry missing relationships without aborting the frame. Close boundaries stay strict.
- Stage places each app tile near its window's real position (its centre as a fraction of its own monitor; all monitors share one space) in loosely aligned, non-overlapping rows, with no per-monitor sections. Tile size compresses real size (displayed area grows with the square root of real area), never exceeds real size, and never drops below a 144 dp longer side unless the window is smaller or the whole set cannot otherwise fit. Ties follow focus order; the layout changes only when the window set, displays or Stage area change (`reach_stage_rebuild_layout`).

## Shell lifecycle

- Windows starts and restarts `reach.exe`: it reruns the logon-time shell command whenever the first process to register a shell window exits, whatever the exit code. Reach never starts Reach, so Reach and every app it launches stay outside any job.
- Reach claims the shell window before anything that can fail and hands it to the desktop host later (`reach_windows_shell_session_claim`, `reach_windows_shell_session_register`).
- Three starts within 60 s mean the installed version is broken: Reach falls back to Explorer permanently (shell value, direct start, tray notice) and exits without claiming, so Windows stops restarting it (`reach_crash_loop_record_start`, `reach_windows_shell_session_fall_back`).
- Switching to Reach from another shell needs a sign-out. `reachctl --start` restarts a running Reach, or starts one when no shell runs at all.
- An update only swaps files (running ones are renamed aside), closes Settings and restarts Reach through `reachctl --start`.
- The first start of a newer version stamps it in `reach.ini`, then cleans up once in the background: the watchdog exe is renamed aside and the watchdog ended unless Reach lives in its job, and, only while the task exists, a notice explains the coming prompt before one `reachElevate` prompt deletes the task. A declined prompt is never repeated. This cleanup is the only watchdog removal and stays in every release by the owner's decision; `reachctl` no longer knows the watchdog (`reach_app_run_install_cleanup_once`, `reach_windows_start_install_cleanup`).

## Touchpad gestures

- Three-finger swipes on a precision touchpad are read and recognized in the elevated `reachService` helper, which keeps receiving every contact while an elevated window has focus. Recognized swipes reach `reach.exe` as gesture records in the shared hotkey queue. Swipes need the helper, like Alt+Tab and Win+D.
- Exactly three confident contacts start a gesture; any further contact, including one the touchpad marks as a palm, abandons it. Locking the session or lost touchpad reports cancel a swipe-opened switcher.
- Left and right drive the Switcher through the Alt+Tab events, and the highlight follows the fingers: a right swipe opens on the previous app, a left swipe on the oldest (like Alt+Shift+Tab). The step per app shrinks with finger speed (`REACH_TOUCHPAD_SWIPE_*` in `src/tools/reach_service/touchpad_swipe.h`). Reversing walks back; lifting a finger commits.
- Whichever input opens the Switcher owns it until commit or cancel; the other input is ignored meanwhile, and Escape cancels either (`reach_input_switch_owner` in `input_win32.cpp`).
- Up opens Stage. Down closes Stage when it is open, otherwise minimizes all windows like Win+D.

## Game mode

- Game mode suppresses Reach actions for the Windows key and its chords without consuming them (`src/tools/reach_service/hotkeys.cpp`).
- Three-finger left and right stay active; up and down are ignored.
- Alt-Tab and media/volume keys stay active. Switcher stays visible and never minimizes the game; committing another target exits the game through ordinary activation. Now Playing stays active with async metadata and covers.
- `system_stats` sampling stops during game mode and rebaselines after.
- Game mode never changes a monitor work area; a static Top Bar reservation stays applied (`reach_host_update_game_mode_surfaces`, `reach_host_sync_work_area_reservation`).

## System HUD

- Every successful media, volume, mute or brightness action shows the System HUD regardless of Top Bar visibility or game mode. It is pointer-transparent and game-mode visible. Each action resets a 1.5 s total lifetime that includes the closing fade (`reach_feature_notify_system_hud`).

## Tray

- Tray UI and interaction belong to the Top Bar. The tray capsule is legacy residue; its data moves to a tray service consumed by the Top Bar.
