# Production GPU/CPU workload migration

Date: 2026-10-11. Status: planned; implementation and performance acceptance pending.

Track delivery with the [GPU composition migration checklist](gpu-composition-migration-checklist.md).

This plan completes the workload balance of the final product. It builds on the retained renderer
introduced in `03f952b`. The earlier [compositor plan](compositor-animation-plan.md) remains the
record of its implemented scope and verification. Its hybrid dispositions are evidence to review
under the approved rule, rather than permanent exemptions. This plan does not authorize unrelated
repository cleanup or changes to visible product behavior.

## Approved rule and final product

Prefer retained, compositor-driven visual animation when appearance and behavior are preserved
without meaningful regressions in total CPU/GPU cost, latency, power, or memory. Small gains justify
migration when implementation is simple or a shared capability benefits multiple consumers. Judge
complexity against the aggregate benefit.

Keep layout decisions, input policy, and necessary native operations on the CPU. Evaluate their
visual animation separately. Every remaining CPU-driven visual animation has an explicit reason,
and each migration removes superseded paths.

| Responsibility | Expected final behavior |
| --- | --- |
| Eligible translation, opacity, scale, feedback, reveal and resize | The compositor advances retained properties; Reach submits intent on change and reconciles completion. |
| Text, icons, chrome, shadows and artwork | GPU-backed rendering produces reusable content on actual invalidation. Motion alone does not regenerate unchanged content. |
| Layout, paging, model, input, activation and gesture policy | CPU capsule logic computes targets and handles events. Sampling motion for input/interruption does not require a permanent frame loop. |
| Live preview pixels | Shared native-preview capability preserves live content, source identity, proportions, eligibility and lifetime. |
| Native geometry that cannot efficiently participate in composition | Explicitly justified CPU synchronization, limited to active work and separated from unnecessary drawing/layout. |
| Holds, completion, carets and repeating effects | Native completion, absolute deadlines or compositor repetition as appropriate; hidden/settled content causes no animation-driven periodic application frames. |

This does not require every calculation to execute on the GPU. There is no GPU readback for ordinary
animation values and no universal bitmap stretch to simulate layout. CPU content/input/native events
remain legitimate work while visual motion runs independently.

## Starting scope

This map comes from docs and consumer searches. Phase 1 completes the per-animation audit.

| Area | Current state and remaining work |
| --- | --- |
| Animation support | Finite scalar tracks now carry canonical `reach_motion` intent and sample absolute event time. Retargeting and retained publication share easing/generation; cached final updates preserve CPU presentation semantics. Remaining visual consumers and direct phase/scroll/native choreography timing are pending. |
| Renderer | Both Windows backends retain X/Y, opacity, uniform or independent axis scale, animated clips, bounded affine curves, tint and marquee movement. Both backends share the cubic/constant segment compiler and retained viewport/content hierarchy with event-time CPU geometry. Empty parent nodes allocate no raster. Fixed-cap chrome preserves source rasters while resizing; feature consumers and remaining small/notched-width capabilities are pending. |
| Runtime | Reach and Settings have retained presentation/deadlines. Remaining predicates can request layout/render frames solely to advance values. |
| Already migrated | Shared popup and Launcher presentation, System HUD, Settings selector, Dock/tray reorder and snap, marquee. Preserve and measure these paths. |
| Shared feedback | Context Menu pill/outline/close and command rows, Battery, tray popup, Clipboard, Quick Settings, Dock/Top Bar and Settings controls. |
| Expansion and resizing | Launcher results, Quick Settings sections, Clipboard, Settings expandable rows, Top Bar widths and Dock slot/capacity fitting. |
| Dock previews and Switcher | CPU width fitting and Dock reflow; DWM placements also follow root presentation through CPU sampling. |
| Stage | Tile movement/reflow/departure, backdrop/close feedback and synchronized close need separate dispositions. |
| Bar reveal/window push | Physical HWND/application-window movement is native CPU work; review visual rendering separately. |
| Timing outside the manager | Settings loader phases and caret timers. Evaluate repetition versus efficient deadlines. |

Shared animation/control primitives, renderer contracts/adapters, generic runtime and many capsules
are affected. Nine main feature areas still have manager consumers, plus Settings. Some consumers
need only local wiring changes; others need shared capabilities. Most services, protocol,
configuration, application identity, search and command policy are outside scope.

## Architecture and behavior constraints

Capsules own identity, intent, target layout, input and semantic actions. Shared feature primitives
provide feedback/reveal/presentation. Neutral render/native-preview contracts describe capabilities;
adapters own native objects/execution. Composition stays generic with no feature-specific animation
branches. Build on retained groups, `reach_motion`, existing completion and native-overlay ownership.
Add only capabilities needed by actual consumers, with one canonical curve/time contract for
compositor execution and justified CPU sampling. Temporary compatibility paths are removed before
migration acceptance, rather than becoming another permanent animation system.

Preserve easing, duration, interruption, painter order, close visibility, capture, identity guards,
game mode and asynchronous activation. Preserve Stage's synchronized close/landing choreography.
Thumbnails remain live, use equal heights and source-dependent widths, and retain placeholders;
Dock nominal height remains 140dp and Switcher 125dp. Theme/DPI/font/resolution, border thickness,
notch, shadow, monitor clamping and input regions stay correct throughout motion. Initial openings
and DPI/display cuts retain existing behavior unless separately approved.

## Migration sequence

### 1. Inventory and baseline

Inventory all manager tracks, retained motion, direct phase/timer updates and recurring visual work
in Reach and Settings, including hidden/idle cases. Maintain a disposition ledger with source symbols,
driver, visual/native consumers, invalidation, frame demand and lifetime. Classify every effect as
already migrated, migrate, efficient CPU/deadline/native work, or capability decision required.
Each CPU disposition records a specific reason and evidence.

Add small opt-in aggregate diagnostics for wakes, ticks, arrange/command production, actual
rasterizations, presents, property commits, native placement calls and retained/cache memory.
Separate content changes from geometry-only work. Reuse previous renderer diagnostics and their
ordering/clipping findings; accepted native commands alone do not establish visual correctness.

Record comparable baseline traces with exact binary/revision, renderer, hardware, DPI, refresh and
power mode. Measure Reach plus DWM GPU cost, cold/warm setup and steady-state CPU, input/presentation
latency and memory. Establish repeat-run variation and meaningful regression thresholds before
comparison. Record unavailable scenarios instead of assigning an invented universal percentage.

**Gate:** complete ledger and repeatable baseline, including the earlier migration's unmeasured
performance. Fewer application draw calls alone are insufficient evidence of efficiency.

### 2. Decide resizing and native-preview capabilities early

Determine the smallest reusable mechanisms for animated clips, section reveal, size-dependent chrome
and content positioning on both Windows backends. Verify when endpoint layout plus retained property
motion reproduces intermediate geometry and when constraints need CPU reconciliation. Preserve text,
icons, corners and thumbnail proportions; motion snapshots and distorted bitmap stretch are not substitutes.

Determine whether live preview content can join the same supported compositor presentation as its
chrome. The current thumbnail port exposes immediate placement, not animation curves. DWM thumbnail
updates alone do not prove independently animated geometry. `CreateSurfaceFromHwnd` wraps layered
window content, not arbitrary thumbnail output. Window capture is a different mechanism and is not
assumed to be an equivalent or cheaper replacement.

Use bounded targeted verification against real adapters/supported Windows versions to decide the
production capability. Assess source lifetime, minimize/unavailable/protected content, simultaneous
targets, z-order, Stage landing/close, synchronization, latency and resource/capture overhead. Avoid
undocumented platform dependencies and changes to foreign-window styles merely to enable composition.

Select a reusable supported capability only if it meets the approved benefit/complexity rule while
preserving behavior. Otherwise select an optimized, explicitly documented hybrid path. That is a
final workload disposition with evidence, not an indefinitely unfinished POC. Resolve concrete
product/API incompatibilities before dependent implementation proceeds.

**Gate:** resize design and preview disposition with capability, limitations, expected cost and
consumer impact. Do not promise fully compositor-driven thumbnails before this gate.

Implemented capability foundation: group clips expose canonical left/top/right/bottom inset motion.
Adapters cache native clips and submit changed descriptors without changing retained content;
fallback uses the same absolute-time rectangle. Supported properties are documented by
[Microsoft's InsetClip reference](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition.insetclip)
and [DirectComposition rectangle-clip edge animation](https://learn.microsoft.com/en-us/windows/win32/directcomp/idcompositionrectangleclip-setright-overloaded).
This preserves unscaled content while changing its viewport. Bounded reversal/retirement and
geometry checks pass; displayed pixels, section reveal integration and chrome resize remain open.
Insets alone cannot preserve moving rounded corners, borders, notches and shadows. Resolve those
through a shared chrome mechanism before migrating width/height consumers; keep this gate pending.

The implemented fixed-cap primitive and its limits are described in the
[resize/reveal design](gpu-resize-reveal-design.md). It reuses cached retained groups with independent
axis scale, supports unchanged-width notches and validates corner/shadow caps before submission.
Complete section/popup migration, small shapes, horizontal notch behavior and displayed product
acceptance still keep the broader capability gate open.

### 3. Unify animation intent and runtime scheduling

Make reusable manager output declarative motion descriptions based on the existing absolute clock,
easing and generation semantics. Bind through render/presentation contracts; arbitrary scalar tracks
do not implicitly identify visuals. CPU sampling uses the same curves for input, retargeting,
justified native synchronization or real model requirements. Explicit sampling times preserve
deterministic testing without another clock.

Distinguish content invalidation, target/layout change, compositor submission, native geometry and
input-region/observation updates. Visual motion requests no repeated arrange/draw work. Native-only
work does not dirty unchanged content. Apply this to both application loops using existing deadlines
and completion delivery. Retarget consistently across submission delays and reject stale completion
generations. Define cancellation, reset, hide, final input/native reconciliation and backend recreation.

Keep shared capability-based fallback for unsupported rendering paths, limited to required work and
active duration; do not duplicate feature execution policy. Give repeating effects explicit fallback
and lifetime dispositions. Land the contract with a complete simple consumer, such as Context Menu
or Battery feedback, deleting its old tick/redraw path in the same change.

**Gate:** both backends run unchanged-content visual motion without periodic application frames;
interruption, completion and fallback use one shared contract.

### 4. Migrate feedback and repeating effects

Use shared retained feedback for hover, close, selection and press visuals. Prefer fixed-content
opacity layers when they reproduce the effect; add color/effect properties only for justified
consumers. Avoid large visual hierarchies for tiny controls or per-feature animation drivers.

Migrate Context Menu pill/outline/close and command feedback, Battery, tray popup, Clipboard, Quick
Settings, Dock/Top Bar and Settings controls. Press/release policy remains CPU-owned. Keep theme
colors and painter order. Move eligible loaders to bounded compositor repetition and remove phase
redraw loops. For carets, choose efficient event-driven deadlines or retained opacity according to
actual cost/complexity; preserve focus and input resets. Preserve existing marquee behavior.

**Gate:** no feedback/repetition redraws unrelated content, no stationary-drag frame demand, no hidden
effect remains active, and migrated consumers have no superseded tracks or timers.

### 5. Migrate reveal, resize and fitting

Implement Phase 2 capabilities in neutral contracts and both adapters. Retain tightly sized content,
animate clip/position and eligible chrome dimensions, and preserve typography/icon proportions.
Use stable native envelopes only where needed; keep shaped input, anchor/notch movement, pointer
transforms and monitor constraints aligned. Bound visual count and memory.

Migrate drawn-content consumers first: Launcher results, Quick Settings sections, Clipboard sizing
and Settings expanded rows. Then migrate Top Bar control widths and Dock insertion/removal/capacity
fitting. Compute target layout on model/constraint changes and animate unaffected item properties.
Preserve intermediate constraints that actually require sampling.

Migrate Dock popup and Switcher width fitting using the chosen native disposition. Target page/source
membership stays stable during motion. Dock reflow remains identity-based and continuously retargets.
Headers, controls, native previews, highlights and hit regions consume consistent presentation.
GPU chrome with CPU native updates is reported as hybrid.

**Gate:** resize/reveal preserves current appearance/input across DPI and resolutions, reuses
unchanged content, and every continuous CPU task has a named remaining responsibility.

### 6. Finish native-preview and bar responsibilities

If Phase 2 selects compositor-native previews, implement the shared port/runtime capability and
migrate Dock, Switcher and Stage end to end. Preserve handles across geometry changes, simultaneous
targets, front-source order, best-effort source loss and Stage's strict synchronized close. Features
never read adapter objects or peer capsules.

If hybrid remains justified, cache target metrics/layout, sample canonical motion only while native
geometry changes, submit changed properties only, and separate placement from drawn-content work.
Settled live DWM pixels continue updating independently. Migrate Stage's eligible backdrop/header/
feedback separately instead of treating the whole capsule as exempt.

Separate bar visual presentation from real HWND movement/window push. Preserve one physical
displacement timeline, input/work-area policy and native alignment. Keep or improve the existing
position-only fast path through shared contracts. Native CPU updates end when movement stops.
Do not alter foreign-window sizing or activation to obtain a nominally GPU-only implementation.

**Gate:** chrome/native/input alignment and activation/close semantics hold; remaining native frame
loops are finite, justified and free of unrelated raster work.

### 7. Complete resource, backend and power behavior

Verify invalidation for theme/font/DPI/assets/source/backend lifetime. Retire hidden groups, stop
repetition, revoke callbacks and release relationships. Reuse resources during ordinary motion and
recreate migrated resources correctly on supported backend lifetime paths. An existing device-recovery
gap that blocks acceptance becomes an explicit prerequisite with a dedicated scoped fix, not a
general renderer rewrite or unrelated cleanup detour.

Keep current retained/cache bounds unless measurements justify a documented revision. Measure
combined memory across Reach, Settings and preview resources as well as the existing 128 MiB
per-renderer limit. Exercise maximum capacity and repeated page/source churn; enforce reclamation.

Compare repeating effects, setup and high-refresh composition in normal power and battery saver.
Reach's frame cap does not establish compositor cadence. Changes to visible motion/timing or power
mode behavior require a separate explicit product decision.

**Gate:** resources stabilize and retire correctly, supported backend recreation works, and measured
power/latency meets the baseline comparison.

### 8. Product acceptance and removal of obsolete paths

Repeat baseline scenarios and publish results, including unavailable cases. Resolve every ledger
entry as already migrated, migrated here, or intentionally CPU/deadline/native-driven with evidence.
No entry remains an unexplained hybrid or pending capability experiment.

Remove scoped obsolete APIs/tracks, sampled render fields, timers, frame predicates and temporary
adapters. Remaining CPU consumers use canonical timing and have explicit responsibilities. Update
`docs/architecture.md` as implementations establish current behavior, and update stale feature plans.
Compilation and visual correctness alone do not close the migration. Missing performance/hardware
evidence remains pending until covered or its support expectation is resolved with the user.

## Commits and dependencies

Use reviewable commits for inventory/baseline, shared animation/runtime with its first consumer,
feedback consumer groups, reveal/resize infrastructure with its first consumer, complete layout
consumer groups, native/bar work, resource/lifecycle fixes, and final cleanup/acceptance. Split large
groups further when needed. Each consumer commit removes its old path and includes relevant checks.

Phase 2 precedes native architecture commitments. Phase 3 enables Phases 4/5; Phase 2 also governs
preview consumers and Phase 6. Drawn-content work can proceed independently of native constraints
once its shared contract is settled. Supporting generic infrastructure is in scope; unrelated findings
remain separate. Cleanup is completed per consumer, not postponed to an indefinite final sweep.

## Validation and final acceptance

Build Reach and Settings in isolated outputs. Run focused existing tests, full `ctest`, architecture,
format and whitespace checks. Adapt timing/geometry coverage to canonical motion; add only a small
number of meaningful non-UI tests for timing, retarget, generations, invalidation or geometry risk.
Do not add UI tests. Use bounded native diagnostics/live checks for actual pixels, painter order,
clip alignment and completion on WUC and DirectComposition, including prior order/slider regressions.

Cover idle/hidden, feedback, moving/stopped drag, repeated retarget/reversal, content changes during
motion, all surface opens/closes, expansion, maximum Dock capacity, source resize/minimize/restore/
close, paging, overlapping Stage/Switcher/Dock and Stage landing. Include capture loss, focus,
activation, game mode, font/theme, reset/hide and backend recreation.
Exercise 100/125/150/200% DPI, offset/mixed-DPI/constrained monitors, 720p/1080p/1440p/4K where
available, normal/high refresh and battery saver. Use at least five comparable runs for key
performance cases, including cold/warm setup and sustained effects, to distinguish baseline noise.

The current goal's 2026-10-17 restart exception supersedes the ordinary one-termination rule.
Follow the checklist's watchdog-first protocol and reuse `C:\Dev\reach\build\reachctl --start`
from that folder. Preserve Explorer; never reinstall, launch Reach/watchdog directly or repair
shell/task registration. Missing registration leaves live validation pending while independent
work continues. Isolated diagnostics do not establish live acceptance of new binaries.

Final acceptance requires:

- Every effect has a GPU/CPU disposition and rationale; no unexamined timing path remains.
- Eligible compositor-only motion causes zero periodic application layout/raster/presents solely
  to advance time between submission and reconciliation. Content/input/native events are counted separately.
- Justified CPU/native sampling performs only active required work, ends when idle, does not redraw
  unchanged chrome, and preserves surviving preview relationships.
- Appearance, proportions, DPI/input, interruption, lifecycle and activation semantics pass.
- Total CPU/GPU cost, latency, power and memory have no meaningful regression. Small gains are
  accepted when complexity is modest or shared aggregate benefit justifies the change.
- Resources remain bounded and retire/recreate correctly; both backends and supported fallbacks pass.
- Unavailable evidence is explicit, scoped obsolete paths are removed, and architecture/docs describe
  the implemented final balance and justified CPU/native responsibilities.

## Platform references

- [Composition architecture](https://learn.microsoft.com/en-us/windows/win32/directcomp/architecture-and-components): independent composition and asynchronous submission.
- [Property animation](https://learn.microsoft.com/en-us/windows/win32/directcomp/animation) and [animated clipping](https://learn.microsoft.com/en-us/windows/win32/directcomp/clipping): supported presentation mechanisms.
- [Best practices](https://learn.microsoft.com/en-us/windows/win32/directcomp/best-practices-for-directcomposition): visual/resource retirement and avoiding unnecessary work.
- [DWM properties](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ns-dwmapi-dwm_thumbnail_properties): immediate placement/opacity contract.
- [CreateSurfaceFromHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondesktopdevice-createsurfacefromhwnd): layered-window wrapper/lifetime constraints.
- [Window capture interop](https://learn.microsoft.com/en-us/windows/win32/api/windows.graphics.capture.interop/nf-windows-graphics-capture-interop-igraphicscaptureiteminterop-createforwindow): capture item creation, not proof of DWM-equivalent behavior or lower cost.
