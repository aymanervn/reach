# Pending work

Open items only.

## Needs the user's decision

- Reach tracks at most 96 open windows (`REACH_MAX_OPEN_WINDOWS`, `include/reach/core/limits.h`); later windows are likely missing from Stage and the Dock. Raising it also raises the render group budget, which Stage's static assert in `stage_render.cpp` enforces.
- Stage covers only the primary monitor, so windows on other monitors stay in place while it is open. Low priority.


## Bugs

- Switcher DP height test fails on master ("retains its nominal DP height", 3 cases). Either the test or the layout is stale.
- Settings toggle-animation test is timing-sensitive under Idle priority. Drive it from explicit timestamps.
- App Control worker test assumes a queued job has not started.
- Quick Settings Bluetooth rows can stay blank on first open. The cause is in retained presentation (`render_d2d_retained.cpp`, `draw_layer`); it needs a renderer trace.
- Validate in daily use: cross-app activation sometimes left the previous app active (open since 2026-09-07). Likely the console foreground-lock bug fixed in `0131c9a4` and `a765ebd2`; delete this item if it does not return.
- Compositor-driven Stage needs live validation: thumbnail stacking with the focused window on top, Dock restore/minimize closing Stage, minimized windows rising from the bottom centre, about 50 windows, and the classic-thumbnail fallback without ordinal 147.
- Renderer failures other than device loss are not logged (`reach_d2d_device_check`), so a failed frame's log line (`reach_host_recover_failed_frame`) gives only the surface slot and result. The DirectComposition branch of `draw_layer` (`render_d2d_retained.cpp`) also has no surface memory budget.
- Launcher fast scroll can leave the last page's items unloaded. Suspect: the icon prefetch range in `launcher_render.cpp`.
- Clipboard wheel scrolling sometimes stopped. Not confirmed fixed.
- Trails behind a shadow-casting background whose y and height both move. Worked around in Clipboard and Switcher; deferred.
- Shadow padding swallows clicks for other apps' windows (`window_win32.cpp:282` `HTTRANSPARENT`). The Dock and Unified/Split bars still overlap a thin strip and need OS-level passthrough.
- Canceled item drags keep their reordered position.
- A hidden Dock parks its window below the primary monitor; with a monitor stacked below, that window covers a strip at its top and swallows clicks there. The retained reveal makes the strip taller by the slide distance.
- A failed Applications refresh clears its loading state without reporting the failure.
- Graphics-device loss is fatal for Reach surfaces; device-loss and fence recovery is unimplemented.
- Display settings has no overflow policy at short window heights.
- The Launcher window is seven times taller than what it draws.
- Editing an embedded SVG does not rebuild `reach.exe` resources.
- Product-composed opacity motions fail render validation; worked around in the Dock window list.
- A capsule tick's `request_update` does not keep frames running (`host_update.cpp:158`). Audit CPU animations that rely on it.
- `reachService` writes the shared state from two threads without a lock: the hook thread appends hotkeys and touchpad gestures while the main thread publishes windows and game mode through the same non-atomic `reach_shared_writer_begin_publish`/`end_publish` (`reach_service_shared_state_win32.cpp`). A reader can accept a torn snapshot. Serialize writers.
- `reach_frame_wait_tests` segfaults intermittently under `ctest` (14/20 runs on 2026-11-22 at `a0d362f`), never when run directly.
- Retire-queue overflow: Clipboard leaks the payload (`clipboard_feature.cpp:287`); tray never releases the cached icon (`tray_win32.cpp:222`).
- Parallel Windows-adapter builds contend for one PDB.
- Concurrent config INI verification failure (2026-10-12), not yet explained.
- WUC root completion callback is missing in repeated native diagnostics.
- Validate startup and shutdown, bar stacking and focused popup keyboard input on a deployed surface registry build.
- `RoActivateInstance(Compositor)` failed with `E_ACCESSDENIED` in Reach and Settings on every start until Explorer had run once (events.log, up to 2026-11-22 17:14). Not reproduced on a cold boot straight into Reach after the transparency-policy fix (2026-11-22 20:29). Cause unknown; reopen if it returns.
- Validate on a deployed build: three short runs within 60 s fall back to Explorer with the tray notice; apps launched from Reach survive ending Reach; the first start of a newer version shows one UAC prompt only while the watchdog task exists, and never again after a decline.
- Windows Error Reporting dump keys for Reach stay in HKLM after uninstall (`reach_crash_dump_register_local_dumps`); removing them belongs to uninstall in `reachctl`, which needs the owner.
- Windows skips a process's unhandled exception filter once its running exe is renamed (measured 2026-11-25), and WER's dump keys no longer match the renamed image. Between an update's rename-aside and the restart, a crash leaves only a WER event.
- A window that becomes foreground before Reach tracks it is skipped by `reach_host_apply_foreground_change` (`host_window_tracking.cpp:332`; trace 2026-11-25). Unconfirmed whether the record catches up without another foreground change.
- The input-language indicator reads the keyboard layout of the reported thread (`input_language_win32.cpp:15`); for a console that is the client, not the conhost thread that receives keys. Unverified whether the shown layout differs.
- A hung app in front keeps the focus: Alt+Tab hands the foreground to the next app, then dwm's "Not Responding" ghost window takes it back, and a Reach panel opened over it does not get focus either (0/3 each, 2026-11-25). Reach and reachService stay responsive (9/9; window actions on it give up after 750 ms), so it is friction until the app recovers or is ended, not a freeze. Untested: whether a mouse click on another window or the ghost's close button escapes it. Needs an investigation of how the ghost takes the foreground.
- Folder opens duplicate the launcher's ShellExecute and foreground handover (`explorer_service_win32.cpp:36`, `diagnostics_log_win32.cpp:314`); move them into `app_launcher_win32.cpp`.
- Launched apps inherit Reach's start-time environment; environment variable changes reach new apps only after Reach restarts.
- Idle Watch starts during host creation. A full host stop ends its worker until the host is recreated (`host_lifecycle.cpp`, `idle_watch.cpp`).

## Architecture debt (cleanup phase)

- `reach_feature_capsule_ops` is bound positionally with five identical signatures.
- `interfeature_routes.cpp` holds the window-list orchestration, not just slot assignment.
- Move feature-owned notification, control, resource and rendering adapters out of `feature_registry.cpp` during capsule cleanup. Keep the definitions and service wiring in composition.
- Shared contracts are not enforced yet: ordering values are literals in each definition (`feature_registry.cpp:1628-1971`), duplicate event owners and duplicate ordering values are accepted, seam callbacks do not state their calling thread, and no UI-thread assertion exists.
- Numeric surface IDs remain: composition indexes state by `reach_surface_id` (`host->surface_participants[id]` in `host_surface_frames.cpp`) instead of `reach_surface_key_for<T>()` alone.
- Capsule state structs are public and composition reads them. A third of the public feature API is internals. Per-feature action enums remain in public headers.
- Dock and Launcher internals live in `core`.
- Glass policy is split: `theme.c` includes `core/config.h` for the default intensity, and the intensity-to-backdrop-weight curve lives in the adapter (`reach_wuc_create_glass_factory`).
- Only the Context Menu draws its glass divider with `text.quaternary`; other glass surfaces keep `divider`.
- Context Menu uses the Dock's capacity as its "no target" sentinel.
- Point-in-rect is implemented 14 times with three semantics; `reach_rect_equal` is redefined 4 times.
- Dock and Top Bar each cache bar trespass with their own copy of the same logic.
- Clipboard owns a port directly; there is no clipboard service.
- On the UI thread: power commands, audio COM endpoint activations on every pointer move, and the clipboard lock during capture.
- The audio adapter reinvents the `icon_provider` resource-ref path.
- The tray capsule still exists. Move tray data into a tray service owned by the Top Bar.
- The tray menu cancellation contract is unused.
- Nested corners are not migrated in Battery saver rows, Clipboard, Switcher and Settings cards. `launcher_state.cpp:183` repeats the highlight inset.
- Consumers that read `.to` on composed motions need review.
- Clipboard preview keeps its own line splitter until the render port supports wrapping (`render_d2d_text.cpp:187`).

## GPU migration (remaining)

- Migrate resizable cards to one modern nine-grid representation (`CompositionNineGridBrush`) used while resizing and at rest, replacing the clip/painted pair in `reach_ui_motion_background_render` and the stretched tiles (`reach_render_push_stretched_chrome`).
- Dock to GPU: size fit and slot insertion/removal (`dock_feature.cpp`, `size_scale` and `slot_motions`). Reveal and reorder are done.
- Top Bar pill widths to GPU: Now Playing, current app, tray (`top_bar_feature.cpp:505`). The reveal slide stays on the CPU.
- Remaining consumers: Launcher results and resize, Settings expandable rows.
- Separate bar visuals from HWND movement and window push on one physical timeline.
- Live visual and total-workload acceptance for the migrated consumers: Quick Settings, Dock, Switcher, Clipboard, Settings controls, caret, loaders, marquee.
- Remove obsolete APIs, tracks, timers and sampled render state as each consumer completes.

## Not important

- Compiler warnings found in the 2026-10-11 audit.
- A copy-pasted "push a text command" helper in five features; shell-thumbnail and string-copy helpers duplicated across adapters.
- `take` means opposite things on two adjacently named services.
- Quick Settings copies about 20 KB by value per frame for its render input.
- Accent preset follow-ups, until wallpaper accent work starts. The album-art accent keeps its own sRGB lightness clamp and alphas (`media_controls_win32.cpp:256-274`, `top_bar_now_playing.cpp:331`); move it to OKLCH with that work.
- Clipboard's close glyph hover opacity range is a literal (`clipboard_render.cpp:528`).
- Window thumbnails fall back to literal black when no background is set (`window_thumbnail_dwm.cpp:69`).
- Color feedback ignores the ambient parent group (harmless today).
- Screenshot: GDI capture is not HDR-aware, freezes all monitors in memory, and puts only `CF_DIB` on the clipboard.
- Bluetooth "connect" for an already-paired device is not implemented (accepted scope).
