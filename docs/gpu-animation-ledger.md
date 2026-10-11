# GPU composition migration: animation ledger

Audit date: 2026-10-11. Baseline revision: `a74a3c1`, plus the migration planning documents.
Inventory and initial dispositions are complete; migrations and performance acceptance are pending.
The [checklist](gpu-composition-migration-checklist.md) tracks completion, and the
[plan](gpu-workload-migration-plan.md) defines acceptance.

## Reading the ledger

An array row describes all instances of one effect, including dynamic source membership. Distinct
effects sharing a driver remain separate rows. Sources name declarations and execution/consumer
symbols so the record survives line-number changes.

Drivers and present work:

- **Scalar:** `reach_animation_manager_tick` advances delta-based values. Consumers bake values
  into commands/layout and request application frames while active. Retained caching may avoid
  some actual rasterization; a command/layout frame is not automatically a raster draw.
- **Retained:** `reach_motion` or compositor repetition drives visual properties. Application work
  submits changes and handles events/deadlines/completion; unsupported rendering uses the existing
  finite fallback, with marquee static. Existing paths still need measured acceptance.
- **Phase:** CPU phase advancement produces changing content; these paths bypass the manager.
- **Scroll:** `reach_scrollbar_update` interpolates offset from frame deltas, causing layout/content
  updates until target convergence. Direct pointer positions and stepped scrolling are input work.
- **Native:** canonical presentation or scalar samples drive immediate native placement/window moves.
- **Deadline:** CPU lifecycle/content updates at an actual deadline, rather than every animation frame.

Dispositions are **migrate**, **verify existing**, **capability decision**, or **CPU deadline**.
The latter preserves semantic timing/event work, not an exemption for per-frame rendering. Capability
decisions must resolve to an implemented compositor path or an evidenced, optimized hybrid path.
No current CPU visual path has final measured acceptance yet. Retained resource and source lifetime
are evaluated independently from the animation's active lifetime.

## Reach scalar consumers

| Owner/effect | Source symbol and consumer | Driver / changing work | Active lifetime | Initial disposition and reason |
| --- | --- | --- | --- | --- |
| Dock reveal Y | `REACH_DOCK_ANIM_Y`; `reach_dock_bar_update_visibility`, shared bar visibility | Scalar/native window geometry | Reveal/hide to target | Capability decision: physical HWND and pushed-window geometry remain necessary; evaluate drawn motion separately. |
| Dock press feedback | `REACH_DOCK_ANIM_FEEDBACK_OPACITY`; `reach_dock_append_render_commands` | Scalar / overlay alpha | Press, disarm, release fade | Migrate: fixed overlay content can use retained opacity. |
| Dock insertion/removal slots | `REACH_DOCK_ANIM_SLOT_BASE`; `reach_dock_slot_reveal` | Scalar / packed widths, presence and capacity layout | Slot target changes, including departing sources | Migrate: retain item content and animate eligible geometry; reconcile capacity constraints explicitly. |
| Top Bar reveal Y | `REACH_TOP_BAR_ANIM_Y`; `reach_top_bar_build_layout` | Scalar/native window push | Reveal/hide to target | Capability decision: preserve common physical displacement; separate visual work. |
| Top Bar power hover | `REACH_TOP_BAR_ANIM_POWER_HOVER`; `reach_top_bar_append_render_commands` | Scalar / hover chrome | Enter/leave fade | Migrate: reusable fixed feedback. |
| Top Bar press feedback | `REACH_TOP_BAR_ANIM_FEEDBACK_OPACITY`; shared pressable | Scalar / overlay alpha | Press/release fade | Migrate: shared retained feedback. |
| Now Playing width | `REACH_TOP_BAR_ANIM_NOW_PLAYING_WIDTH`; `reach_top_bar_build_layout` | Scalar / content and bar geometry | Visibility/content target change | Migrate: shared retained resize and positioning without stretching content. |
| Current app width | `REACH_TOP_BAR_ANIM_CURRENT_APP_WIDTH`; `reach_top_bar_build_layout` | Scalar / text-sized geometry | Identity/title target change | Migrate: target metrics and retained positioning. |
| Tray width | `REACH_TOP_BAR_ANIM_TRAY_WIDTH`; `reach_top_bar_build_layout` | Scalar / tray/bar geometry | Membership target change | Migrate: retain stable icon identities. |
| Quick Settings control width | `REACH_TOP_BAR_ANIM_QUICK_SETTINGS_WIDTH`; `reach_top_bar_build_layout` | Scalar / control/bar geometry | System-state target change | Migrate: shared width capability. |
| Tray popup feedback | `reach_top_bar_tray_popup::press_feedback`; `reach_top_bar_append_tray_render_commands` | Retained motion / icon tint or fallback rectangle | Press/release fade | Implemented: shared absolute-time pressable intent and retained opacity; scalar track/manager accessor and composition frame blocker removed. Both compositors and fallback pass bounded capsule diagnostics. Final visual/performance acceptance remains open. |
| Battery press feedback | `reach_battery::press_feedback`; `reach_battery_append_render_commands` | Retained motion / fixed saver-row overlay | Press/release fade | Implemented: shared pressable absolute-time intent and retained opacity; no capsule feedback ticks/frame demand. HWND fallback samples published motion. Native operation-count verification passed; final visual/performance acceptance remains open. |
| Launcher results expansion | `REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION`; `reach_launcher_capsule_tick` | Scalar / results height and reveal | Results enter/leave | Migrate: endpoint layout, animated clip and eligible chrome. |
| Context Menu item/header hover | `REACH_CONTEXT_MENU_ANIM_HOVER`; menu and window-preview rendering | Scalar / row/pill/preview border | Hover enter/leave, identity switch | Migrate: theme-preserving retained feedback. |
| Context Menu close hover | `REACH_CONTEXT_MENU_ANIM_CLOSE_HOVER`; preview close rendering | Scalar / close feedback | Close target enter/leave | Migrate: retain fixed close content. |
| Dock popup width | `REACH_CONTEXT_MENU_ANIM_WIDTH`; `window_preview_layout.cpp` | Scalar / popup packing and chrome | Instance membership/size changes | Capability decision: resize chrome and native destinations must align. |
| Dock popup reflow | `REACH_CONTEXT_MENU_ANIM_REFLOW`; `window_preview_animation.cpp` | Scalar / identity-based screen positions | Membership changes and interruption | Capability decision: native previews, headers and input must share motion. |
| Switcher width | `REACH_SWITCHER_ANIMATION_WIDTH`; `reach_switcher_arrange` | Scalar / page fitting and chrome | Page/source target changes | Capability decision: shared preview/resize contracts. |
| Clipboard item hover array | `REACH_CLIPBOARD_ANIMATION_HOVER_BASE`; Clipboard renderer | Scalar / row backgrounds | Hover enter/leave | Migrate: stable item identities and shared feedback. |
| Clipboard height | `REACH_CLIPBOARD_ANIMATION_HEIGHT`; `reach_clipboard_feature_animate_layout_value` | Scalar / surface and viewport | Content/constraint changes | Migrate: reusable retained resize. |
| Clipboard item width | `REACH_CLIPBOARD_ANIMATION_ITEM_WIDTH`; Clipboard animated layout | Scalar / item geometry | Content/constraint changes | Migrate: target layout and retained movement/reveal. |
| Quick Settings height | `REACH_QUICK_SETTINGS_ANIMATION_HEIGHT`; `reach_quick_settings_tick` | Scalar / popup chrome | Section/content target changes | Migrate: shared resize. |
| Output-device expansion | `REACH_QUICK_SETTINGS_ANIMATION_OUTPUT_DEVICES_EXPANSION`; Quick Settings layout/render | Scalar / section height, reveal and following content | Expand/collapse | Migrate: shared retained section reveal. |
| App-volume expansion | `REACH_QUICK_SETTINGS_ANIMATION_APP_VOLUMES_EXPANSION`; Quick Settings layout/render | Scalar / section height, reveal and following content | Expand/collapse | Migrate: same section capability. |
| Quick Settings press feedback | `reach_quick_settings::press_feedback`; shared pressable and `reach_ui_feedback_render` | Retained motion / fixed target overlay | Press/release fade | Implemented: direct absolute-time intent, no feedback scalar advancement/frame demand; native compositor and HWND fallback verification pass. Final visual/performance acceptance remains open. |
| Stage tile progress | `REACH_STAGE_ANIMATION_PROGRESS`; `stage_layout.cpp` | Scalar/native / geometry and close landing | Enter/close | Capability decision: preserve synchronized native close. |
| Stage desktop progress | `REACH_STAGE_ANIMATION_DESKTOP_PROGRESS`; `stage_layout.cpp` | Scalar/native / desktop thumbnail geometry | Enter/close | Capability decision: desktop source and ordering semantics. |
| Stage reflow | `REACH_STAGE_ANIMATION_REFLOW`; `stage_layout.cpp` | Scalar/native / source rearrangement | Membership changes | Capability decision: stable sources and shared motion. |
| Stage departure | `REACH_STAGE_ANIMATION_DEPARTURE`; `stage_layout.cpp` | Scalar/native / departing content | Removed source exit | Capability decision: retiring source lifetime and geometry. |
| Stage close hover | `REACH_STAGE_ANIMATION_CLOSE_HOVER`; Stage render | Scalar / close feedback | Hover enter/leave | Migrate: shared retained feedback independently of native geometry. |
| Stage backdrop | `REACH_STAGE_ANIMATION_BACKDROP`; Stage render | Scalar / backdrop alpha | Presentation/close | Migrate: retained opacity independently of preview decision. |
| Stage retarget | `REACH_STAGE_ANIMATION_RETARGET`; `stage_layout.cpp` | Scalar/native / interrupted landing correction | Close retarget | Capability decision: preserve verified landing choreography. |

Declarations: `include/reach/features/dock.h`, `include/reach/features/top_bar.h`, and private
`*_common.h`/feature enums in Battery, Context Menu, Stage, Launcher, Switcher, Clipboard and Quick
Settings. Shared scalar feedback routing is in `src/features/common/pressable/pressable.cpp`;
bar routing is in `src/features/common/bar_visibility/bar_visibility.cpp`.

## Settings scalar consumers

All declarations are in `include/reach/apps/settings/settings.h`; initialization is in
`settings_feature.cpp`. `reach_settings_app_update` ticks these drivers, sets dirty, refreshes
bounds/layout and renders. `reach_settings_app_needs_frame` includes active tracks and some
independent input/worker states. Each row must remove its old sampled commands/frame request when
migrated, rather than only changing the track representation.

| Effect / declared track storage | Execution and visual consumer | Active lifetime | Initial disposition and reason |
| --- | --- | --- | --- |
| `wifi_row_tracks[]`, including add-network row | `settings_wifi.cpp`, `settings_wifi_layout.cpp`, `settings_wifi_render.cpp` | Expand/collapse | Migrate: retained section reveal and following-row movement. |
| `wifi_radio_track` | `settings_wifi.cpp`, Wi-Fi toggle render | Radio target changes | Migrate: retained knob position and eligible color/opacity feedback. |
| `bluetooth_row_tracks[]` | `settings_bluetooth.cpp`, Bluetooth layout/render | Expand/collapse | Migrate: same section capability. |
| `bluetooth_radio_track` | `settings_bluetooth.cpp`, Bluetooth toggle render | Radio target changes | Migrate: shared retained toggle. |
| `startup_tracks[]` | `settings_startup_apps.cpp`, `settings_render.cpp` | Enable/disable target changes | Migrate: shared toggle across all entries. |
| `power_tracks[]` | `settings_power_sleep.cpp`, `settings_render.cpp` | Custom timer entry/reconfiguration | Migrate: preserve picker/edit reveal with retained content. |
| `power_wait_tracks[]` | `settings_power_sleep.cpp`, `settings_render.cpp` | Wait option target changes | Migrate: shared retained toggle. |
| `display_fps_track` | `settings_monitors_scaling.cpp`, `settings_render.cpp` | Refresh preference changes | Migrate: shared retained toggle. |
| `display_font_track` | `settings_monitors_scaling.cpp`, `settings_render.cpp` | Font preference changes | Migrate: retain feedback and invalidate actual font content. |
| `display_theme_track` | `settings_monitors_scaling.cpp`, `settings_render.cpp` | Theme selection | Migrate: retained selection without changing theme policy. |
| `top_bar_style_track` | `settings_top_bar.cpp`, `settings_render.cpp` | Style selection | Migrate: retained segmented selection. |
| `top_bar_mode_track` | `settings_top_bar.cpp`, `settings_render.cpp` | Mode selection | Migrate: retained segmented selection. |
| `button_press_track` | `settings_feature.cpp`, app button routing and renderers | Press/disarm/release fade | Migrate: shared retained feedback with original-target identity. |

## Already retained presentation and movement

| Effect / owner | Source and driver | Present work / lifetime | Initial disposition |
| --- | --- | --- | --- |
| Dock neighbour movement `item_x[]` | `dock_feature.cpp`, `dock_render.cpp`; Retained | Input/membership publishes motion; event-time hit samples; finite move | Verify existing, including interaction with slot fitting. |
| Dock released item `drag.snap` | Dock drag and render; Retained | Release submits finite snap; stable identity | Verify existing; stopped drag must cause no periodic frames. |
| Tray neighbour movement `tray_x[]` | `top_bar_interaction.cpp`, `top_bar_render.cpp`; Retained | Drag/reorder submits finite movement | Verify existing, including width changes. |
| Tray released icon `tray_drag.snap` | Same tray interaction/render; Retained | Release submits finite snap | Verify existing. |
| Now Playing title marquee | `top_bar_now_playing.cpp`, `reach_render_group.repeat_offset_x`; Retained repetition | Content change submits bounded clipped title; hidden group retires | Verify existing repetition, setup, power and resource cost. |
| Settings nav selector `nav_selection` | `settings_feature.cpp`, `settings_render.cpp`; Retained | Selection submits finite Y motion | Verify existing and finite fallback. |
| Launcher `surface_y` | `launcher_state.cpp`; Retained | Open/close submission, events and completion | Verify existing. |
| Launcher `surface_opacity` | Same; Retained | Open/close, final visibility reconciliation | Verify existing. |
| Launcher `surface_scale` | Same; Retained | Open/close with proportion-preserving scale | Verify existing. |
| HUD `opacity` | `system_hud_feature.cpp`; Retained | Action submits/restarts open/close; absolute total lifetime | Verify existing; hold must not demand animation frames. |
| Battery popup Y and opacity | `reach_feature_transition` in Battery; Retained | Open/close, absolute deadline and completion | Verify existing. |
| Context Menu popup Y and opacity | Same shared transition in Context Menu; Retained/native | Open/close; previews require CPU native alignment | Capability decision for native pixels; verify retained chrome. |
| Switcher popup Y and opacity | Shared transition in Switcher; Retained/native | Open/close; native geometry samples during presentation | Capability decision for native pixels; verify retained chrome. |
| Clipboard surface Y and opacity | Shared transition in Clipboard; Retained | Open/close and final reconciliation | Verify existing. |
| Quick Settings popup Y and opacity | Shared transition in Quick Settings; Retained | Open/close and final reconciliation | Verify existing. |
| Tray popup Y and opacity | Shared transition in tray popup; Retained | Open/close and final reconciliation | Verify existing. |

Shared descriptors: `include/reach/features/common/presentation.h`,
`src/features/common/presentation/presentation_animation.cpp`. Generic execution/CPU input sampling:
`host_update.cpp`, `host_surface_frames.cpp`, `host_input.cpp`; native animation and fallback:
`render_d2d_retained.cpp`, `render_d2d_commands.cpp`. Static retained group geometry in Top Bar,
artwork and controls is caching, not an additional temporal effect.

## Timing and movement outside scalar tracks

| Owner/effect | Source / driver | Present work and active lifetime | Initial disposition and reason |
| --- | --- | --- | --- |
| Wi-Fi loader `wifi_loader` | `settings_wifi.cpp`, `reach_loader_update`; Phase | Scan while active; phase changes loading bar geometry | Migrate: bounded compositor grow/shrink repetition with live semantic status. |
| Bluetooth loader `bluetooth_loader` | `settings_bluetooth.cpp`; Phase | Scan while active; changing bar geometry | Migrate: shared loader. |
| Update loader `update_loader` | `settings_update.cpp`; Phase | Update work stages; changing bar geometry | Migrate: shared loader; progress/status remains CPU event data. |
| Installed apps loader `installed_apps_loader` | `settings_installed_apps.cpp`; Phase | Loading/refreshing; changing bar geometry | Migrate: shared loader. |
| Wi-Fi `wifi_caret_phase` | `reach_settings_model_tick_wifi_caret`; Phase | Focused text; half-period toggles | CPU deadline candidate: discrete blink needs only actual toggle wakes; compare retained opacity cost. |
| Account `account_caret_phase` | `reach_settings_model_tick_account_caret`; Phase | Focused text; 0.55/0.45 duty cycle | CPU deadline candidate: preserve duty cycle and input reset; compare compositor alternative. |
| Power `power_caret_phase` | `reach_settings_model_tick_power_caret`; Phase | Focused timer edit; 0.55/0.45 duty cycle | Same deadline candidate. |
| Launcher `caret_deadline` | `reach_launcher_tick_caret`; Deadline | Actual blink only while open; input reset | Verify existing efficient deadline and isolated caret invalidation. |
| Clipboard smooth scrolling | `reach_clipboard_tick_scroll`, `reach_scrollbar_update`; Scroll | Offset approaches target; row geometry and scrollbar change | Migrate eligible viewport translation; preserve hit testing and bounded content. |
| Settings Wi-Fi scrolling | `reach_settings_model_wifi_scroll`; Scroll | Offset approaches target | Migrate eligible retained viewport translation, with constraint/model reconciliation. |
| Settings Bluetooth scrolling | `reach_settings_model_bluetooth_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings update scrolling | `reach_settings_model_update_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings startup scrolling | `reach_settings_model_startup_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings installed-app scrolling | `reach_settings_model_installed_apps_scroll`; Scroll | Offset approaches target | Same shared scrolling capability; retain bounded virtualization. |
| Settings display scrolling | `reach_settings_model_display_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Launcher stepped scroll | `reach_launcher_set_result_scroll_offset_state`; Input | Events set offset directly; no easing loop | CPU input/layout: discrete selection and viewport policy, not an animation to migrate. |
| Context Menu, Quick Settings and tray wheel offsets | Capsule pointer handlers; Input | Wheel events directly change offsets | CPU input/layout; review retained reuse on events, no recurring visual driver. |
| Native preview root alignment | `reach_host_native_overlay_placement`, `reach_host_sync_native_overlay`; Native | Root motion samples and DWM immediate properties while geometry changes | Capability decision; preserve source handles and remove unrelated render work if hybrid remains. |
| Bar window push | `reach_host_move_bar_animation_frame`, `reach_bar_reveal_ops.position_frame`; Native | Physical HWND movement and bounded pushed-window positions | CPU native candidate: necessary operations; use common timeline without unrelated repaint. |

## Adjacent frame demand and semantic timing

These do not become GPU algorithms, but their scheduling must not hide redundant visual work:

- Battery saver pending timeout (`saver_pending_seconds`) requests frames while awaiting change;
  replace elapsed-frame accumulation with a deadline/event wake where appropriate.
- Top Bar Bluetooth absence grace (`bluetooth_absent_seconds`) is status policy; preserve the grace
  via event/deadline work, not an animation redraw loop.
- Settings worker pending/in-flight states, scanning statuses, focused edits and scrollbar drag
  flags are included in `reach_settings_app_needs_frame`. Service completion should wake the loop;
  unchanged waiting/drag state alone should not require content or animation frames.
- Clipboard `scrollbar_drag.active` requests frames even when stationary. Pointer events already
  update direct geometry; frame demand must depend on actual motion/content needs.
- Pending Dock unpins, popup-hook reassertion, clock updates, input-language settle and monitor
  retries are semantic/runtime work. Audit deadline/event scheduling without migrating policy to
  the GPU or introducing polling. Existing system-stats sampling is the separately approved exception.

## Inventory evidence and acceptance updates

The audit searched all `include` and `src` C/C++ sources for animation/motion/loader declarations,
manager start/tick/value consumers, retained groups/repetition, delta/phase accumulation, scrollbar
updates, timers, frame predicates and lifecycle deadlines. Every feature and Settings declared
track family is represented above, as are the direct loader/caret/scroll drivers. Shared support,
render/port value objects and adapter animation execution are execution mechanisms, not omitted
feature effects. Future changes must update this ledger and resolve newly discovered effects.

Baseline source build: `cmake --build build/switcher-previews -j 4` passed on 2026-10-11, including
Reach, Settings and architecture checks. `ctest --test-dir build/switcher-previews --output-on-failure
-j 4` passed all 39 tests. This proves build/test health only, not runtime workload efficiency.
Inventory commit: `754de91`. Performance acceptance is not checked off by this inventory.

Battery feedback is the first migrated consumer. Its scalar enum/storage, per-tick advancement,
sampled alpha render function and feedback frame predicate are removed. A shared
`reach_ui_feedback_render` retains a fixed overlay while pressable events publish `reach_motion`
intent. Original-target arming, capture, cancellation and hide resets are preserved. Focused
pressable logic tests cover descriptor generation, cancellation and target retirement.

The pressable style temporarily accepts either its existing manager/track or one motion pointer,
and rejects ambiguous ownership. Migrated Battery and Quick Settings use only motion. Remaining legacy styles are
enumerated above; remove the manager/track style fields and dispatch branches when those consumers
migrate. This bridge is migration work, not an accepted final parallel animation design.

Quick Settings removes its press-feedback scalar enum/track and sampled opacity from render input.
It publishes the descriptor to the shared overlay, preserves output-row scissoring and the existing
target/capture policy, and allocates content on target introduction or geometry changes. Retained
release changes no raster content. Its height and section expansion tracks remain listed separately;
their CPU work does not justify leaving the feedback animation on the CPU.
