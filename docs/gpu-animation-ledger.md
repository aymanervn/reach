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

- **Scalar:** `reach_animation_manager_sample_at` samples canonical absolute-time `reach_motion`
  intent. Consumers still bake cached values into commands/layout and request application frames
  until their final endpoint sample. Retained caching may avoid
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
The latter preserves semantic timing/event work, not an exemption for per-frame rendering. The selected [live-preview design](gpu-live-preview-design.md) retains DWM live content with
active-only native synchronization and retained Reach visuals; its consumers remain pending.
Capability decisions must resolve to an implemented compositor path or an evidenced, optimized hybrid path.
No current CPU visual path has final measured acceptance yet. Retained resource and source lifetime
are evaluated independently from the animation's active lifetime.

## Reach scalar consumers

| Owner/effect | Source symbol and consumer | Driver / changing work | Active lifetime | Initial disposition and reason |
| --- | --- | --- | --- | --- |
| Dock reveal Y | `REACH_DOCK_ANIM_Y`; `reach_dock_bar_update_visibility`, shared bar visibility | Scalar/native window geometry | Reveal/hide to target | Capability decision: physical HWND and pushed-window geometry remain necessary; evaluate drawn motion separately. |
| Dock press feedback | `reach_dock::press_feedback`; shared pressable and UI/icon feedback | Retained opacity / trigger RGBA tint | Press, disarm, release fade | Implemented: scalar track, sampled alpha and frame blocker removed. Flat feedback groups share item movement descriptors. Build/tests and bounded compositor/fallback diagnostics pass; final visual/performance acceptance remains open. |
| Dock insertion/removal slots | `REACH_DOCK_ANIM_SLOT_BASE`; `reach_dock_slot_reveal` | Scalar / packed widths, presence and capacity layout | Slot target changes, including departing sources | Migrate: retain item content and animate eligible geometry; reconcile capacity constraints explicitly. |
| Top Bar reveal Y | `REACH_TOP_BAR_ANIM_Y`; `reach_top_bar_build_layout` | Scalar/native window push | Reveal/hide to target | Capability decision: preserve common physical displacement; separate visual work. |
| Top Bar power hover | `reach_top_bar::power_hover`; shared UI RGBA tint | Retained color / fixed rounded background | Enter/leave fade | Implemented: original easing and theme RGBA interpolation retained; glyph cached separately. Scalar track, tick/redraw and bar frame blocker removed. Native reversal/reset diagnostics pass on both compositors and fallback; final visual/performance acceptance remains open. |
| Top Bar press feedback | `reach_top_bar::press_feedback`; shared UI/icon feedback | Retained opacity / overflow RGBA tint | Press/release fade | Implemented: scalar track, sampled alpha/colors and feedback frame demand removed. Tray feedback inherits item movement. Bounded native/fallback diagnostics pass; widths remain separate pending consumers. |
| Now Playing width | `REACH_TOP_BAR_ANIM_NOW_PLAYING_WIDTH`; `reach_top_bar_build_layout` | Scalar / content and bar geometry | Visibility/content target change | Migrate: shared retained resize and positioning without stretching content. |
| Current app width | `REACH_TOP_BAR_ANIM_CURRENT_APP_WIDTH`; `reach_top_bar_build_layout` | Scalar / text-sized geometry | Identity/title target change | Migrate: target metrics and retained positioning. |
| Tray width | `REACH_TOP_BAR_ANIM_TRAY_WIDTH`; `reach_top_bar_build_layout` | Scalar / tray/bar geometry | Membership target change | Migrate: retain stable icon identities. |
| Quick Settings control width | `REACH_TOP_BAR_ANIM_QUICK_SETTINGS_WIDTH`; `reach_top_bar_build_layout` | Scalar / control/bar geometry | System-state target change | Migrate: shared width capability. |
| Tray popup feedback | `reach_top_bar_tray_popup::press_feedback`; `reach_top_bar_append_tray_render_commands` | Retained motion / icon tint or fallback rectangle | Press/release fade | Implemented: shared absolute-time pressable intent and retained opacity; scalar track/manager accessor and composition frame blocker removed. Both compositors and fallback pass bounded capsule diagnostics. Final visual/performance acceptance remains open. |
| Battery press feedback | `reach_battery::press_feedback`; `reach_battery_append_render_commands` | Retained motion / fixed saver-row overlay | Press/release fade | Implemented: shared pressable absolute-time intent and retained opacity; no capsule feedback ticks/frame demand. HWND fallback samples published motion. Native operation-count verification passed; final visual/performance acceptance remains open. |
| Launcher results expansion | `REACH_LAUNCHER_ANIMATION_RESULTS_EXPANSION`; `reach_launcher_capsule_tick` | Scalar / results height and reveal | Results enter/leave | Migrate: endpoint layout, animated clip and eligible chrome. |
| Context Menu item/header hover | `reach_context_menu::hover`; shared retained opacity and RGBA tint | Retained / header pill and preview border | Hover enter/leave, identity switch | Implemented: canonical motion, window-keyed pixel-aligned sources; scalar track/accessor removed. Border drawing forces opaque alpha, so its visible color is affine and uses the existing tint primitive. Ordinary rows keep immediate feedback. Native/fallback and four-DPI cached-source comparisons pass; final product acceptance remains open. |
| Context Menu close hover | `reach_context_menu::close_hover`; shared UI opacity | Retained opacity / fixed close coverage keyed by window | Close target enter/leave | Implemented: affine alpha preserves 0.14–0.88 range and easing, with separate unchanged glyph. Scalar track/accessor removed; hide/reset clears motion. Both compositor/fallback close checks and subsequent full WUC completion checks pass; final product acceptance stays open. |
| Dock popup width | `REACH_CONTEXT_MENU_ANIM_WIDTH`; `window_preview_layout.cpp` | Scalar / popup packing and chrome | Instance membership/size changes | Capability decision: resize chrome and native destinations must align. |
| Dock popup reflow | Canonical `reflow` and per-card `x`; `window_preview_animation.cpp` | Retained translation / active-only native destination sampling | Membership changes and interruption; one cleanup deadline | Implemented horizontal reflow: scalar track/sample-position writes removed; retained card parents, DWM rectangles and hit testing share intent. Width/card sizing, live alignment and total workload acceptance remain pending. |
| Switcher width | `REACH_SWITCHER_ANIMATION_WIDTH`; `reach_switcher_arrange` | Scalar / page fitting and chrome | Page/source target changes | Capability decision: shared preview/resize contracts. |
| Clipboard item/border/close hover | `reach_clipboard_feature::hover`; shared UI RGB blend and retained opacity | Retained / item-keyed outer/inner fills and close coverage | Hover enter/leave, reversal and membership change | Implemented: canonical motion, stable surviving item source ids and original nested RGB/alpha preserved. Hover scalar tracks, sampled values, tick redraw and hover-only frame demand removed. Both builds/tests, scoped pixel/lifetime proofs and full native/fallback suites pass; displayed pixels and total product acceptance remain open. |
| Clipboard height | `REACH_CLIPBOARD_ANIMATION_HEIGHT`; `reach_clipboard_feature_animate_layout_value` | Scalar / surface and viewport | Content/constraint changes | Migrate: reusable retained resize. |
| Clipboard item width | `REACH_CLIPBOARD_ANIMATION_ITEM_WIDTH`; Clipboard animated layout | Scalar / item geometry | Content/constraint changes | Migrate: target layout and retained movement/reveal. |
| Quick Settings height | `reach_quick_settings::height`, `state.surface_motion`; shared fixed-cap chrome and viewport parents | Retained / chrome, body placement and viewport | Section/content target changes | Implemented: one layout epoch, stable session envelope, event-time input rectangle and scroll clamp; height scalar manager/tick/accessors/blocker removed. Native/fallback, constrained-monitor and four-DPI source-pixel verification pass; displayed pixels and total product acceptance remain pending. |
| Output-device expansion | `state.output_devices_expansion`; Quick Settings retained section parents/leaves | Retained / clip, shared fade/slide, following content and chevrons | Expand/collapse | Implemented: scalar track removed, event-time input and one cleanup deadline; native/fallback and DPI/source-pixel checks pass. Popup height shares the retained layout epoch; final product acceptance remains pending. |
| App-volume expansion | `state.app_volumes_expansion`; Quick Settings retained section parents/leaves | Retained / clip, shared fade/slide and label/chevron crossfades | Expand/collapse | Implemented: same retained contract; scalar track and section-only frame demand removed. Popup height shares the retained layout epoch; final product acceptance remains pending. |
| Quick Settings press feedback | `reach_quick_settings::press_feedback`; shared pressable and `reach_ui_feedback_render` | Retained motion / fixed target overlay | Press/release fade | Implemented: direct absolute-time intent, no feedback scalar advancement/frame demand; native compositor and HWND fallback verification pass. Final visual/performance acceptance remains open. |
| Stage tile progress | `REACH_STAGE_ANIMATION_PROGRESS`; `stage_layout.cpp` | Scalar/native / geometry and close landing | Enter/close | Capability decision: preserve synchronized native close. |
| Stage desktop progress | `REACH_STAGE_ANIMATION_DESKTOP_PROGRESS`; `stage_layout.cpp` | Scalar/native / desktop thumbnail geometry | Enter/close | Capability decision: desktop source and ordering semantics. |
| Stage reflow | `REACH_STAGE_ANIMATION_REFLOW`; `stage_layout.cpp` | Scalar/native / source rearrangement | Membership changes | Capability decision: stable sources and shared motion. |
| Stage departure | `REACH_STAGE_ANIMATION_DEPARTURE`; `stage_layout.cpp` | Scalar/native / departing content | Removed source exit | Capability decision: retiring source lifetime and geometry. |
| Stage close hover | `reach_stage::close_hover`; shared UI RGBA tint | Retained color / separate background and glyph coverage | Hover enter/leave | Implemented: pointer events submit canonical motion independently of geometry ticks; scalar enum/state sampling removed. Theme RGBA, presence alpha and reversal preserved. Bounded native/fallback checks and subsequent full WUC completion checks pass; final acceptance remains pending. |
| Stage backdrop | `REACH_STAGE_ANIMATION_BACKDROP`; `reach_stage_thumbnail_at`, Stage native-overlay registration, DWM behind-target host | Scalar/native / layered desktop host opacity | Synchronized close reveal | Native-preview migration dependency: the backdrop is the desktop thumbnail host's background and whole-HWND alpha, not a Stage render command. The supported immediate port requires active-only canonical CPU projection with the preview path; no independent retained Stage layer is selected. Preserve desktop source/host ordering and close synchronization. |
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
| `wifi_radio_toggle` | `settings_wifi.cpp`, shared retained toggle | Radio target changes | Implemented: canonical track tint/knob translation; scalar radio track, sampled commands and frame demand removed. Native/fallback verification passes; final product acceptance pending. |
| `bluetooth_row_tracks[]` | `settings_bluetooth.cpp`, Bluetooth layout/render | Expand/collapse | Migrate: same section capability. |
| `bluetooth_radio_toggle` | `settings_bluetooth.cpp`, shared retained toggle | Radio target changes | Implemented: same retained primitive; scalar radio track, sampled commands and frame demand removed. Final product acceptance pending. |
| `startup_toggles[]` | `settings_startup_apps.cpp`, shared retained toggle | Enable/disable target changes | Implemented: list refresh resets intent, entry changes retarget the live curve; scalar manager/tick/frame APIs removed. Full-capacity page command ownership verified. Final product acceptance pending. |
| `power_tracks[]` | `settings_power_sleep.cpp`, `settings_render.cpp` | Custom timer entry/reconfiguration | Migrate: preserve picker/edit reveal with retained content. |
| `power_wait_toggles[]` | `settings_power_sleep.cpp`, shared retained toggle and label tint | Wait option target changes | Implemented: track, knob and label alpha share canonical intent; scalar wait tracks and frame demand removed. Picker/edit reveal remains separate. Final product acceptance pending. |
| `display_fps_toggle` | `settings_monitors_scaling.cpp`, shared retained toggle | Refresh preference changes | Implemented: retained track/knob; scalar track and frame demand removed, refresh policy unchanged. Final product acceptance pending. |
| `display_font_toggle` | `settings_monitors_scaling.cpp`, shared retained toggle | Font preference changes | Implemented: retained track/knob; scalar track and frame demand removed, actual font changes still invalidate content. Final product acceptance pending. |
| Reach theme selection | Canonical `display_theme_selection`; shared retained segmented control | Theme selection | Implemented: cached pill translation and affine label tint; scalar manager/sample/frame demand removed without changing theme policy. Both builds/tests, scoped native/fallback pixels and full native suites pass; displayed pixels and total acceptance pending. |
| Top Bar style selection | Canonical `top_bar_style_selection`; shared retained segmented control | Style selection | Implemented: cached pill translation and exact peak-shaped interior label weights preserve outer jumps/reversal. Scalar manager/sample/frame demand removed. Both builds/tests, scoped native/fallback pixels and full native suites pass; displayed pixels and total acceptance pending. |
| `top_bar_mode_toggle` | `settings_top_bar.cpp`, shared retained auto-hide toggle | Mode selection | Implemented: this control is a binary toggle, not a segmented selector; retained track/knob replace its scalar track and frame demand. Style selection uses the shared retained selector. Final product acceptance pending. |
| `button_press_feedback` | `settings_feature.cpp`, shared UI buttons and app routing | Absolute-time retained RGBA tint / original-target feedback | Implemented: scalar manager, tick and sampled colors removed. Both compositors reuse coverage; fallback schedules the same descriptors to their final frame. Build, tests and bounded diagnostics pass; displayed compositor pixels and total product cost remain pending. |

## Already retained presentation and movement

| Effect / owner | Source and driver | Present work / lifetime | Initial disposition |
| --- | --- | --- | --- |
| Dock neighbour movement `item_x[]` | `dock_feature.cpp`, `dock_render.cpp`; Retained | Input/membership publishes motion; event-time hit samples; finite move | Verify existing, including interaction with slot fitting. |
| Dock released item `drag.snap` | Dock drag and render; Retained | Release submits finite snap; stable identity | Verify existing; stopped drag must cause no periodic frames. |
| Tray neighbour movement `tray_x[]` | `top_bar_interaction.cpp`, `top_bar_render.cpp`; Retained | Drag/reorder submits finite movement | Verify existing, including width changes. |
| Tray released icon `tray_drag.snap` | Same tray interaction/render; Retained | Release submits finite snap | Verify existing. |
| Now Playing title marquee | `top_bar_now_playing.cpp`, `reach_render_group.repeat_offset_x`; Retained repetition | Content change submits one clipped title; unchanged source preserves the native loop; hide/disable/font changes retire it | Native audit complete in both builds/backends, themes and four DPI values. Logical speed/threshold now scale consistently; native starts are counted. Existing backend easing differs; canonical curve/phase unification, live appearance and power/product cost remain pending. |
| Settings nav selector `nav_selection` | `settings_feature.cpp`, `settings_render.cpp`; Retained | Selection submits finite Y motion | Verify existing and finite fallback. |
| Launcher `surface_y` | `launcher_state.cpp`; Retained | Open/close submission, events and completion | Verify existing. |
| Launcher `surface_opacity` | Same; Retained | Open/close, final visibility reconciliation | Verify existing. |
| Launcher `surface_scale` | Same; Retained | Open/close with proportion-preserving scale | Verify existing. |
| HUD `opacity` | `system_hud_feature.cpp`; Retained | Action submits/restarts open/close; absolute total lifetime | Verify existing; hold must not demand animation frames. |
| Battery popup Y and opacity | `reach_feature_transition` in Battery; Retained | Open/close, absolute deadline and completion | Verify existing. |
| Context Menu popup Y and opacity | Shared transition and generic native-motion scheduling; Retained/native | GPU presentation; active-only CPU DWM projection and one terminal sample | Root presentation no longer requests content frames. Card reflow is retained; body width and live/product acceptance remain pending. |
| Switcher popup Y and opacity | Shared transition and generic native-motion scheduling; Retained/native | GPU presentation; active-only CPU DWM projection and one terminal sample | Root presentation no longer requests content frames. Shared context caching skips unchanged layout/commands/raster; width fitting, native body-motion descriptors and live/product acceptance remain pending. |
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
| Wi-Fi loader `wifi_loader` | `settings_wifi.cpp`, shared loader renderer; Retained | Absolute periodic grow/shrink and translation; status events own visibility | Implemented: fixed-cap sources and static rounded viewport on both compositors; no phase ticks or scan-driven frames. Unsupported backends and tiny bounds use explicit sampled geometry; displayed/product acceptance pending. |
| Bluetooth loader `bluetooth_loader` | `settings_bluetooth.cpp`, shared loader renderer; Retained | Same loader intent; scan completion retires sources | Implemented: shared loader and event wake; same backend/tiny-bound fallback and final acceptance limits. |
| Update loader `update_loader` | `settings_update.cpp`, shared loader renderer; Retained | Scanning publishes repetition; worker progress/status is event data | Implemented: shared loader; local completion/progress wakes the sleeping application without in-flight polling. Same fallback and final acceptance limits. |
| Installed apps loader `installed_apps_loader` | `settings_installed_apps.cpp`, shared loader renderer; Retained | Busy snapshot owns visibility; completion retires repetition | Implemented: shared loader and service wake; same fallback and final acceptance limits. |
| Wi-Fi `wifi_caret` | Shared `reach_blink`, Settings visible-field idle deadline | Focused visible text; 0.53-second intervals | Implemented deadline scheduling: phase/tick API and focus-driven continuous frame demand removed; page/blur, radio-off, viewport and minimized filtering verified. Compound textbox still draws at blink boundaries; retained-caret comparison and final product cost remain pending. |
| Account `account_caret` | Same shared deadline model and Settings scheduling | Focused visible text; 0.55/0.45 duty cycle | Implemented: focus/input resets and late-wake sampling preserve nominal timing; no periodic frame demand between boundaries. Isolated scheduler/command checks pass; independent retained content and final product cost remain pending. |
| Power `power_caret` | Same shared deadline model and Settings scheduling | Focused visible timer edit; 0.55/0.45 duty cycle | Implemented: same scheduling, with custom-field viewport filtering. Isolated scheduler/command checks pass; retained-caret comparison and final product cost remain pending. |
| Launcher `caret` | Shared `reach_blink`, capsule-published deadline | Blink only while open; input reset | Implemented shared timing: closing/hidden reset stops its deadline; render snapshot and edit resets verified. Compound textbox invalidation and final cost comparison remain pending. |
| Clipboard smooth scrolling | `reach_clipboard_tick_scroll`, `reach_scrollbar_update`; Scroll | Offset approaches target; row geometry and scrollbar change | Migrate eligible viewport translation; preserve hit testing and bounded content. |
| Settings Wi-Fi scrolling | `reach_settings_model_wifi_scroll`; Scroll | Offset approaches target | Migrate eligible retained viewport translation, with constraint/model reconciliation. |
| Settings Bluetooth scrolling | `reach_settings_model_bluetooth_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings update scrolling | `reach_settings_model_update_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings startup scrolling | `reach_settings_model_startup_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings installed-app scrolling | `reach_settings_model_installed_apps_scroll`; Scroll | Offset approaches target | Same shared scrolling capability; retain bounded virtualization. |
| Settings display scrolling | `reach_settings_model_display_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Launcher stepped scroll | `reach_launcher_set_result_scroll_offset_state`; Input | Events set offset directly; no easing loop | CPU input/layout: discrete selection and viewport policy, not an animation to migrate. |
| Context Menu, Quick Settings and tray wheel offsets | Capsule pointer handlers; Input | Wheel events directly change offsets | CPU input policy. Quick Settings publishes a retained scroll parent and clamps against its shared animated content/viewport range; source changes reconcile the descriptor. Context Menu/tray reuse and full live acceptance remain pending. |
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

Finite scalar tracks now share `reach_motion` timing, easing and generation with retained effects.
The delta tick and unused scalar crossing API are removed across every production caller. Retargeting
uses input time rather than the last rendered value; one sample retires pending endpoint activity.
Settings samples its five remaining scalar families at one update timestamp. Manager intent can be
published unchanged to retained groups. Existing scalar consumers still need their visual/frame
migration, and direct loader/scroll/native choreography timing remains separately pending.

The shared retained adapter caches ordered group/parent ownership and scene content matrices.
Raster edits and property motion do not rebuild unchanged native trees. Membership/order/reparenting,
font-source replacement and recreation invalidate the appropriate ownership; partial rebuilds are
not cached as successful. The two-layer native comparison removes 300 tree mutations and 100 matrix
writes per 100 unchanged submissions on both backends. Full Debug/Release native suites and focused
recovery/lifecycle checks pass. This improves execution of all retained consumers; it does not close
their remaining feature migrations, displayed-pixel verification or total workload acceptance.

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

All pressable feedback now uses one `reach_motion` pointer. The final Dock/Top Bar consumers
removed the temporary manager/track style fields, scalar dispatch branches and sampled-value API.
Input capture, arming, cancellation, drag blocking and context-feedback latching remain owned by
shared pressable intent; renderers consume its descriptor without scalar advancement.

Quick Settings removes its press-feedback scalar enum/track and sampled opacity from render input.
It publishes the descriptor to the shared overlay, preserves output-row scissoring and the existing
target/capture policy, and allocates content on target introduction or geometry changes. Retained
release changes no raster content. Its height track remains pending; section expansions now use retained motion and a cleanup deadline;
their CPU work does not justify leaving the feedback animation on the CPU.

Tray popup feedback also removes its sole scalar track, manager accessor and composition
position-frame blocker. Its icon-tint helper keeps the original command content and animates
retained opacity; fallback rectangles use the shared UI feedback helper. The scalar icon helper was subsequently removed with Dock and Top Bar feedback migration. Canonical explicit-time and affine
motion helpers landed in `18153ae`; tray feedback landed in `6845768`, with Debug build, full tests
and bounded native verification. Supported preview design is recorded in `426d6e2`.

Settings buttons now derive background and optional text RGBA channels from one press descriptor.
Shared UI rendering emits fixed white coverage into independently identified retained groups. The
Windows adapter owns WUC mask/color/property objects and DirectComposition color-matrix effects.
DirectComposition uses Visual3 opacity, freeing the effect slot for tint and removing obsolete
opacity-effect groups. The fallback tints aggregate coverage once, including overlaps, instead of
replacing command colors independently. It samples all group/presentation properties at one frame
time. Settings schedules generic command-buffer fallback motion with a final endpoint frame;
feedback retirement happens on the next input, with no compositor feedback ticks.

Dock and Top Bar feedback owns one auxiliary flat group per active target, copying item placement
where needed. This preserves painter order and alignment without nested render groups. Trigger
and overflow glyphs use shared RGBA tint; ordinary controls and icon tint use retained opacity.
Legacy overlay alpha replaces theme alpha, preserved explicitly in the retained command. Remaining
width/slot/hover scalar tracks are independently pending and do not advance migrated press feedback.
