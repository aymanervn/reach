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
| Launcher results expansion | Canonical `results_expansion`; `reach_launcher_capsule_tick` | Retained height, clip, per-paint opacity and slide / one endpoint cleanup | Results enter/leave | Implemented: scalar manager, sampled reveal fields and periodic capsule frames removed. Shared compact chrome and generic animated root center preserve the pill and overlapping opening scale. Event-time input and native hit projection share the body. Both builds/all forty tests, focused consumer/appearance/source checks, full native regressions and installed validation pass; broad product/cost acceptance remains open. |
| Context Menu item/header hover | `reach_context_menu::hover`; shared retained opacity and RGBA tint | Retained / header pill and preview border | Hover enter/leave, identity switch | Implemented: canonical motion, window-keyed pixel-aligned sources; scalar track/accessor removed. Border drawing forces opaque alpha, so its visible color is affine and uses the existing tint primitive. Ordinary rows keep immediate feedback. Native/fallback and four-DPI cached-source comparisons pass; final product acceptance remains open. |
| Context Menu close hover | `reach_context_menu::close_hover`; shared UI opacity | Retained opacity / fixed close coverage keyed by window | Close target enter/leave | Implemented: affine alpha preserves 0.14–0.88 range and easing, with separate unchanged glyph. Scalar track/accessor removed; hide/reset clears motion. Both compositor/fallback close checks and subsequent full WUC completion checks pass; final product acceptance stays open. |
| Dock popup width | Canonical `reflow` owner and affine `width`; `window_preview_layout.cpp` | Retained chrome/fitting / active-only native destination sampling | Membership/size/page changes; one cleanup deadline | Implemented: stable envelope, monitor-clamped body/notch, same-epoch fitting, independent titles/controls and event-time input/native geometry. Scalar width manager, periodic tick/frame demand and sampled render rectangles are removed. Both builds/all forty tests and focused displayed/static/motion/resource matrices pass; full Debug/Release regressions and installed watchdog-first/reachctl validation pass. Live alignment and total product acceptance remain open. |
| Dock popup reflow | Canonical shared layout `reflow` and per-card rectangle; `window_preview_animation.cpp` | Retained fitting/translation / active-only native destination sampling | Membership/source changes and interruption; one cleanup deadline | Implemented: width and survivor corrections share one epoch; moving constraints preserve complete cards and event-time positions. Close freezes all geometry; hide resets it. Input snapshots are sampled separately from retained rendering. Native producer, static pixel, constrained paging and maximum-source checks pass; live/product-cost acceptance remains open. |
| Switcher width | Canonical `width`; `reach_switcher_arrange` and shared retained fills/width text | Retained fitting / active-only CPU DWM destination projection | Page/source target changes; one cleanup deadline | Implemented: stable session envelope, proportional equal-height cards, fixed-radius chrome/borders, independent centered title fitting and event-time native/input rectangles. Width scalar manager/tick/frame demand and sampled render geometry are removed. Producer/source/lifetime matrices and intermediate flat-color comparisons pass on both renderers at four DPI values/two themes; antialiased edges are bounded and inspected. Live preview alignment and final product-cost acceptance remain open. |
| Clipboard item/border/close hover | `reach_clipboard_feature::hover`; shared UI RGB blend and retained opacity | Retained / item-keyed outer/inner fills and close coverage | Hover enter/leave, reversal and membership change | Implemented: canonical motion, stable surviving item source ids and original nested RGB/alpha preserved. Hover scalar tracks, sampled values, tick redraw and hover-only frame demand removed. Both builds/tests, scoped pixel/lifetime proofs and full native/fallback suites pass; displayed pixels and total product acceptance remain open. |
| Clipboard height | `REACH_CLIPBOARD_ANIMATION_HEIGHT`; `reach_clipboard_feature_animate_height` | Scalar / surface and viewport | Content/constraint changes | Migrate: reusable retained resize. |
| Clipboard item width | Canonical `item_width`; endpoint layout and event-time input snapshot | Retained borders, close/text/image fitting and scrollbar placement | Gutter changes and exact-time retarget; one cleanup deadline | Implemented: width scalar track/accessor and periodic width frame/layout work removed. Stable source domain preserves custom preview policy. Extremely narrow clipped-glyph geometry uses generic active-only content sampling. Focused and full validation evidence is recorded in the baseline; height, scrolling and total product acceptance remain open. |
| Quick Settings height | `reach_quick_settings::height`, `state.surface_motion`; shared fixed-cap chrome and viewport parents | Retained / chrome, body placement and viewport | Section/content target changes | Implemented: one layout epoch, stable session envelope, event-time input rectangle and scroll clamp; height scalar manager/tick/accessors/blocker removed. Native/fallback, constrained-monitor and four-DPI source-pixel verification pass; displayed pixels and total product acceptance remain pending. |
| Output-device expansion | `state.output_devices_expansion`; Quick Settings retained section parents/leaves | Retained / clip, shared fade/slide, following content and chevrons | Expand/collapse | Implemented: scalar track removed, event-time input and one cleanup deadline; native/fallback and DPI/source-pixel checks pass. Popup height shares the retained layout epoch; final product acceptance remains pending. |
| App-volume expansion | `state.app_volumes_expansion`; Quick Settings retained section parents/leaves | Retained / clip, shared fade/slide and label/chevron crossfades | Expand/collapse | Implemented: same retained contract; scalar track and section-only frame demand removed. Popup height shares the retained layout epoch; final product acceptance remains pending. |
| Quick Settings press feedback | `reach_quick_settings::press_feedback`; shared pressable and `reach_ui_feedback_render` | Retained motion / fixed target overlay | Press/release fade | Implemented: direct absolute-time intent, no feedback scalar advancement/frame demand; native compositor and HWND fallback verification pass. Final visual/performance acceptance remains open. |
| Stage tile progress | `REACH_STAGE_ANIMATION_PROGRESS`; `stage_layout.cpp`, `stage_render.cpp` | Retained geometry, bar height and border fade; native thumbnails sample the same published `destination_motion` | Enter/close | Implemented: tile chrome is one window-keyed group tree driven by `reach_stage_tile.rect_motion`, `bar_motion` and `fade_motion`, so animation frames change properties only and solid fills use no drawing surface. CPU raster remains only for settled tiles (exact legacy shapes), tiles whose motions cannot be composed (two live epochs) and tiles beyond `REACH_MAX_RENDER_GROUPS` (each group tree needs 20 groups); those draw per tile instead of one monitor-sized run. Close choreography and hit testing keep the sampled `current_rect`. |
| Stage desktop progress | `REACH_STAGE_ANIMATION_DESKTOP_PROGRESS`; `stage_layout.cpp` | Scalar/native / desktop thumbnail geometry | Enter/close | Capability decision: desktop source and ordering semantics. |
| Stage reflow | `REACH_STAGE_ANIMATION_REFLOW`; `stage_layout.cpp` | Retained geometry / source rearrangement | Membership changes | Implemented through the shared tile motion; a reflow that overlaps a live open/close epoch falls back to the sampled per-tile raster path for that tile. |
| Stage departure | `REACH_STAGE_ANIMATION_DEPARTURE`; `stage_layout.cpp` | Retained geometry and fade / departing content | Removed source exit | Implemented through the shared tile motion (presence scale and fade); native source lifetime unchanged. |
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
| `power_selection[]` | `settings_power_sleep.cpp`, shared retained selection-item/textbox ink | Option selection/custom timer entry; canonical 220ms ease-out color crossfade and visible endpoint cleanup | Implemented: independent border/background/label and custom-field ink; placeholder, suffix, selection and caret remain independent. Scalar Power manager, sampled updates and periodic frame predicate removed. Review needed: intermediate glyph contrast, live editing/focus and total source/driver cost (R9). This track owns no row sizing/reveal. |
| `power_wait_toggles[]` | `settings_power_sleep.cpp`, shared retained toggle and label tint | Wait option target changes | Implemented: track, knob and label alpha share canonical intent; scalar wait tracks and frame demand removed. Independent option/custom-field colors use the shared retained selection and textbox-ink paths. Final product acceptance pending. |
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
| Now Playing title marquee | Capsule-owned `marquee`; canonical periodic X intent | Content/overflow change starts a readable hold; unchanged content/theme/source recreation preserves absolute phase; hide/disable/reset retires intent | Implemented: legacy cycle fields and both custom native drivers removed. Shared compilation/easing on both backends; generic retained capability keeps fallback static without CPU demand. Both builds/all tests, focused native/capsule and full regression suites pass. Correct installed-folder startup is verified; displayed appearance and final product/power cost remain open. |
| Settings nav selector `nav_selection` | `settings_feature.cpp`, `settings_render.cpp`; Retained | Selection submits finite Y motion | Verify existing and finite fallback. |
| Launcher `surface_y` | `launcher_state.cpp`; Retained | Open/close submission, events and completion | Verify existing. |
| Launcher `surface_opacity` | Same; Retained | Open/close, final visibility reconciliation | Verify existing. |
| Launcher `surface_scale` | Same; Retained | Open/close with proportion-preserving scale | Verify existing. |
| HUD `opacity` | `system_hud_feature.cpp`; Retained | Action submits/restarts open/close; absolute total lifetime | Verify existing; hold must not demand animation frames. |
| Battery popup Y and opacity | `reach_feature_transition` in Battery; Retained | Open/close, absolute deadline and completion | Verify existing. |
| Context Menu popup Y and opacity | Shared transition and generic native-motion scheduling; Retained/native | GPU presentation; active-only CPU DWM projection and one terminal sample | Root presentation no longer requests content frames. Width/card fitting and reflow are retained; live/product acceptance remains pending. |
| Switcher popup Y and opacity | Shared transition and generic native-motion scheduling; Retained/native | GPU presentation; active-only CPU DWM projection and one terminal sample | Root presentation no longer requests content frames. Shared context caching skips unchanged layout/commands/raster; width fitting and native body-motion descriptors are implemented. Live/product acceptance remains pending. |
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
| Wi-Fi `wifi_caret` | Shared `reach_blink` projected into canonical discrete opacity; retained stroke part | Focused visible text; 0.53-second intervals | Implemented end to end: no native caret-driven application update/deadline/build/raster/submission; radio/page/viewport/minimized filtering and reset/generation verified. Unsupported rendering keeps visible-field deadlines. Pixel/source/consumer checks pass; displayed and total product cost acceptance remain open. |
| Account `account_caret` | Same shared owner/retained textbox part | Focused password field; 0.55/0.45 duty cycle | Implemented: exact masked-text layout, focus/edit generation reset and no native deadline. Same optimized deadline fallback and remaining product acceptance. |
| Power `power_caret` | Same shared owner/retained textbox part | Focused visible timer edit; 0.55/0.45 duty cycle | Implemented: trailing alignment, suffix exclusion and viewport filtering preserve ink/input geometry; native blinking stays off the application loop. Same fallback and remaining product acceptance. |
| Launcher `caret` | Same shared owner; capsule renderer capability and retained textbox part | Blink while open; input reset | Implemented: native updates/deadline publication removed; close/suspension retire sources and unsupported rendering restores the existing deadline. Actual capsule producer/scheduler checks pass. Displayed and total product cost acceptance remain open. |
| Clipboard smooth scrolling | `reach_clipboard_tick_scroll`, `reach_scrollbar_update`; Scroll | Offset approaches target; row geometry and scrollbar change | Migrate eligible viewport translation; preserve hit testing and bounded content. |
| Settings Wi-Fi scrolling | `reach_settings_model_wifi_scroll`; Scroll | Offset approaches target | Migrate eligible retained viewport translation, with constraint/model reconciliation. |
| Settings Bluetooth scrolling | `reach_settings_model_bluetooth_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings update scrolling | `reach_settings_model_update_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings startup scrolling | `reach_settings_model_startup_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Settings installed-app scrolling | `reach_settings_model_installed_apps_scroll`; Scroll | Offset approaches target | Same shared scrolling capability; retain bounded virtualization. |
| Settings display scrolling | `reach_settings_model_display_scroll`; Scroll | Offset approaches target | Same shared scrolling capability. |
| Width-text capacity fallback | `reach_d2d_sampled_text`; generic render-port source sampling | Canonical width / only required text raster content | Active finite width plus one endpoint sample | Dock and Switcher consume the shared capability. Existing byte/layer/dimension or curve limits choose bounded ordinary sources; the controlled sixteen-card Dock scene verifies one sampled title under the byte bound. Both loops sample only source content without capsule arrangement/commands. Ownership and endpoint checks pass; total product cost acceptance remains pending. |
| Launcher stepped scroll | `reach_launcher_set_result_scroll_offset_state`; Input | Events set offset directly; no easing loop | CPU input/layout: discrete selection and viewport policy, not an animation to migrate. |
| Context Menu, Quick Settings and tray wheel offsets | Capsule pointer handlers; Input | Wheel events directly change offsets | CPU input policy. Quick Settings publishes a retained scroll parent and clamps against its shared animated content/viewport range; source changes reconcile the descriptor. Context Menu/tray reuse and full live acceptance remain pending. |
| Native preview root alignment | `reach_host_native_overlay_placement`, `reach_host_sync_native_overlay`; Native | Root motion samples and DWM immediate properties while geometry changes | Capability decision; preserve source handles and remove unrelated render work if hybrid remains. |
| Bar window push | `reach_host_move_bar_animation_frame`, `reach_bar_reveal_ops.position_frame`; Native | Physical HWND movement and bounded pushed-window positions | CPU native candidate: necessary operations; use common timeline without unrelated repaint. |

## Adjacent frame demand and semantic timing

These do not become GPU algorithms, but their scheduling must not hide redundant visual work:

- Battery saver pending timeout (`saver_pending_deadline`) now publishes one two-second absolute
  deadline through generic surface geometry, merged with popup completion. Awaiting confirmation
  causes no periodic update/frame request. Acknowledgement and reset cancel it; expiry reconciles once.
- Top Bar Bluetooth absence grace (`bluetooth_absence_deadline`) preserves the two-second glyph hold
  through one semantic wake. Repeated absent snapshots retain the original deadline; recovery and
  reset cancel it. No grace-only frames or frame-delta accumulation remain. Both builds/all forty
  tests and owned before/after scheduling, expiry, recovery and cancellation checks pass.
- Settings worker completion, progress, service notifications, caret deadlines, active scalar
  effects and smooth scrolling wake the loop. Stationary scrollbar drag flags no longer request
  frames; pointer capture and move/release events retain ownership. Worker/status scheduling
  remains subject to the final audit.
- Clipboard stationary scrollbar drags no longer request frames. Pointer events retain capture
  and update direct geometry; existing height and smooth-scroll work still requests updates; item width publishes retained intent.
  Before/after owned scheduling diagnostics pass in both builds; retained sizing/scroll migration
  and total product acceptance remain open.
- Pending Dock unpins, popup-hook reassertion, clock updates, input-language settle and monitor
  retries are semantic/runtime work. Audit deadline/event scheduling without migrating policy to
  the GPU or introducing polling. Existing system-stats sampling is the separately approved exception.

## Inventory evidence and acceptance updates

Finite scalar tracks now share `reach_motion` timing, easing and generation with retained effects.
The delta tick and unused scalar crossing API are removed across every production caller. Retargeting
uses input time rather than the last rendered value; one sample retires pending endpoint activity.
Settings samples its two remaining scalar families (Wi-Fi/Bluetooth row expansion) at one update
timestamp; Power selection uses canonical retained color intent and an endpoint cleanup deadline. Manager intent can be
published unchanged to retained groups. Existing scalar consumers still need their visual/frame
migration, and direct loader/scroll/native choreography timing remains separately pending.

The shared retained adapter caches ordered group/parent ownership and scene content matrices.
Initial scalar values and finite/repeating native curves now share one timestamp per submission;
the former finite path sampled each property separately. Completion publication restores the
previous sampling scope. This removes intra-submission phase drift without adding application
frames or a second timeline. It does not resolve the separate Stage text filtering or snapped
chrome geometry differences; their consumer acceptance remains open.
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


## Review markers for the current continuation

**Review needed:** all broad live/product acceptance entries remain open in
[gpu-composition-review.md](gpu-composition-review.md). On 2026-10-17 the user resumed the full goal
and authorized restarts under the checklist's watchdog-first, existing-folder reachctl protocol.
Preserve Explorer and do not reinstall or repair shell/task registration. Startup limitations
leave the affected live checks pending. R3 covers Clipboard width and its measured edge/memory costs; R4 covers semantic Battery/
Bluetooth policy; R1/R2/R5/R6 cover the other implemented primitives/consumers and environment/
lifecycle behavior; R7/R8 cover complete performance and final ledger/code acceptance.
