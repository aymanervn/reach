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

## Review status — 2026-10-17

The user resumed the full migration with coordinated parallel implementation and authorized
Reach restarts throughout this goal under the protocol below. Preserve the intentional return to
Explorer: do not reinstall, repair shell registration or restore scheduled tasks. If the existing
folder's reachctl cannot start Reach because registration is absent, record the limitation and
continue independent work. Builds and owned input-transparent diagnostics remain authorized.

**Review needed** marks missing acceptance evidence. It does not close an implementation gate or
turn unimplemented work into review-only work. The [review register](gpu-composition-review.md)
names the evidence required and the current limits. Completed implementation subitems retain their
scoped test results; live/product acceptance remains unchecked.

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
  - [x] Extend fixed-cap chrome with a rigid horizontal focal band and combined edge caps.
    Five columns preserve notch, corners, border and shadow while straight shoulders resize;
    unchanged height requires only five actual groups. Canonical two-slope mapping, same-epoch
    combination and full-range validation reuse the existing bounded compiler and both adapters.
    Shared popup width intent preserves monitor-clamped body/notch placement at four DPI scales,
    either edge, outside anchors, tiny widths and both drop directions. Both builds/all 40 tests
    and five full native runs per backend plus fallback pass. Central/edge notch readback differs
    by at most two channel values; native growth/reversal reuses sources and settled submissions
    change no properties. Consumer integration, overlapping-cap transitions, compact animated
    chrome, displayed native pixels and total product cost remain in the parent gates.
  - [x] Support animated rounded viewport bounds on both native backends.
    Shared canonical clip geometry validates paired epochs and collapsed extents. WUC animates
    geometry offset/size/radii; DC animates matching edges/radii. Reversal reuses sources, settled
    intent changes no properties/tree, and clip disable/repetition retirement stops native bindings.
    Both builds/all 40 tests, full five-run native suites per backend plus fallback and focused
    compact/collapse checks pass. Six widths/four DPI scales match established static viewport
    pixels exactly. Direct rounded-fill edge AA differs by at most 39 and was inspected; consumer
    appearance, compact shadows, complete width fitting and total workload acceptance remain open.
  - [x] Support circular corners across compact animated clip dimensions.
    Shared radius intent couples both axes to the smaller dimension; defaults preserve independent
    clamping. Both backends and CPU masks use the same derivation and reject incompatible epochs.
    Both builds/all 40 tests and focused native reversal/repetition/retirement/reuse checks pass.
    Cached/fallback coverage matches static viewport coverage at six widths/four DPI scales.
    Installed Reach is restarted through reachctl and verified alive. The descriptor adds 1,800 bytes
    per render buffer (0.18%). Consumer appearance, shadows and width fitting remain open.
  - [x] Support an optional canonical radius on retained clip owners.
    A finite or repeating radius driver overrides the scalar, including zero, and clamps against
    animated dimensions without a second clock. Invalid ranges/epochs and unsupported fitted-image
    drivers reject atomically. Both native bindings reuse paint and retire radius-only repetition;
    CPU masks sample canonical scalars directly. Both builds/all 40 tests and 80 owned poses per
    configuration across both backends/four DPI values pass (native maximum one, CPU zero).
    This shared capability does not accept outstanding consumer edge/text appearance or product cost.
  - [x] Preserve width-dependent text layout through retained source parts.
    Shared text/viewport ownership, bounded canonical finite lookup and DirectWrite trim-state
    blocks are implemented on both native backends, with ordinary sampled fallback. Owner-based
    identities survive reorder; static fields use one source; whole-output scissors and font/color
    replacement preserve the width epoch. Source/layer bounds select active-only renderer-owned
    sampling without feature layout/commands in either loop. Pure ownership/clock checks, both
    builds/all forty test groups, full native regressions and owned source/pixel/lifetime matrices
    pass. Final bounded-staging and installed watchdog-first/reachctl verification also pass.
    Consumers, intermediate product appearance and total cost remain open; see the text-width design for evidence/limits.
  - [x] Support canonical animated root scale centers on both native backends.
    Two optional root descriptors preserve moving pivots without per-group storage growth. Generic
    rendering, CPU fallback, native preview projection, host inverse input and native hit testing
    share the center; finite completion and suspension preserve lifetime. Both builds/all forty
    tests, owned motion/input/lifetime/source probes, full native regressions and installed
    validation pass. Launcher consumes the capability; broader product acceptance stays open.
  - [ ] Resolve compact native shadow appearance and total cost before width consumer integration.
    An owned-window prototype renders the existing interior cutout on both compositors. Three widths
    match within two channel values outside a one-pixel geometry edge; edge differences reach 23.
    These are private experiments, not production capabilities or accepted product appearance.
    Arbitrary notch anchors, intermediate frames, failure/recreation, product resource cost and
    consumer checks remain open.
    - [x] Select the bounded single-source shadow graph and verify displayed source motion.
      The [compact shadow design](gpu-compact-shadow-design.md) replaces the three-surface WUC
      experiment with one alpha surface and records the matching DC graph. Forty-eight displayed
      wide/narrow/wide states at four DPI scales and both notch directions follow canonical motion
      with zero application submissions/source repaints. An exact reference explains two cached
      fractional-DPI mismatches. Twelve repeated Release workload phases remove 70 submissions and
      repaints per phase; 276 contained CPU/GPU/power samples support further implementation but
      do not prove final efficiency. Production ownership, failure handling, full chrome appearance,
      large/finite product workloads and consumer integration remain open.
    - [x] Implement the retained shape-shadow contract through both adapters and fallback.
      Neutral groups own bounded shape alpha; adapters retain caster/effect/output separately.
      Parameter updates, output retargets, disable/re-enable and reparenting reuse ordinary sources;
      retirement clears ownership/nominal bytes. Core checks reject invalid ownership/material and
      unsupported paint/nesting. Displayed native motion, four-DPI hardware/software source pixels,
      actual HWND capture and bounded-allocation rejection/recovery pass. Fix HWND double DPI scaling
      and test-local render-buffer stack overflow exposed by descriptor growth. Both builds/all 40
      tests, full native regressions and installed reachctl restart pass. Shared chrome, complete
      width consumers, intermediate product appearance and final total-cost acceptance remain open.
    - [x] Compose shared popup width chrome across rigid and compact ranges.
      `reach_popup_push_width_background` shares canonical body/notch intent, stable source envelopes,
      opaque border/fill semantics and optional whole-output scissoring. Atomic combined preflight
      rejects ownership/ID/capacity errors; disabled shadows create no filter. DirectComposition
      rounded clips select soft border mode, resolving the jagged inherited aliased edges. Both
      builds/all forty tests, four-DPI/two-theme complete-popup matrices, final Debug/Release
      repeated/finite-retarget captures, scissor captures and full renderer regressions pass.
      Same-envelope retargets reuse rasters/tree; motion submits/repaints nothing and retirement
      clears layers/nominal bytes. Flat colors match exactly; remaining antialias edge differences
      are bounded and inspected. See the compact-shadow design for evidence and limits. Complete
      Dock/Switcher consumers, intermediate product appearance and total-cost gates remain open.
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
    retarget continuity, affine sampling and endpoint deadlines. The scalar manager now uses the
    same intent; remaining custom timing and consumer invalidation are tracked separately.
  - [x] Replace scalar-manager delta accumulation with canonical absolute-time intent.
    Every remaining manager consumer samples monotonic time; Settings shares one timestamp across
    its finite families. Retargets preserve event-time continuity, and cached activity delivers a
    final endpoint update after a delayed wake. The old tick and unused crossing APIs are removed.
    Switcher width and Clipboard hover now preserve pre-sample activity for terminal redraw.
    Three pure clock tests, existing deterministic capsule checks and owned caret scheduler probes
    pass in both builds; focused native publication/reversal checks pass with unchanged rasters and
    visual trees, including explicit initialized/reset zero intent. Both builds, all 40 tests and
    five full native runs per backend plus fallback pass in Debug/Release. This does not migrate
    the remaining visual consumers or settle loader/scroll/native choreography timing and product acceptance.
  - [x] Sample initial native property values and finite/repeating curves at one submission time.
    Both adapters reuse the existing scoped timestamp; completion publication restores its scope.
    Owned independent ancestry markers reduce root/global phase drift from 0.008405 to 0.000101,
    with zero application executions/raster/tree/property updates during motion. Both builds,
    all 40 tests per configuration and full native/fallback suites pass. Stage text filtering,
    snapped chrome geometry and complete consumer/product acceptance remain open.
  - [x] Add a finite absolute-time scrolling response and bounded native exponential compilation.
    The shared policy matches the former 60Hz decay, preserves event-time reversal, and ends at one
    exact deadline. Debug/Release builds and all 40 test groups pass; independent dense compilation
    checks cover 5,887,882 poses with at most 29/32 segments and 0.061898px error. The former Euler
    response varied with refresh cadence, so other refresh rates require timing review R10.
    This prerequisite does not complete Clipboard/Settings scrolling consumers or product acceptance.
  - [x] Add bounded same-epoch ratio and quadratic-product motion without another clock or descriptor
    growth. Core retained validation and fixed-cap derivation accept both maps; CPU range, affine
    limits, retarget and crossing queries share their canonical intent. Focused pure checks, dense
    curve comparisons and both builds/all 40 tests pass. Ratio compilation preserves the existing
    precision and 32-piece budget, including bottom-anchor cancellation. Explicit unsupported input
    and compilation-budget rejections remain; this prerequisite does not accept Dock/Stage consumer
    pixels, all eligible combinations, native lifecycle or total product cost.
  - [x] Remove stationary scrollbar-drag frame demand in Clipboard and Settings.
    Pointer events preserve capture, immediate geometry, repaint and release/cancellation. Actual
    sizing/scrolling, worker completion, service notifications and renderer fallback still wake.
    Both builds/all forty tests, architecture/format/whitespace and owned before/after scheduling
    diagnostics pass. Each stationary case changes from 300/300 frame requests to 0/300; this is a
    scheduling check, not a live workload or full scrolling-migration acceptance result.
  - [x] Add shared bounded affine motion and native piecewise compilation for section reveals.
    Preserve delayed fade/early saturation, negative affine mapping, event-time retarget and owner
    deadlines. Both native renderers, fallback, root endpoints/envelopes and fixed-cap normalization
    consume the limits; the old adapter-local compiler is removed. Debug builds, all 39 tests and
    five full native runs per backend plus HWND fallback pass. Quick Settings height/section
    integration is implemented; displayed-pixel/product-performance acceptance remains pending.
  - [x] Support exact same-epoch fitting and moving viewport constraints in canonical motion.
    Bounded multi-knot maps combine active clamps, different breakpoints and moving min/max bounds;
    both compositors and CPU input/fallback reuse the existing compiler and owner. Independent
    fitting and periodic/capacity tests pass. Both builds, all 40 test groups and five full native
    runs per backend plus fallback pass in Debug/Release. Reversal reuses rasters/tree; settled and
    repeated intent submits no properties; hiding retires periodic bindings. Descriptor memory
    cost is recorded in the workload baseline. Complete Dock/Switcher consumer integration,
    compact shadows, title trimming and total product workload acceptance remain open.
- [ ] Separate content/layout invalidation, compositor submission, native placement and input updates
  in both application loops; pure compositor motion must not request periodic application frames.
  - [x] Support event-time disjoint input regions without periodic native-region updates.
    An optional const capsule provider shares one timestamp with root inverse projection in generic
    host input and Win32 nonclient hit testing. Client transforms, two regions and their gap,
    changing extents without publication, empty results, bounded counts and reset cancellation
    pass in owned hidden-window Debug/Release probes. Both builds/all forty tests, architecture,
    formatting and whitespace pass. Top Bar width integration and live/product acceptance remain open.
  - [x] Separate native preview root-presentation sampling from retained content frames.
    Shared runtime demand schedules active DWM projection and one endpoint sample, then returns to
    idle. Unchanged surface contexts skip capsule arrangement, command production, window placement
    and renderer submission; native generation changes reconcile separately. Switcher and Context
    Menu remove root-transition content frame requests. Dirty/environment/anchor/intent/window/
    capability changes invalidate cached contexts; CPU fallback and synchronized-close paths remain
    supported. Both builds, all 40 tests and owned native-frame/capsule checks pass. Five full native
    runs per backend plus existing fallback checks pass in Debug/Release. Width/reflow/body movement,
    wider application-loop separation, live
    alignment and total product workload acceptance remain open.
  - [x] Preserve feature-command sampling when renderer-only sampling overlaps it.
    Per-surface ownership prevents the generic source-only fast path from freezing a finite feature
    fallback, even when another surface overwrites the shared command buffer. Owned nonvisual
    host probes pass in Debug/Release: mixed work rebuilds commands, renderer-only work skips
    layout/commands, endpoint returns to idle, and hide/show/reset retire or republish ownership.
    Both integrated builds, all 40 tests per configuration and architecture checks pass.

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

  - [x] Preserve retained scene topology and content matrices on unchanged submissions.
    Ordered identity/parent ownership is cached separately from raster and motion intent. Native
    trees rebuild on membership/order/reparenting or font replacement; partial rebuilds remain
    invalid until repaired, and suspension resets ownership. The focused two-layer comparison
    removes 300 tree mutations and 100 matrix submissions per 100 unchanged executions on each
    backend. Reorder, source edits, matrix/motion changes, parent-content retirement and recovery
    checks pass. Both builds, all 40 tests, five full native runs per backend plus fallback in
    Debug/Release and final lifecycle-counter probes pass. Displayed pixels, actual device loss
    and repeated total product workload acceptance remain open.

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
  - [x] Migrate Clipboard item, border and close hover using shared retained RGB blending and opacity.
    Item-keyed cached sources preserve the original nested RGB, opaque outer/inner border geometry
    and close alpha. Remove twenty hover scalar tracks, sampled values, tick redraw and hover-only
    frame demand. Surviving source identities follow membership changes; native reversal reuses
    rasters and unchanged settled submissions change no properties or trees. Both builds, all 40
    tests, owned capsule/native lifetime checks and five full native runs per backend plus fallback
    pass. Two themes/four DPI values/five hover values match fallback exactly and cached two-matrix
    coverage within two channel values. Displayed pixels, source memory comparison and total product
    workload acceptance remain in the broader gates.

  - [x] Migrate Settings Power option/custom-field colors through shared retained paint.
    Implementation replaces the scalar manager, sampled Power values and periodic frame predicate
    with canonical 220ms intent, shared selection-item tint, independent textbox ink and one visible
    cleanup deadline. Existing reset-on-selection, placeholder/suffix/highlight/caret and ordinary
    settled paint are preserved. Debug/Release builds/all forty tests and real-loop scheduler checks
    pass. Complete Debug/Release former-producer matrices cover 960 poses across both backends,
    four DPI values, two themes/fonts and three transitions; CPU error is at most one, native glyph contrast
    at most 17 with other/flat pixels within three. The fractional source-phase defect is fixed.
    Five full native runs per backend plus HWND fallback pass in both configurations. No deployment
    or Reach restart was performed.
    **Review needed:** live editing/focus, glyph contrast and source/driver memory/cost; see R9.

  - [x] Migrate Settings Reach-theme and Top Bar style selectors through shared retained selection.
    Canonical position drives cached pill translation and fixed label tint. Shared bounded peak
    mapping preserves the middle label's brightening/dimming during outer-choice jumps; outer
    labels keep clamped affine weights. Remove both scalar managers, sampling APIs and frame demand;
    immediate Windows theme controls reuse constant intent. Both builds, all 40 tests, owned loop/
    producer checks and five full native runs per backend plus fallback pass. Fallback pixels match
    exactly across two themes/four DPI scales/nine positions; cached differences stay at glyph AA
    (at most 17 channel values) and moving rounded edges (48). Other pixels retain the original
    tolerance. Source reuse, interruption and hidden page retirement pass. Displayed native pixels,
    product input and total memory/workload acceptance remain open; Computer Use monitor capture
    failed with `0x80070057` after startup connection was verified.

- [ ] Migrate eligible loaders/repeating effects; select efficient compositor or deadline behavior for
  carets, preserve focus resets and marquee behavior, and stop hidden repetitions.
  - [x] Add canonical periodic properties for retained transforms, opacity, clips and tint.
    Shared motion adds independent endpoint holds, absolute phase, affine/clamped sampling and a
    bounded rotated-cycle compiler. Both adapters use native repetition; unchanged intent does not
    repaint or resubmit properties. Layer retirement, font replacement and suspension stop bindings;
    root presentation rejects infinite motion. Pure sampling/compiler/crossing checks, both builds,
    all 40 tests and focused native lifetime probes pass. Five full native runs per backend plus
    existing fallback checks also pass in Debug/Release. Existing marquee audit, displayed pixels
    and product acceptance remain open; Settings loader consumers are completed below.
  - [x] Migrate Settings Wi-Fi, Bluetooth, Windows Update and installed-app loaders end to end.
    Canonical periodic width/translation drive shared fixed-cap sources and a static rounded viewport
    on both backends. Remove phase state, phase-update APIs and in-flight/scan frame polling. Worker
    and service completion/progress use a coalesced bootstrap wake event; notifications drain before
    result consumption. Unsupported backends and tiny bounds use direct geometry from the same
    motion with explicit sampled demand; minimized rendering suspends sources and offscreen loaders
    publish no repetition. Both isolated builds and all 40 tests pass. Owned probes cover actual
    capability selection, notification races, busy pages in two themes/four DPI values, native idle,
    source reuse/restart/retirement and 136 pixel comparisons. Direct fallback pixels match exactly;
    retained filtering differences stay at rounded caps and are bounded/inspected. Five full native
    runs per backend plus existing fallback checks pass in both builds. Displayed pixels, the tiny-bound
    CPU cost disposition and total product workload acceptance remain in the broader gates.
  - [x] Audit existing Now Playing marquee setup, source reuse and repetition retirement.
    Both native drivers already avoid animation-driven application frames. Fix logical speed and
    overflow threshold scaling so travel duration stays consistent at 100/125/150/200% DPI; account
    native starts/restarts in workload diagnostics. Eighty scoped native cases per build cover both
    backends, two themes, five repetitions, unchanged submissions, title changes, static disable,
    threshold crossing, font replacement and hiding. Existing deterministic checks and both builds
    pass; all 40 tests pass in Debug/Release. Preserve the established WUC Bezier and DC smoothstep
    curves. Canonical curve/phase unification, displayed appearance and total workload/power
    acceptance remain open; this audit does not close the shared-motion or repetition gates.
  - [x] Unify Now Playing marquee curve, phase and lifecycle through canonical repetition.
    The capsule owns absolute motion; both backends reuse shared piecewise compilation. Remove
    legacy cycle fields and adapter timing. Preserve logical overflow/speed/holds and unchanged
    phase; title/extent changes begin a fresh readable hold. Renderer capability keeps unsupported
    execution static without CPU frames. Both builds/all 40 tests, eighty focused native cases
    per build, owned media-service/capsule checks and five full regression runs per backend plus
    fallback pass in Debug/Release. The approved installed-folder restart succeeded and the same
    host/watchdog PIDs remained alive. Screenshot capture still fails with 0x80070057; displayed
    appearance and total product/power acceptance remain in the parent gates.
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
  - [x] Compare owned compound, isolated-deadline and native-step caret execution.
    Five rotated phases per method/backend and 256 offscreen pixel cases pass. Isolation removes
    eleven textbox repaints per phase; native stepping also removes eleven application submissions,
    at 400 extra estimated source bytes. QPC-aligned process/DWM/GPU/package/board measurements have
    valid coverage, but overlapping ranges and the small translucent scene do not establish a total
    product cost difference. The workload baseline records the conditions and limits. Shared caret
    source/intent integration, displayed blink behavior and final CPU disposition remain pending.
  - [x] Support exact discrete repetition through canonical intent and both native compilers.
    Zero travel reuses positive endpoint holds, generation, phase rotation and lifetime cleanup;
    no descriptor fields or application clock are added. Pure checks cover existing caret duty
    cycles at their absolute deadlines, mapped ranges, skipped values, delayed starts and retargets.
    Both builds/all forty tests and focused native reuse/reset/retirement checks pass; existing
    continuous compositor/fallback regressions also pass. This completes the shared timing
    capability, not independent caret sources, consumer integration or product acceptance.
  - [x] Migrate Settings Account/Power/Wi-Fi and Launcher caret sources and scheduling end to end.
    Shared textbox parts resolve a small stroke from the existing cached DWrite layout and inherit
    editor clips/transforms. Canonical opacity preserves focus/edit generation and phase across
    source/font recreation. Supported paths publish no caret update/deadline or periodic application
    submission; unsupported paths retain visible-field deadlines. Both builds/all forty tests,
    previous-renderer pixels, actual retained-source graphs, owner reuse/reset/suspension and real
    consumer producer/scheduler checks pass. Bounded repeated production-path captures remove eleven
    submissions/repaints per phase but do not prove total product cost or displayed timing; those
    remain in the broader acceptance gates.

- [x] Remove periodic frame demand from Battery pending and Top Bar Bluetooth grace policies.
  **Review needed:** real service acknowledgement/radio-enumeration behavior (R4); isolated policy checks pass.
  Preserve their two-second lifetimes using generic absolute deadline wakes. Keep service
  acknowledgement/recovery and reset cancellation, merge Battery's timeout with popup completion,
  and reconcile expiry once. Both builds/all forty tests and owned scheduling/lifetime probes pass;
  pending waits request zero frames or recurring updates. Other semantic scheduling remains audited
  separately in the ledger and broad product acceptance remains open.

## 5. Reveal, resize and layout motion

- [ ] Implement the selected generic resize/reveal capabilities end to end, preserving anchors,
  notches, corners, shadows, monitor constraints and input alignment.
  - [x] Add shared retained image crop/stretch fitting for resize consumers.
    Actual bitmap dimensions drive cached native source geometry; fixed axes/endpoints preserve
    bitmap rounding. Unsupported clocks and resource budgets use existing active-only render-port
    sampling, retiring at endpoint/hide. Both builds/all forty tests and native/fallback, fractional
    origin, zero-axis, replacement and memory matrices pass. Full native regressions pass on both
    backends/configurations. Clipboard width consumes it; height, scrolling and total product acceptance remain open.
- [ ] Migrate Launcher results, Quick Settings sections, Clipboard sizing and Settings expandable rows,
  including eligible smooth viewport scrolling identified in the ledger.
  - [x] Migrate Clipboard item width through retained item/text/image/control fitting.
    **Review needed:** live input/focus/capture and visual edge/memory acceptance; see R3/R5 below in the review register.
    One canonical 160ms ease-out, endpoint layout and event-time input replace the width scalar
    track, accessor and periodic width frame/layout work. Stable source domains preserve custom
    preview variants, visible-fragment border policy and exact-time reversal. One deadline retires
    expanded sources; extremely narrow clipped-glyph geometry uses generic finite content sampling.
    Both builds/all forty tests and normal/maximum-membership native/fallback matrices pass across
    four DPI values and both themes. Moving edge-filter differences and temporary source-memory
    cost are measured in the baseline. Full native/fallback regressions and installed watchdog-first/
    reachctl startup pass. Height, smooth scroll and full product acceptance remain open.
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
  - [x] Migrate Launcher result-height/reveal and overlapping opening-scale integration.
    Canonical height, stationary clipping, per-paint fade/slide and one endpoint cleanup replace
    the scalar manager and periodic result work. Generic root center motion preserves the changing
    zoom pivot in both renderers, fallback, native input and preview projection. Both builds/all
    forty tests and focused native/fallback/error/settled/source matrices pass. Shared pixel-aligned
    constant axes preserve border coverage; intermediate glyph/curve sampling differences are
    scoped and inspected. Full native/fallback regressions and installed watchdog-first/reachctl
    validation pass. Source-memory cost is measured and remains part of total product acceptance.
- [ ] Migrate Top Bar control widths and Dock insertion, removal, reorder and capacity fitting;
  verify existing compositor consumers as well as newly migrated paths.
- [ ] Migrate Dock popup width/reflow and Switcher width fitting using the selected preview design;
  preserve stable source identity, live content, equal heights, variable widths and aligned controls.
  - [x] Migrate Switcher width fitting through retained chrome/cards/text and canonical native destinations.
    Implementation removes the scalar width manager, periodic width tick/frame demand and sampled
    render geometry. Stable source tokens, equal-height aspect fitting, event-time input/native
    rectangles, monitor/DPI cuts and one endpoint cleanup are verified in the real producer.
    Both builds/all forty tests and four-DPI/two-theme owned native matrices pass. Intermediate
    flat colors match exactly; antialias/glyph differences are bounded and inspected. Full renderer
    regressions and installed validation pass. Live preview alignment and total product
    acceptance remain in the parent/broader gates.
  - [x] Complete Dock popup width/card/title fitting through the shared layout and native path.
    Width, reflow, body/notch and fitted cards now share one canonical epoch and stable envelope.
    Scalar width ownership, periodic width work and sampled render rectangles are removed.
    Both builds/all forty tests and focused native source/static pixel/motion/resource matrices
    pass, including maximum membership, paging, exact event-time reversal and close freeze.
    Five full native runs per backend plus fallback pass in both builds; final complete Dock and
    Switcher matrices pass. The installed watchdog-first/reachctl restart is verified alive.
    Live DWM alignment and total product acceptance remain in the parent/broader gates.
  - [x] Migrate Dock preview horizontal reflow through retained motion and shared native destinations.
    Stable window parents translate cached card sources; headers, borders, close controls, fallback
    icons, DWM rectangles and event-time hit testing share canonical offsets. Remove the scalar
    reflow track and sampled layout-position writes; preserve interruption, close-time freeze,
    constraints, source identity and endpoint cleanup. Both builds, all 40 tests and focused native
    checks pass, including destination-only motion with a settled root. Cached SVG differences are
    bounded to antialias edge pixels inside close glyphs; outside and solid interiors retain the
    original tolerance. Five full native runs per backend plus fallback checks pass in Debug/Release.
    Popup width/card sizing,
    live alignment and total product workload acceptance remain in the parent/broader gates.

## 6. Native responsibilities

- [ ] Complete the selected shared preview path across Dock, Switcher and Stage; preserve relationships,
  simultaneous targets, painter order, source-loss handling and Stage close/landing synchronization.
- [ ] Migrate Stage's remaining eligible movement, backdrop and feedback effects; any CPU preview
  synchronization must sample canonical motion only while needed and avoid unchanged-content redraw.
- [ ] Separate bar visuals from necessary HWND movement/window push, preserve one physical timeline,
  and ensure required native CPU updates stop when movement settles.
  - [x] Publish the final position through the generic native-only bar path after the position
    manager settles. A fresh Debug/Release actual-host probe passes 56 checks per build: five active
    samples and one terminal bounds/push callback produce no arrange, command build or execution;
    the next idle sample does no work. Game return, reset and constraint cuts use the existing
    Dock/Top Bar owners. Broader bar visual migration, installed interaction and total-cost gates
    remain open.

  - [x] Project native-only bar pointer events and popup-owner guards into the capsule's stable
    shown geometry. One generic helper also owns retained-root inverse translation/scale; no
    feature-specific host branch is added. Actual-host Debug/Release checks pass 1,760 cases each,
    including reveal/return, capture/rearm/release, shadow padding and transformed popup guards.
    Both builds/all 40 tests pass. Moving popup anchors and broader live/product acceptance remain open.

## 7. Behavior and resource verification

- [ ] **Review needed:** Verify appearance, easing, retargeting, capture, hit regions, focus, activation and game mode,
  including source changes during motion and overlapping preview surfaces.
- [ ] **Review needed:** Verify theme/font changes, 100/125/150/200% and mixed DPI, constrained monitors, multiple screen
  resolutions, normal/high refresh and battery saver; record unavailable configurations.
- [ ] **Review needed:** Verify resource reuse, bounded memory, hidden-surface retirement, callback cancellation, source
  churn and supported backend recreation on both backends and required fallbacks.
- [ ] Pass isolated Reach/Settings builds, focused non-UI tests, full ctest, architecture, formatting
  and whitespace checks, plus bounded native/live visual verification.

  - [x] Make existing asynchronous worker verification independent of scheduling order.
    `7b85e26` records the path-probe thread separately and verifies identity after completion,
    rather than assuming that the worker has not started immediately after scheduling. Shell and
    path probes still require one execution on the worker. Twenty repeated focused runs in each
    configuration and both full 40-test suites pass; service behavior is unchanged.

## 8. Final acceptance

- [ ] **Review needed:** Publish comparable repeated performance measurements; show no meaningful regression in total
  CPU/GPU cost, latency, power or memory and justify complexity against measured/shared benefit.
- [ ] **Review needed:** Confirm eligible compositor motion causes no periodic application layout, rasterization or
  presents solely to advance time; justified CPU/native work is active-only and ends when idle.
- [ ] Resolve every ledger entry; remove obsolete APIs, tracks, timers, sampled render state and
  duplicate execution paths, with no unexplained hybrid or pending capability experiment.
- [ ] **Review needed:** Update architecture and migration records with implemented ownership, evidence and justified
  CPU responsibilities; close the goal only when all required acceptance evidence is resolved.

Use reviewable commits following the plan's dependencies. Purely drawn-content consumers may
advance after their shared contract is settled; native-preview consumers depend on the capability
decision. No checklist item authorizes unrelated cleanup or visible behavior changes.

Current GPU goal exception: the user explicitly authorizes restarts throughout this goal.
The user reaffirmed this exception on 2026-10-17, superseding the ordinary AGENTS.md one-termination
restriction and earlier stop-only continuation notes. No further restart confirmation is needed.
Always stop `reachWatchdog.exe` first and wait for its exit, then stop Reach and wait for its exit.
Enumerate fresh processes for each restart. Reuse `C:\Dev\reach\build` and start only through that
folder's `reachctl --start`, with that folder as the working directory. Never reinstall or launch
Reach/watchdog directly, and never repair shell/task registration. Verify actual process lifetime,
executable paths and deployed binary hashes before treating startup as successful. A missing
registration is a live-validation limitation, not permission to bypass these constraints.
