# Pending work

Open items only.

## Needs the user's decision


## Bugs

- Switcher DP height test fails on master ("retains its nominal DP height", 3 cases). Either the test or the layout is stale.
- Settings toggle-animation test is timing-sensitive under Idle priority. Drive it from explicit timestamps.
- App Control worker test assumes a queued job has not started.
- Quick Settings Bluetooth rows can stay blank on first open. The cause is in retained presentation (`render_d2d_retained.cpp`, `draw_layer`); it needs a renderer trace.
- Cross-app activation sometimes leaves the previous app active; a direct click restores focus. Unresolved since 2026-09-07.
- Stage minimized placeholders expand toward restore bounds on close (reopened 2026-09-19).
- Stage held input about 500ms after its motion looked finished. The fix is waiting for live validation.
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
- Self-update after a fixed wait: the root cause of the restart report is unconfirmed (mitigated 2026-11-17).
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
- Idle Watch starts during host creation. A full host stop ends its worker until the host is recreated (`host_lifecycle.cpp`, `idle_watch.cpp`).

## Architecture debt (cleanup phase)

- `reach_feature_capsule_ops` is bound positionally with five identical signatures.
- `interfeature_routes.cpp` holds the window-list orchestration, not just slot assignment.
- Move feature-owned notification, control, resource and rendering adapters out of `feature_registry.cpp` during capsule cleanup. Keep the definitions and service wiring in composition.
- Shared contracts are not enforced yet: ordering values are literals in each definition (`feature_registry.cpp:1546-1835`), duplicate event owners and duplicate ordering values are accepted, seam callbacks do not state their calling thread, and no UI-thread assertion exists.
- Numeric surface IDs remain: composition indexes state by `reach_surface_id` (`host->surface_participants[id]` in `host_surface_frames.cpp`) instead of `reach_surface_key_for<T>()` alone.
- Capsule state structs are public and composition reads them. A third of the public feature API is internals. Per-feature action enums remain in public headers.
- Dock and Launcher internals live in `core`.
- `reach_ui_layout` and its only consumer are dead.
- Context Menu uses the Dock's capacity as its "no target" sentinel.
- Point-in-rect is implemented 14 times with three semantics; `reach_rect_equal` is redefined 4 times.
- Dock and Top Bar each cache bar trespass with their own copy of the same logic.
- Clipboard owns a port directly; there is no clipboard service.
- On the UI thread: power commands, audio COM endpoint activations on every pointer move, and the clipboard lock during capture.
- The audio adapter reinvents the `icon_provider` resource-ref path.
- The tray capsule still exists. Move tray data into a tray service owned by the Top Bar.
- The tray menu cancellation contract is unused.
- Nested corners are not migrated in Battery saver rows, Clipboard, Switcher and Settings cards. `launcher_state.cpp:183` repeats the highlight inset.
- Every scroll surface owns an identical pair of scrollbar theme tokens.
- Consumers that read `.to` on composed motions need review.
- Clipboard preview keeps its own line splitter until the render port supports wrapping (`render_d2d_text.cpp:187`).

## GPU migration (remaining)

- Migrate resizable cards to one modern nine-grid representation (`CompositionNineGridBrush`) used while resizing and at rest, replacing the clip/painted pair in `reach_ui_motion_background_render` and the stretched tiles (`reach_render_push_stretched_chrome`).
- Repeatable baseline measurements, and final comparable performance acceptance. Windows profiling policy blocks system trace collection.
- Dock to GPU: size fit and slot insertion/removal (`dock_feature.cpp`, `size_scale` and `slot_motions`). Reveal and reorder are done.
- Top Bar pill widths to GPU: Now Playing, current app, tray (`top_bar_feature.cpp:505`). The reveal slide stays on the CPU.
- Remaining consumers: Launcher results and resize, Settings expandable rows, Stage movement, backdrop and feedback.
- Separate bar visuals from HWND movement and window push on one physical timeline.
- Live visual and total-workload acceptance for the migrated consumers: Quick Settings, Dock, Switcher, Clipboard, Settings controls, caret, loaders, marquee.
- Remove obsolete APIs, tracks, timers and sampled render state as each consumer completes.

## Not important

- Compiler warnings found in the 2026-10-11 audit.
- A copy-pasted "push a text command" helper in five features; shell-thumbnail and string-copy helpers duplicated across adapters.
- `take` means opposite things on two adjacently named services.
- Quick Settings copies about 20 KB by value per frame for its render input.
- Accent preset follow-ups, until wallpaper accent work starts.
- Color feedback ignores the ambient parent group (harmless today).
- Screenshot: GDI capture is not HDR-aware, freezes all monitors in memory, and puts only `CF_DIB` on the clipboard.
- Bluetooth "connect" for an already-paired device is not implemented (accepted scope).
