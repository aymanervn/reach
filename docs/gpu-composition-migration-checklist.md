# GPU composition migration

Status: migration active; inventory and initial dispositions complete, shared feedback consumers
migrating, broader implementation and performance acceptance pending. Evidence is in the
[animation ledger](gpu-animation-ledger.md).
Counter verification and remaining measurement prerequisites are in the
[workload baseline](gpu-workload-baseline.md).

Follow the [production migration plan](gpu-workload-migration-plan.md) for design constraints,
dependencies and validation details. This checklist tracks delivery toward the approved final
GPU/CPU workload balance in both Reach and Settings.

Eligible visual motion runs through retained GPU composition. CPU work owns policy, layout, input
and justified native synchronization. Small gains justify migration when complexity is modest or
shared benefit makes it worthwhile. Appearance and behavior must be preserved without meaningful
regressions in total CPU/GPU cost, latency, power or memory.

Check an item only after its implementation or decision and required verification are complete.
Record evidence, relevant commits and any remaining limitations beside completed items. Existing
compositor paths still require audit and measurement. Remove each consumer's superseded path in
the same migration change. Unavailable acceptance evidence stays pending until obtained or its
support expectation is explicitly resolved.

## 1. Inventory and baseline

- [x] Inventory every animation in Reach and Settings, including manager tracks, retained motion,
  loaders, carets, direct timers, native synchronization, hidden and idle behavior.
- [x] Create a per-effect ledger recording ownership, driver, invalidation, lifetime and GPU/CPU
  disposition; require a specific rationale and evidence for each retained CPU visual path.
  Inventory, ledger and opt-in workload counters: `754de91`; isolated Debug build and 39 tests pass.
- [ ] Establish repeatable baseline measurements for application wakeups, layout, command production,
  rasterization, presents, native updates, total Reach/Settings plus DWM CPU/GPU cost, latency, power
  and memory; establish variation and regression thresholds before comparison.

  - [x] Resolve the optimized-build blocker and verify protocol storage reset behavior.
    Three wire-state aggregate resets use explicit representation clearing with compile-time trivial
    and standard-layout guards. Normal `/O2 /Ob2` flags remain enabled. Isolated Reach/Settings/tool
    Release builds, all 39 Release tests, Debug builds/tests and the owned native protocol probe pass.
    Optimized compositor deduplication failures were resolved by semantic equality; full optimized
    and Debug native suites now pass. Workload capture and final acceptance remain pending.
  - [x] Add and verify bounded read-only CPU/GPU/power/memory collection for existing process PIDs.
    The standalone collector preserves native/PDH attribution, per-engine GPU identity, package
    energy units, optional NVML board power, missing-data statuses and observer/system activity.
    Ten-second Reach/DWM and unavailable-process captures, five pure accounting tests, isolated
    Debug/Release builds, all 40 tests and architecture checks pass. See the workload baseline for
    source contracts, overhead and evidence. Controlled repeated product baselines, variation
    thresholds, latency and independent GPU-memory validation remain pending.

## 2. Resolve production capabilities

- [ ] Select and verify reusable animated clipping, reveal and resize mechanisms on Windows
  Composition and DirectComposition without stretching text, icons, borders or thumbnail proportions.
  Animated group insets are implemented on both backends, with a shared fallback/event-time clip
  sampler. Bounded diagnostics verify reversal, unchanged raster content and native clip retirement;
  focused geometry tests and Debug builds pass. Displayed pixels, complete reveal/resize consumers
  and remaining compact/horizontal-notch capabilities are pending, so this gate stays open.
  - [x] Implement fixed-cap retained chrome using independent axis scale on existing groups.
    Geometry validation prevents corner/shadow distortion, unsupported compact sizes, stretched
    text/icons and horizontal notch stretching. Both renderers, fallback, root completion and
    input/native projection share the axis contract. Debug builds, 39 tests and bounded native
    growth/reversal plus four-DPI pixel comparison pass; complete feature consumers, displayed
    compositor pixels and the remaining small/notched-width capabilities stay pending.
  - [x] Support retained viewport/content hierarchy with shared event-time geometry.
    Empty parents use existing native visuals without raster allocation; content movement,
    stationary inner clips and ancestor motion compose without a new clock. Preserve painter
    order and validate parent ownership; reparenting and parent-content retirement reuse children.
    Debug builds, all 39 tests and five full native runs per backend plus HWND fallback pass.
    Quick Settings height/section integration is implemented; displayed-pixel/product acceptance remains pending.
- [ ] Decide whether supported live-preview composition meets behavior and efficiency requirements;
  document the selected compositor or optimized hybrid design for Dock, Switcher and Stage.
  - [x] Select the existing DWM port with retained Reach visuals and active-only native motion.
    Supported API limits and five bounded adapter runs are recorded in the
    [live-preview design](gpu-live-preview-design.md). Consumer migration, live pixels and total
    workload verification remain pending, so the main capability gate stays open.

## 3. Shared motion and runtime

- [ ] Unify animation intent around canonical absolute-time motion, easing and generation semantics;
  preserve interruption, reversal, cancellation and deterministic CPU sampling where required.
  - [x] Add explicit event-time start/retarget/active/crossing APIs and affine motion derivation;
    popup and Launcher presentation properties now share one timestamp. Focused non-UI tests cover
    retarget continuity, affine sampling and endpoint deadlines. The scalar manager remains pending.
  - [x] Add shared bounded affine motion and native piecewise compilation for section reveals.
    Preserve delayed fade/early saturation, negative affine mapping, event-time retarget and owner
    deadlines. Both native renderers, fallback, root endpoints/envelopes and fixed-cap normalization
    consume the limits; the old adapter-local compiler is removed. Debug builds, all 39 tests and
    five full native runs per backend plus HWND fallback pass. Quick Settings height/section
    integration is implemented; displayed-pixel/product-performance acceptance remains pending.
- [ ] Separate content/layout invalidation, compositor submission, native placement and input updates
  in both application loops; pure compositor motion must not request periodic application frames.
- [ ] Verify completion, stale-generation rejection, hide/reset and supported fallback semantics on
  both backends; land one complete consumer and remove its obsolete tick/redraw path.
  - [x] Migrate Battery press feedback end to end using shared pressable intent and a retained
    opacity layer; remove its scalar track, tick, sampled render value and frame demand. Build,
    39 tests and bounded native diagnostics pass on both compositors and the HWND fallback.
    See the workload baseline for evidence and the remaining visual/performance acceptance.

  - [x] Resolve WUC completion deferred by occlusion: wait for every canonical root motion, then
    reconcile final properties with one asynchronous commit and synchronization if the batch is
    pending. Component retarget/reset retire old completion ownership. Existing runtime deadline
    waking is reused; no timer/frame loop added. Debug builds, 39 tests and five full native runs
    per backend pass without exclusions; unoccluded native verification also passes. Broader
    failure/recreation, displayed-pixel and product-performance acceptance remain pending.

  - [x] Report terminal presentation errors separately from pending completion; latch failures
    until retarget/reset, suspend failed composition and release ordinary close cleanup. Hidden
    capsules stop completion queries when their deadline clears. Debug builds, 39 tests and five
    bounded native runs per backend pass, including injected async error/cancel/result failures,
    unchanged terminal queries, stale generations and suspension/recreation. Actual device-loss,
    fence failure and full live acceptance remain pending in the broader resource gate.

  - [x] Make retained intent comparison independent of object padding in optimized builds.
    Shared named-field motion/group equality covers tint, clips, transforms and root completion
    ownership. Preserve generation, clock, limits and hierarchy changes. Pure padding regressions,
    both builds, all 39 tests and five full native runs per backend plus fallback pass in Release
    and Debug. Padding-only changes and unchanged settled chrome submit no properties or raster
    work; real generations still update ownership. Total product acceptance remains pending.

## 4. Feedback and repetition

- [ ] Migrate eligible hover, press, selection and close feedback across Context Menu, Battery, tray,
  Clipboard, Quick Settings, Dock, Top Bar and Settings using shared retained primitives.
  - [x] Battery press/release overlay: `d3f71dd`; obsolete scalar path removed, build/tests and both
    compositor/fallback diagnostics pass.
  - [x] Quick Settings press/release overlay uses the same shared primitive; obsolete feedback track
    and sampled alpha removed (`e4c133e`). Build, 39 tests and native compositor/fallback diagnostics pass.
    Height and section expansion now use retained layout motion; final product acceptance remains open.
  - [x] Tray popup press/release uses retained icon-tint or rectangle opacity, preserving its
    previous appearance. The scalar track, accessor and composition frame blocker are removed.
    Debug builds, 39 tests and bounded capsule diagnostics pass on both compositors and fallback.
  - [x] Settings button press/release, including uninstall background and text colors, uses shared
    retained RGBA tint over cached coverage. Remove the scalar feedback manager and sampled colors;
    generic Settings fallback scheduling samples every published motion through its final frame.
    Debug builds, 39 tests and bounded native/fallback diagnostics pass at 100%, 125% and 200%
    geometry scales. GPU color-matrix and fallback coverage readback pass. Displayed compositor
    pixels, live input and total product cost remain part of final acceptance.
  - [x] Migrate Settings' animated binary toggles and the power wait label fade.
    Shared cached track tint and knob translation preserve a stationary viewport and pixel-aligned
    sources. Display/font preferences, top-bar auto-hide, Wi-Fi/Bluetooth radio state, startup
    entries and power wait options use canonical event-time reversal; their scalar tracks and
    frame demand are removed. Both builds and all 40 tests pass. Five full native runs per backend
    plus fallback pass in Debug/Release; full pages, maximum startup membership, themes, four DPI
    values and nested scope are checked. Fallback source pixels match exactly; cached endpoints
    differ by at most one channel value. Intermediate filtering stays at the moving knob's
    antialiased edge and is bounded/inspected. Displayed pixels and total product cost remain open.
  - [x] Dock and Top Bar press/release feedback uses retained opacity or RGBA tint. Feedback
    shares item movement descriptors in separate flat groups; scalar feedback tracks, sampled
    alpha/colors, frame blockers and the final shared pressable scalar bridge are removed.
    Debug builds, 39 tests and five bounded capsule runs per compositor plus HWND fallback pass.
    Native cancellation reuses content; live pixels, icon/DPI churn and product cost remain pending.
  - [x] Top Bar power hover uses the shared retained RGBA tint with a separately cached glyph.
    Preserve enter/leave easing; remove scalar advancement, redraw/frame demand and bar blocker.
    Reset and game-mode suppression clear the motion. Debug build, 39 tests and five bounded runs
    per compositor plus HWND fallback pass, including reversal with no retained raster work.
    Displayed pixels, live DPI/theme changes and total product cost remain in final acceptance.
  - [x] Dock preview close hover uses stable window groups and shared retained opacity. Preserve
    alpha range, easing and pointer behavior; remove its scalar track/accessor and clear hidden motion.
    Debug build, 39 tests and five close-hover runs per compositor plus fallback pass with no
    retained raster work. Subsequent full native runs include the resolved WUC completion path;
    final product acceptance is still pending.
  - [x] Context Menu preview header/border hover uses canonical retained opacity and RGBA tint.
    Remove the scalar hover track/accessor and its frame demand; retain ordinary rows' immediate
    feedback and existing leave/identity/reset behavior. Window-keyed layers share pixel-aligned
    source bounds. The final border helper forces opaque alpha, so the existing affine tint preserves
    its visible color. Debug build, 39 tests and five full native runs per backend plus fallback pass;
    native hover retargets have zero raster passes/scalar advances. Five hover values at four DPI
    settings, including fractional placement, match the former fallback pixels exactly and cached
    composition within one channel value. Displayed pixels and total product cost remain pending.
  - [x] Stage close hover uses the shared retained RGBA tint for background and glyph. Pointer
    events submit changes directly; remove the scalar enum/state sampling and feedback frame demand.
    Preserve theme colors, presence alpha, reversal, source retirement and hide/game-mode reset.
    Debug build, 39 tests and five scoped native runs per compositor plus fallback pass. The known
    WUC root-completion check was excluded from the initial scoped run; subsequent full native
    runs include that resolved path. Final product acceptance remains pending.
- [ ] Migrate eligible loaders/repeating effects; select efficient compositor or deadline behavior for
  carets, preserve focus resets and marquee behavior, and stop hidden repetitions.
  - [x] Replace frame-rate caret ticking with shared absolute-time deadline scheduling.
    Settings' Account/Power/Wi-Fi fields and Launcher use one pure blink model, preserving their
    duty cycles and immediate focus/edit resets. Late wakes skip missed cycles; hidden pages,
    minimized/offscreen editors and radio-off forms cause no caret wakes. Settings returns to idle
    after each boundary; Launcher closes retire its clock. Both builds, all 40 tests and owned
    scheduler/command probes pass, including moved windows and four DPI values. Textbox caret
    drawing remains compound; isolated retained-caret benefit/cost comparison and broad acceptance
    remain pending, so this does not close the repetition or final CPU-disposition gates.
  - [x] Isolate ordinary textbox raster sources from surrounding static content.
    Both retained backends partition ungrouped editors without changing explicit group ownership,
    painter order or fallback drawing. The sample caret repaint area falls from 70,400 to 5,249
    pixels, with about 8% more estimated source memory. Adjacent fields, placeholder edits, selection,
    clipping, fractional geometry at four DPI values and complete retirement pass; cached source
    pixels differ from direct drawing by at most one channel value. Both builds, all 40 tests and
    five full native runs per backend plus fallback pass in Debug/Release. Independent caret
    composition, displayed pixels and total product cost remain pending.

## 5. Reveal, resize and layout motion

- [ ] Implement the selected generic resize/reveal capabilities end to end, preserving anchors,
  notches, corners, shadows, monitor constraints and input alignment.
- [ ] Migrate Launcher results, Quick Settings sections, Clipboard sizing and Settings expandable rows,
  including eligible smooth viewport scrolling identified in the ledger.
  - [x] Migrate Quick Settings section reveals, following content and label/chevron crossfades.
    Remove both section scalar tracks and sampled render values. Retained parents share curves;
    individual paint sources preserve overlap and stable control identity. Event-time input includes
    the slide and visible clips; one deadline retires content. Debug builds, 39 tests and five full
    native runs per backend plus fallback pass, including maximum membership and DPI/source-pixel
    checks. Endpoint limits stay strict; intermediate bitmap filtering is explicitly bounded and
    inspected. Popup height/scroll/monitor integration is implemented; final product acceptance remains open.
  - [x] Migrate Quick Settings popup height, scroll clamping and input alignment.
    One canonical layout epoch drives shared fixed-cap chrome, body placement, viewport clipping and
    scroll translation inside a stable session envelope. Remove the height scalar manager, tick,
    accessors, sampled render state and composition frame blocker. Panel source-extent changes
    retarget scroll geometry even when the collapsed target is unchanged; reopening resets intent.
    Generic capsule/native input rectangles sample the same motion without a window-update timer.
    Debug build, all 39 tests, architecture/format/whitespace and five full native runs per backend
    plus fallback pass. Positive scroll/collapse/reversal, source churn, both directions, compact
    monitors and four DPI values are covered; source chrome readback differs by at most one channel
    value. Native collapse/reversal reuses sources with zero raster passes. Animated custom compact
    geometry, displayed compositor pixels and total product acceptance remain in the broader gates.
- [ ] Migrate Top Bar control widths and Dock insertion, removal, reorder and capacity fitting;
  verify existing compositor consumers as well as newly migrated paths.
- [ ] Migrate Dock popup width/reflow and Switcher width fitting using the selected preview design;
  preserve stable source identity, live content, equal heights, variable widths and aligned controls.

## 6. Native responsibilities

- [ ] Complete the selected shared preview path across Dock, Switcher and Stage; preserve relationships,
  simultaneous targets, painter order, source-loss handling and Stage close/landing synchronization.
- [ ] Migrate Stage's remaining eligible movement, backdrop and feedback effects; any CPU preview
  synchronization must sample canonical motion only while needed and avoid unchanged-content redraw.
- [ ] Separate bar visuals from necessary HWND movement/window push, preserve one physical timeline,
  and ensure required native CPU updates stop when movement settles.

## 7. Behavior and resource verification

- [ ] Verify appearance, easing, retargeting, capture, hit regions, focus, activation and game mode,
  including source changes during motion and overlapping preview surfaces.
- [ ] Verify theme/font changes, 100/125/150/200% and mixed DPI, constrained monitors, multiple screen
  resolutions, normal/high refresh and battery saver; record unavailable configurations.
- [ ] Verify resource reuse, bounded memory, hidden-surface retirement, callback cancellation, source
  churn and supported backend recreation on both backends and required fallbacks.
- [ ] Pass isolated Reach/Settings builds, focused non-UI tests, full ctest, architecture, formatting
  and whitespace checks, plus bounded native/live visual verification.

## 8. Final acceptance

- [ ] Publish comparable repeated performance measurements; show no meaningful regression in total
  CPU/GPU cost, latency, power or memory and justify complexity against measured/shared benefit.
- [ ] Confirm eligible compositor motion causes no periodic application layout, rasterization or
  presents solely to advance time; justified CPU/native work is active-only and ends when idle.
- [ ] Resolve every ledger entry; remove obsolete APIs, tracks, timers, sampled render state and
  duplicate execution paths, with no unexplained hybrid or pending capability experiment.
- [ ] Update architecture and migration records with implemented ownership, evidence and justified
  CPU responsibilities; close the goal only when all required acceptance evidence is resolved.

Use reviewable commits following the plan's dependencies. Purely drawn-content consumers may
advance after their shared contract is settled; native-preview consumers depend on the capability
decision. No checklist item authorizes unrelated cleanup or visible behavior changes.

Validation must not terminate a running Reach instance without explicit authorization in the
current instruction; each authorization permits one termination.
