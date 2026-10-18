# GPU composition migration: workload baseline

Status: counters verified; full product performance baseline and acceptance pending.

## 2026-10-18: Canonical scroll response prerequisite

The shared `reach_scrollbar_motion_retarget` samples the current canonical offset at the input
instant, then publishes a finite normalized exponential-out descriptor. It replaces no consumer by
itself. The response uses rate `-60*log(.7)` and a 0.560734650-second lifetime; jumps within 0.1px
settle immediately. Retargets preserve visible position, invalid input preserves the old descriptor,
and an exhausted native segment budget returns `REACH_NOT_IMPLEMENTED` while leaving a finite
canonical curve available to ordinary sampling. Remaining scroll consumers and their old frame
paths are still implementation work until their own milestones are validated and committed.

The canonical compiler adds adaptive cubic Hermite pieces with an analytical fourth-derivative error
bound, existing map/clamp crossing boundaries, monotone native control points and a 32-segment cap.
No motion descriptor, render group or fixed command-buffer field grows. Existing easing policies
retain their implementations. CPU exponential interpolation uses double intermediate subtraction
so extreme finite endpoints do not overflow before fallback.

Coordinator rerun of the private dense proof checks 1,260 curves and 5,887,882 native polynomial
poses (directions, distances, three durations, late publication, affine, clamp and hinge mappings).
Peak is 29 segments; maximum sample error is 0.061897513px. Stored junction values/times agree;
polynomial endpoint drift is at most 0.000398034px. Tests also cover invalid targets, exact endpoint
retirement, reversal and native capacity fallback. Debug/Release full builds and all forty test
groups pass (`build-current-integrated-{debug,release}.log`,
`ctest-current-integrated-{debug,release}.log`). The checked tree includes uncommitted consumers;
these tests establish shared compatibility, not their native/visual/product acceptance.

At 60Hz a 72px step differs from the former Euler response by at most 0.000442px. The old response
was frame-dependent: relative to that old loop, the new absolute-time curve differs by up to 6.4798px
at 30Hz, 1.6777px at 90Hz, 2.4591px at 120Hz, 2.8333px at 144Hz and 3.5553px at 240Hz for that step.
The finite normalization bias is below 0.1px for a 16k-pixel jump; larger jumps need their own scale
and native-budget disposition. This is a measured 60Hz calibration, not proof of identical behavior
at every prior cadence. Timing review R10, actual consumer/input/source-turnover checks and repeated
total Reach + Settings + DWM cost remain open. No deployment or restart was performed for this
prerequisite.

## 2026-10-17: Event-time disjoint input regions

The generic capsule/runtime/native-window contract can sample multiple input rectangles on an
input event. The provider receives the same monotonic timestamp used for root inverse projection;
the native port also applies the published surface-to-client transform. It carries one callback
and borrowed opaque owner, rather than an array of animated rectangles. Hide clears native
ownership, and existing teardown destroys windows before capsules. No animation wake, timer or
periodic window update is added.

The ignored `input_regions_provider_probe.cpp` exercises the actual host input projection and
Windows adapter using its own hidden window. Debug and optimized Release runs pass transformed
two-region/gap hit tests, extent changes with no new publication, empty provider results,
capacity bounding and reset cancellation. Evidence is in
`input_regions_provider_probe-{debug,release}.log`. These are input-policy/native checks, not
live Top Bar acceptance or visual/performance measurements.

Both integrated product builds and all forty tests pass at this checkpoint
(`build-resume-shared-debug.log`, `build-image-material-tests-debug.log`,
`build-resume-integrated-release.log`, `ctest-resume-integrated-{debug,release}.log`).
Architecture, formatting and whitespace checks pass. The broader tree includes unfinished
Clipboard height, Settings row and Top Bar width work; this capability does not validate or
complete those consumers. No deployment, Reach restart or shell/task change was performed.

## Settings retained segmented selection

Reach theme and Top Bar style now publish canonical position motion. Cached pill coverage translates
and fixed text coverage consumes bounded affine/peak RGBA tint. The middle choice still brightens
then dims during an outer-to-outer jump. Windows system/application theme selection remains immediate.
Both scalar managers, sampled values and selector frame predicates are removed. `reach_motion_peak`
adds a bounded nonmonotonic mapping to the existing clock/compiler; it does not add a driver or timer.

Both isolated builds and all 40 tests pass (`build-debug-selectors.log`, `build-release-selectors.log`,
`test-debug-selectors.log`, `test-release-selectors.log`). Pure checks cover direction/easing,
late submission, affine/clamped mapping, both peak crossings, equal visible endpoints, periodic
holds/phase, enclosing input limits and invalid nesting. Existing preference tests verify targets.
Owned scheduler probes verify actual Settings has no selector-driven native frames while fallback
requests sampled motion (`probe-selectors-capsule-debug.log`, `probe-selectors-capsule-release.log`).

Actual Settings producer probes cover outer jumps, interruption, theme-position change, unchanged
submissions and page retirement on both backends. Retargets have zero raster passes; native motion
advances with no renderer executions. Unchanged settled submissions change neither raster, native
properties nor trees. The Display page now owns three toggles and four selectors within group capacity.

Offscreen comparisons cover both themes, four DPI scales, fractional placement and nine positions.
Fallback single-text tint draws directly with the effective sampled brush, matching original pixels
exactly. Cached text tint differs only at partially covered glyph pixels, by at most 17 channel values;
white-mask recoloring has different edge coverage from DirectWrite's final-color brush. Moving
rounded pill filtering reaches 48 channel values. Other pixels and solid text interiors retain the
strict two-channel tolerance; observed error outside those edge regions is zero. The owned
`selector-compare.png` artifact was inspected. This is bounded source/composition evidence, not a
claim of exact displayed compositor pixels. Logs are `probe-selectors-native-debug.log` and
`probe-selectors-native-release.log`.

Five full native runs per backend plus existing HWND fallback checks pass in both builds
(`probe-selectors-full-debug.log`, `probe-selectors-full-release.log`). Named-field equality includes
the new mapping, and text source snapping excludes animated mapped transforms. The native runtime
record grows from 3,928 to 4,448 bytes; motion descriptors also carry peak metadata. Full product
memory and total workload acceptance remain pending.

Computer Use launched the actual updated Settings app. Its startup requires a sibling Reach host,
so the verified host was restarted from the matching build after stopping the watchdog. Settings
then connected successfully. Monitor capture failed with
`IGraphicsCaptureItemInterop.CreateForMonitor ... 0x80070057`; accessibility exposes only the custom
window, so displayed appearance/input acceptance could not be completed. An optimized validation
copy was attempted from `build/gpu-composition-migration/live-validation`, but its host and watchdog
subsequently exited. A direct Debug-folder host was then launched with `--launch` and confirmed alive
through a bounded five-second startup observation. The matching Debug-folder `reachWatchdog.exe` was
restored. Per the current goal instruction, validation uses the Debug folder and stops the actual
watchdog executable before restarting Reach. Installation registration is unchanged. The relocated
copy's termination cause remains unresolved; it is not a proven code regression.

## Clipboard retained hover and nested RGB tint

Clipboard publishes canonical item hover with stable item-keyed source ids. Shared bordered feedback
uses the existing pixel-aligned opaque outer/inner fills. An optional canonical RGB mixing weight
blends the base animated tint with a fixed target while preserving coverage alpha: WUC evaluates
this in its existing color-brush expression, and DC chains a second color matrix. Close background
and glyph sources use retained opacity. The twenty hover scalar tracks, value getter, sampled colors,
hover tick redraw and hover-only frame demand are removed. Size and scrolling are still pending.

Both isolated builds and all 40 tests pass (`build-debug-clipboard-feedback.log`,
`build-release-clipboard-feedback.log`, `test-debug-clipboard-feedback.log`,
`test-release-clipboard-feedback.log`). Existing pure retained-intent tests cover the nested RGB
product, unchanged alpha, final endpoints, normalized blend preflight and semantic equality.
Owned capsule probes verify terminal hover needs no content redraw in both builds
(`probe-clipboard-feedback-capsule-debug.log`, `probe-clipboard-feedback-capsule-release.log`).

Scoped native probes exercise the actual Clipboard command producer, hover enter/leave/reversal,
unchanged active/settled submissions, item removal and source retirement on WUC and DC. Hover and
reversal have zero raster passes; unchanged settled submissions have zero raster, property and tree
updates. A native hold advances without renderer executions and hover alone requests no capsule
frames. Periodic mixed-tint probes also verify unchanged submission reuse and font/hide binding
retirement. Item removal preserves surviving source ids; membership changes retire outgoing sources and
fade surviving hover from its current canonical value. These are bounded owned surfaces, not a full
Reach workload measurement.

Offscreen comparisons cover two themes, 100/125/150/200% DPI, fractional input placement and five
hover values. Direct fallback matches the former color/geometry formula exactly. Reconstructed
cached coverage with the actual two-matrix blend differs by at most two channel values, preserving
the existing tolerance. The rounded layered experiment was removed because it altered coverage;
no rendering tolerance was increased. Logs are `probe-clipboard-feedback-native-debug.log` and
`probe-clipboard-feedback-native-release.log`.

Five full native runs per backend plus existing HWND fallback checks pass in both builds
(`probe-clipboard-feedback-full-debug.log`, `probe-clipboard-feedback-full-release.log`). Architecture,
formatting and whitespace checks pass. The existing Reach process remains running. Displayed native
pixels, source memory comparison and repeated total CPU/GPU/power/latency acceptance remain open.

## Dock preview reflow and native body intent

Horizontal card reflow now publishes canonical progress and a clamped translation for each stable
window. Retained parents move card sources without resampling their geometry. The shared native item
contract accepts a destination `reach_rect_motion`; runtime samples it and root presentation at one
timestamp, projects into client coordinates and schedules only active motion plus its endpoint.
Input samples the card offset at event time. Source changes retarget from the current survivor
position, close freezes current offsets, and a deadline retires origins. The obsolete scalar reflow
track and per-frame layout-position writes are removed. Width/card sizing and title fitting remain
on the pending resize path, so these results do not claim the complete popup is frame-independent.

Existing deterministic capsule checks verify removal, interrupted additions, source generation,
settling, close freeze, reopening and DPI cuts. Owned WUC/DC checks exercise the real card renderer:
reflow alone causes no capsule frame demand or tick invalidation, and ten unchanged submissions
repaint/resubmit/rebuild nothing. A separate real-renderer/DWM check drives destination-only body
motion with a settled root and verifies zero additional arrangement/commands/raster/property work,
shared client-space endpoint sampling and terminal retirement. Both builds and all 40 tests pass.

Separating the close SVG into its own cache source changes antialias coverage at edge pixels. A
large diagnostic canvas eliminates the difference; source-local reconstruction matches the
production localization path and exposes bounded coverage variation. The enlarged cached/reference
glyphs were inspected. The comparison requires differences greater than two channel values to lie
inside a close glyph at a partially covered reference edge; all other pixels, including solid
interiors, retain the two-value limit. Edge errors stay within 32/255 across five feedback values
and 100/125/150/200% DPI. No shared SVG rendering change or larger production canvas was introduced.

Evidence is `build-{debug,release}-preview-reflow.log`, `test-{debug,release}-preview-reflow.log`,
`probe-preview-reflow-svg-edges-debug.log` and `probe-preview-reflow-focused-release.log` under
`build/gpu-composition-migration`. Five full native runs per backend plus fallback checks pass
in Debug/Release (`probe-preview-reflow-{debug,release}-complete.log`). Architecture, formatting
and whitespace checks pass. Displayed
appearance/alignment, complete width consumers and repeated total CPU/GPU/power/memory acceptance
remain open.

## Native root-presentation frame separation

Switcher and Context Menu no longer request content frames solely to advance preview-linked popup
translation or opacity. Generic runtime native-motion demand samples the existing canonical root
properties while a visible relationship needs synchronization, including one final endpoint sample.
This remains necessary CPU work for DWM's immediate placement API; source pixels stay DWM-owned.
Native motion does not independently dirty content. Unchanged arrangement inputs and canonical
geometry reuse the prior projection context, so other surfaces also skip unnecessary arrangement
and rendering on these wakes. Native overlay generation changes can reconcile without a content
frame. Renderer capability, HWND/window identity, environment, anchors and changed intent invalidate
the cache. Existing dirty flags, CPU/fallback animation demand and synchronized-close behavior retain
their full path. A bounds-only HWND change does not by itself force a content redraw.

The owned native check calls the actual registered-surface frame function with real WUC/DC renderers
and a real DWM adapter over its own hidden source/target windows. Twenty motion samples and the
terminal sample request native placement without additional capsule arrangements, commands,
window placement, raster passes or property submissions. Ten settled calls issue no native placement
requests. Canonical retarget, DPI/theme/anchor changes, a simulated capability loss/restore,
hide/show and native membership changes exercise invalidation and retirement. Actual Switcher tick
verification confirms root motion itself requests no content frame. These operations do not contact
a product instance or Reach IPC, and do not verify displayed alignment or total system cost.

Both isolated builds, all 40 test groups and focused checks pass. Evidence is
`build-{debug,release}-native-frames.log`, `test-{debug,release}-native-frames.log`,
`probe-native-frames-{debug,release}.log` and
`probe-native-frames-capsule-{debug,release}.log` under `build/gpu-composition-migration`.
The private runtime currently occupies 3,928 bytes per surface, including existing overlay arrays
and the new context snapshots. Five full native runs per backend plus existing fallback checks pass
in both builds (`probe-native-frames-{debug,release}-complete.log`). Architecture, formatting and
whitespace checks pass.
Switcher width, Dock reflow, Stage body movement, broader loop separation, live pixels and repeated
CPU/GPU/power/memory acceptance remain pending.

## Canonical periodic properties

`reach_motion` now expresses periodic forward/backward travel with independent endpoint holds and
an absolute epoch. Affine and clamped mappings keep that epoch. A bounded compiler rotates one full
cycle to the sampled phase, preserving initial values and a continuous wrap. Windows Composition
repeats the keyframes; DirectComposition repeats the cubics. Related periodic properties in one
submission share a sampled time. Finite properties retain their existing timing path, and root
presentation rejects repetition because its completion protocol remains finite.

Pure checks compare compiled polynomials with CPU sampling across all easing values, asymmetric
holds, clipping, interrupted cycles and delayed wakes. They also cover affine mapping, next crossings,
cancellation and invalid timing. Named-field equality includes repetition and holds; padding probes
are updated for the extended value object. Both builds and all 40 tests pass.

The owned native probe publishes repeating X/scale/opacity, clip and tint properties. Holding them
requests no application submissions. Ten identical submissions produce no raster, property or tree
updates; changing one repeating component submits one helper without repainting. Font replacement
and hiding stop every binding; WUC queries verify either no animation or a closed retired visual.
The same layer cleanup is used on suspension. Source byte accounting retires with the layer table.
Evidence is `probe-periodic-focused-{debug,release}.log`, `build-{debug,release}-repetition.log` and
`test-{debug,release}-repetition.log` under `build/gpu-composition-migration`. Full native suites pass
five runs per backend plus existing fallback checks in Debug/Release
(`probe-periodic-{debug,release}-complete.log`). Architecture, formatting and whitespace checks pass.

The lifetime query follows the [AnimationController contract](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition.compositionobject.trygetanimationcontroller).
DirectComposition repetition uses the existing native repeat primitive; its timing model is described
in [Microsoft's animation documentation](https://learn.microsoft.com/en-us/windows/win32/api/dcompanimation/nf-dcompanimation-idcompositionanimation-setabsolutebegintime).
These checks establish property/lifetime behavior, not displayed pixels or total CPU/GPU/power/memory
benefit. Existing marquee behavior is preserved pending its separate audit. Loader consumers now use
this capability as described below.

## Settings loader migration

All four Settings loader families now publish canonical periodic width and translation instead of
advancing phase state each application frame. The shared renderer keeps rounded caps at their
original size, stretches only the middle and uses a static rounded viewport. Both native backends
retain two empty parents and three raster sources. Unsupported tiny bounds keep sampled clipped
geometry explicitly: shrinking fixed caps would distort the original appearance, and adding a second
shape renderer for an exceptional tiny range would add complexity without a demonstrated benefit.
Without retained backend support, the helper also uses direct geometry from the same motion; this
avoids vector stretch seams at fractional patch boundaries. These fallback pixels match the original
clipped geometry exactly in all 136 cases. The final CPU-disposition and total-workload gates remain open.

Owned native checks verify active repeat bindings, no rendering during a hold, no raster/property/tree
work on unchanged submission, a fresh restart generation without repainting, complete retirement,
offscreen omission and the tiny-bound fallback. Source reconstruction compares 17 phases at four DPI
values on both devices (136 cases). Integral-size cycle endpoints match exactly; fractional filtering
differences are confined to rounded-cap neighborhoods, with straight sections within two channel
values. The largest cap difference is 50/255 and whole-image mean error is at most 0.007828/255.
The enlarged retained/reference cap images were inspected. The initial retained implementation had
truncated the rounded mask's antialiased edge with an aliased rectangular bound; ancestor rounded
masks and hard rectangular clips now have separate execution paths.

Settings pending work no longer drives frame polling. A bootstrap-owned auto-reset event receives
coalesced service/worker notifications after data publication. Draining notification intent before
consuming data avoids a lost-wake race; progress wakes respect an existing frame deadline. Owned
checks cover notification-before-wait, notification-during-wait, coalescing, progress payload,
completion consumption, minimized fallback demand and all four busy pages in both themes at
100/125/150/200% DPI. They use the actual Settings implementation and Windows frame wait, without
starting a product instance or contacting Reach IPC.

Both isolated builds, all 40 test groups and scheduler probes pass. Evidence is
`build-{debug,release}-loader-gpu.log`, `test-{debug,release}-loader-gpu.log`,
`probe-loader-wake-{debug,release}.log` and `probe-loader-focused-debug.log` under
`build/gpu-composition-migration`. Five full native runs per backend plus existing fallback checks
pass in Debug/Release (`probe-loader-{debug,release}-complete.log`). Each suite includes ten native
loader lifecycle checks and all 136 retained/fallback pixel cases. Architecture, formatting and
whitespace checks pass.
These results establish implementation, source appearance and scheduling; displayed compositor
pixels, complete live interaction and repeated total CPU/GPU/power/memory comparisons remain pending.

## Existing marquee audit

The Now Playing title already uses one cached clipped source and native repetition, without a
feature frame predicate or CPU phase advancement. Windows Composition uses its existing custom
Bezier `(0.645, 0.045), (0.355, 1)`; DirectComposition uses its existing smoothstep cubics. Replacing
both with the shared motion curve would change the Windows Composition path, so this audit preserves
both established curves and leaves canonical curve/phase unification open.

The producer previously scaled overflow with DPI but kept speed and the overflow threshold in
physical pixels. It now scales all three together: a 52-logical-pixel overflow travels for two seconds
at 100/125/150/200% DPI, with the original 1.6-second holds at each endpoint. A sub-one-logical-pixel
overflow stays static at each scale. Diagnostic property submissions now also include native marquee
starts/restarts, which the old helper omitted; old start counts are not directly comparable.

Owned probes execute 80 cases per build: both backends, both themes, four DPI values and five runs.
During holds there are no application renders. Ten unchanged submissions per case cause zero
raster, property or tree updates; a title edit repaints once and records one native restart. Disabling
retains the static source without repainting or motion demand. Font replacement and hide retire
source ownership and native bindings; WUC controller queries verify binding lifetime. Evidence is
`probe-marquee-{debug,release}.log`, `build-{debug,release}-marquee-audit.log` and
`test-{debug,release}-marquee-audit.log` under `build/gpu-composition-migration`.
Both builds, all 40 test groups, architecture, formatting and whitespace checks pass. These scoped
checks establish the current lifecycle and DPI policy; live appearance, canonical unification and
repeated total CPU/GPU/power/memory acceptance remain pending.

## Canonical scalar-manager timing

Every finite scalar track now stores `reach_motion`; easing, event origin, generation and endpoints
share the retained renderer's contract. All production manager callers use explicit monotonic time
instead of frame-delta accumulation. Settings samples its five remaining finite families at one
update timestamp. Cached value/activity remain necessary CPU presentation state until each visual
consumer migrates: activity retires only when a sample delivers the final endpoint. Retargeting uses
the curve at the input time, even when the last rendered value is older. Equal targets and immediate
sets do not keep frame demand active; reset retires ownership. The obsolete delta tick and unused
track-relative crossing API are removed.
Initialization and reset publish an explicit zero-valued generation; an absent native property must
not substitute its identity default for a manager's numeric zero.

Three pure temporal checks cover shared time/easing, late wakes, event-time reversal, published
intent, immediate transitions, cancellation, reset and invalid samples. Four existing capsule test
drivers advance a test-linked clock rather than synthetic frame deltas; the production clock remains
an independent monotonic implementation with no mutable override. Multiple capsules in one test
frame share one time. A close-position test now inspects previews during the close, before retirement.
Both isolated builds and all 40 tests pass. Owned Settings/Launcher caret scheduler probes also pass.
The owned callback probe also checks delayed terminal samples in Switcher width and Clipboard hover.
Both previously tested only post-sample activity when deciding redraw; their pre-sample activity now
requests the endpoint frame even when the first wake arrives after the duration has elapsed.

Focused native probes publish the manager's descriptor into one retained translation/opacity group,
reverse it between CPU samples and hold without application frame submissions. Each backend records
one reversal execution, one property-helper submission, zero raster passes and zero visual-tree
mutations. A delayed CPU sample then delivers its endpoint and clears pending activity. Evidence is
`probe-canonical-manager-{debug,release}-focused.log`, `probe-caret-canonical-manager-{debug,release}.log`,
`build-{debug,release}-canonical-manager*.log` and `test-{debug,release}-canonical-manager.log` under
`build/gpu-composition-migration`. Both full native suites pass five runs per backend plus fallback
(`probe-canonical-manager-{debug,release}-complete.log`). Fresh zero intent also passes a native WUC
opacity query. Full builds/tests, architecture, formatting and whitespace checks pass. The Stage
feedback fixture now waits for actual opening settlement instead of using a synthetic delta.

One Release test run exposed an existing scheduling-dependent assertion in asynchronous path
verification. `7b85e26` records the probe thread separately and checks it after completion, preserving
the worker requirement without assuming start order. Twenty repeated focused runs per configuration
and both final full test suites pass. This is a separate test-only commit; service behavior is unchanged.

This unifies timing and enables retained publication; it does not make existing scalar-rendered
visuals GPU animations automatically. Their content/frame predicates, loaders, scrolling and native
close synchronization remain explicit migration work. Broad displayed-pixel and repeated product
workload acceptance remain open.

## Caret deadline scheduling

Settings previously requested frames whenever any edit field retained focus, then advanced three
independent caret phases from frame deltas. Shared `reach_blink` now owns nominal visible/hidden
intervals and an absolute next boundary on the same monotonic clock as presentation motion.
Power/Account retain 0.55/0.45 seconds; Wi-Fi and Launcher retain 0.53-second intervals. Typing,
edit keys and focus reset visibility immediately. Late wakes sample the current phase in constant
time and schedule the next boundary without catch-up bursts. Blur/page changes stop inactive
clocks; closing or resetting a hidden Launcher stops its clock.

Settings' idle wait takes the earliest existing monitor deadline or visible caret boundary.
The frame predicate tests a due boundary instead of field focus. Its client-coordinate field
checks exclude offscreen fields, minimized windows and Wi-Fi forms hidden by radio state. The
visible clock is sampled after layout refresh before rendering an event-driven update, so font,
DPI, bounds and scrolling changes do not need a caret-only layout loop.

The owned `caret_scheduler_probe.cpp` includes the actual Settings scheduling implementation with
inert worker/service state and an owned window-state stub. It never starts Settings, touches Reach
IPC or changes the running instance. Real Windows frame waits produce two caret wakes per cycle
for each field family, followed by idle between boundaries. The Debug capture measured about
1.012, 1.001 and 1.067 seconds for the Account, Power and Wi-Fi cycles; delivery includes ordinary
Windows timeout quantization. Focus/input reset, correct textbox command visibility, page return,
blur, offscreen fields, minimization, radio-off forms, moved-window client coordinates and four DPI
scales pass. An owned Launcher capsule verifies render snapshots, edit resets and close retirement.
Three pure temporal checks cover duty boundaries, delayed wakes, reset/stop and invalid or
unrepresentable deadlines. Debug/Release builds and all 40 tests pass.

Evidence under `build/gpu-composition-migration`: `build-{debug,release}-carets.log`,
`test-{debug,release}-carets.log`, `probe-caret-scheduler-{debug,release}.log` and the owned probe
launchers. This verifies scheduling and command visibility, not live focus/input or total product
power/latency. The existing textbox remains a compound raster source; independent retained caret
opacity needs a reusable caret-source contract. Measure per-blink command/raster cost and the
alternative's resource/composition overhead before settling the final CPU/GPU disposition. This
step removes the continuous wake path and provides shared fallback timing; it does not close the
repetition or final performance acceptance gates.

## Retained scene topology and content matrix

Both adapters previously cleared and reattached the scene on every execution, independently of
group property/content deduplication. The owned two-layer comparison records 300 retained-tree
mutations and 100 scene matrix submissions across 100 unchanged executions on each backend.
Ordered group/parent ownership and named content-matrix fields are now cached. The same comparison
records zero tree mutations, matrix submissions, raster passes or group-property updates.
No additional topology vector is allocated for an unchanged submission.

Raster edits and canonical motion retargets preserve native ownership. Matrix changes submit one
matrix without changing sources or topology; reorder changes native attachment order without
rasterization. Font replacement invalidates visual identity even when IDs are unchanged. Empty
scenes remain cached; suspend/recreation clears the cache. Cache validity is withheld until every
attachment succeeds, and a deliberately rejected oversized source during rebuilding is repaired
by the next valid submission. Existing hierarchy probes also check reparenting and parent-source
introduction/retirement without unnecessary tree work. Windows Composition's actual collection is
enumerated to verify painter order and replacement identity.

`visual_tree_mutations` counts instrumented retained-tree attachment/removal attempts separately
from group properties; `scene_transform_submissions` counts content matrix writes. These are native
API observations, not GPU frame counts. The focused comparison passes on both compositors in
`probe-topology-before.log`, `probe-topology-after.log` and `probe-topology-focused.log` under
`build/gpu-composition-migration`. Both isolated builds and all 40 tests pass; full native suites
pass five runs per backend plus fallback in Debug and Release (`probe-topology-{debug,release}-complete.log`).
Final scoped probes verify the added teardown instrumentation and all topology scenarios on the
final build (`probe-topology-{debug,release}-counters.log`). Architecture, formatting and whitespace
checks pass. The existing Reach instance is unchanged.
Displayed DirectComposition pixels, actual device loss and repeated total product workload
acceptance remain open; reduced native submissions alone do not prove those gates.

## Isolated textbox sources

The retained adapter now partitions each ungrouped `TEXTBOX` into its own source and preserves
static runs before and after it. Explicit render groups keep their declared ownership. This applies
to ordinary Settings and Launcher editor commands without feature-specific rendering or a new clock.
Fallback command execution is unchanged. Carets still draw inside the compound editor source.

The bounded native scene has a full background, fractional editor with selection/caret and an
overlapping foreground. Previously each blink repainted its 70,400-pixel combined source; now it
repaints 5,249 pixels while the surrounding sources remain unchanged. Estimated source bytes rise
from 281,600 to 304,296, about 8%, because overlapping content occupies separate surfaces. Twelve
flips produce twelve raster passes and no group-property updates on each compositor. Batch timings
vary, especially on DirectComposition, and do not establish a total performance improvement.

Offscreen readback reconstructs actual localized retained commands and compares with direct
drawing, requiring a nonempty reference. Maximum channel error is one. Adjacent editors, placeholder
changes, selection, scissoring and fractional placements at 100/125/150/200% DPI also pass. Explicit
two-command editor groups stay intact; hiding retires all sources. Full native suites pass five runs
per backend plus HWND fallback in Debug and Release, alongside both builds and all 40 tests.
Evidence is `probe-textbox-{debug,release}-complete.log`, `probe-textbox-before.log`,
`probe-textbox-after.log`, `build-{debug,release}-textbox-cache.log` and
`test-{debug,release}-textbox-cache.log` under `build/gpu-composition-migration`.

This is source invalidation isolation, not independent GPU caret animation or displayed compositor
capture. Retained scene topology now has a focused unchanged-submission comparison, and the final
caret disposition and repeated product CPU/GPU/power/memory/latency comparisons remain open.

## Settings retained binary toggles

Settings Display/font preferences, top-bar auto-hide, Wi-Fi/Bluetooth radio state, startup entries
and power wait options publish canonical `reach_motion` intent through one shared toggle control.
A stationary parent owns viewport clipping, with separate cached track coverage and knob sources.
RGBA tint and X translation advance natively without scaling the knob or changing source commands.
Both sources use shared pixel-aligned padded bounds. The power wait label alpha follows the same
timeline. Their scalar managers/tracks, advancement, sampled render values and frame demand are
removed; fallback uses Settings' existing published-motion scheduling through the final frame.
Immediate controls, row expansion and segmented selectors retain their separate current behavior.

Seven simultaneous controls start and reverse through their real model actions. Each native
backend submits 28 property changes across the two retargets, with zero scalar advances and zero
retained raster passes. Four repeated settled submissions change no properties. Both Debug and
Release complete five full native runs per backend plus HWND fallback without exclusions or
failures. A separate scope check verifies inherited placement and restoring the containing group
and scissor. Production command construction covers every affected page in both themes and four
DPI scales, including all 64 startup entries within retained-group capacity.

Readback compares the shared static control with retained execution at five positions, four DPI
scales, fractional placement, non-unit theme-like RGBA and full/partial viewport clipping. The
fallback matches exactly. Cached-source GPU composition differs by at most one channel value at
endpoints. Intermediate bitmap filtering reaches 38 at individual edge channels, with maximum
mean 0.054943 over the 220x100 image and at most 314 channels differing by more than two. Every
such difference is within 1.75 pixels of the moving knob's circular antialias boundary; position,
scale, track RGBA and stationary clip match the direct vector reference. The diagnostic enforces
strict endpoints, bounded/localized edge filtering and nonempty reference content. The exported
125% midpoint `toggle-retained.png` and `toggle-reference.png` were inspected. This is offscreen
source composition, not displayed WUC/DC capture or a full product performance comparison.

Evidence under `build/gpu-composition-migration`: `build-{debug,release}-toggles.log`,
`test-{debug,release}-toggles.log`, `probe-toggles-{debug,release}-complete.log`,
`probe-toggles-pixels-inspected.log`, `probe-toggles-parent.log`, and the workload JSONL reports.
All 40 tests pass in serialized Debug/Release runs; architecture/format/whitespace checks pass.
Live theme/font changes, input, displayed compositor pixels and total CPU/GPU/power/memory/latency
acceptance remain pending in the broad checklist gates. The running Reach instance was not changed.
See the [ledger](gpu-animation-ledger.md) and [checklist](gpu-composition-migration-checklist.md).

## Collection contract

`reach_workload_session` enables process-wide aggregate counters only when `REACH_WORKLOAD_REPORT`
contains a nonempty output prefix at startup. Reach and Settings write `<prefix>.reach.json` and
`<prefix>.settings.json` on ordinary exit. Use a unique prefix for each comparable run and an existing
output directory. Disabled diagnostics do not start workers, timers or repeated reporting. An
unsuccessful report write does not alter application behavior; verify the file after collection.

The report records schema, elapsed wall time, counters and retained-surface byte estimates.
Counters are observations of instrumented application/native calls, including attempts that fail;
they are not GPU utilization, completion latency or proof of displayed pixels. Native placement
requests are separate from actual DWM property calls and registration/retirement calls. Backend
execution counts distinguish WUC, DirectComposition and fallback. Property submissions count
group-property helper invocations, not individual properties or GPU frames. Raster passes count
retained drawing-surface draws and fallback whole-target draws; swap-chain presents and explicit
DirectComposition commits remain separate. WUC commits internally and has no equivalent explicit
commit counter. Capsule ticks count the generic Reach runtime; Settings model work is represented
by application updates and the shared scalar/loader/scroll counters.

The retained byte estimate sums successfully rasterized layer width * height * 4 across renderers,
including its observed peak, and returns to zero after retirement. It excludes compositor overhead,
swap chains, text/icon/shadow/artwork caches, capture/native resources and total GPU allocation.
Measure process memory and GPU allocation independently. Live snapshot fields are independent
atomic observations; compare quiescent snapshots or ordinary-exit summaries where consistency matters.

The existing running Reach process is not replaced or terminated for collection. Installed/running
binaries do not gain new counters until explicitly launched with the new build and environment.

## Bounded native verification on 2026-10-11

Source: baseline `a74a3c1` plus workload instrumentation; MSVC 19.51.36256 Debug build in
`build/switcher-previews`. Windows 11, NVIDIA GeForce RTX 4070, driver 32.0.16.1714.
The ignored diagnostic creates hidden, nonactivating windows and real renderer targets independently
of Reach. It does not exercise live product interaction or establish DWM presentation/power cost.

Five independent runs on each backend verified identical operation counts:

| Scenario | Application render executions | Actual retained raster passes | Result on both backends |
| --- | ---: | ---: | --- |
| Cold first submission | 1 | 1 | Draw allocated content; 12,800 estimated retained bytes. |
| 100 unchanged submissions | 100 | 0 | Reused content; no group-property submissions. |
| Submit retained X motion and pump for 150 ms | 1 | 0 | Motion requires no further application render calls. |
| 30 alpha changes baked into commands | 30 | 30 | CPU-sampled content changes cause raster work. |
| 30 width changes baked into geometry | 30 | 30 | Size changes cause raster work; peak estimate 17,600 bytes. |
| Destroy each backend | 0 additional executions | 0 additional raster passes | Estimated retained bytes return to zero. |

Verification source, launcher, JSONL samples and report:
`build/gpu-composition-migration/workload_probe.cpp`, `run_workload_probe.ps1`,
`workload-baseline.jsonl`, and `workload.probe.json`. They are local ignored diagnostics, not added
UI tests. Wall timings are retained in the raw samples but are not product latency measurements;
hidden windows, Debug compilation and the deliberate hold make that inference invalid.

The diagnostic also drives the real Battery capsule's pointer down/cancellation and render
production, with only the popup's opening transition disabled to isolate feedback. Five runs on
each compositor produce two render executions (press and cancellation), zero new raster passes,
two group-property submissions and zero scalar-manager advances after cold content creation.
Capsule frame demand remains false during press, release and after force-close. A separate hidden
HWND render target verifies fallback sampling and both final endpoints: four explicit executions
(the two input submissions and their final samples), four raster passes and no scalar-manager
advances. The command buffer reports active motion only until its deadline.

This verifies the consumer's descriptor/submission path and fallback lifecycle. It does not verify
displayed pixels, live product input, DWM workload, total memory or power. The retained Battery
scene's estimate in the diagnostic is 86,688 bytes; the feedback layer adds a fixed surface whose
cost must be included in the final product comparison. Do not infer zero GPU work from zero raster
passes or use the deliberate message-pump hold as product latency.

Quick Settings uses the same diagnostic method, isolating one system tile and cancellation from
height/expansion and service work. Five runs on each compositor verify that after the first target
submission, cancellation needs one execution/property submission, zero raster passes and zero
scalar-manager advances. An explicit capsule tick during the held press requests neither redraw
nor updates; press/release do not request frames. The HWND fallback executes the same release
descriptor with one raster pass. Initial target introduction may allocate/raster a layer, and target
geometry or clipping changes still update content. Hidden/source/DPI and product cost verification
remain part of the final acceptance matrix.

The same bounded diagnostic now drives the Tray popup capsule with a local synthetic provider.
Five runs per compositor verify cancellation requires one execution/property submission, zero
raster passes and zero scalar advances after the initial target submission. Held feedback and
release request no periodic capsule frames; hiding resets the feedback. The fallback rectangle
path samples the same descriptor with one raster pass on cancellation. The diagnostic uses a
fallback title rather than a real shell icon: real icon tint appearance, DPI/theme/source churn
and total product cost remain pending.

The animated-group-clip diagnostic adds a right-inset motion, reverses it after 30 ms, then submits
ten identical descriptors after completion. Five runs per compositor report two changed-property
submissions, zero raster passes and stable surface allocation; removing clipping releases the native
clip object. Geometry tests sample all four insets at explicit absolute times, including crossed
edges and disabled clipping. These checks verify API acceptance, ownership and canonical sampling;
they do not establish displayed-pixel equivalence or rounded-chrome resize capability.

DirectComposition initialization now uses `DCompositionCreateDevice3` with the desktop-device
interface and Visual2 objects. Five native runs verify its effects-device query and color-matrix
creation, alongside the existing motion/clip/feedback diagnostics. WUC mask-brush creation is also
verified. This establishes supported native objects, not color-animation or displayed-pixel
acceptance; neither object-creation check adds a product animation path.

Settings button diagnostics drive the real model press/cancel intent through the shared UI
renderer. Five runs per compositor at 100%, 125% and 200% geometry scales verify one release
execution, two tint-property submissions, zero raster passes and zero scalar advances after cold
background/text coverage creation. Ten identical post-completion submissions change no tint
properties. Disabling tint releases its brushes/property sets/effects; suspension releases layers.
Root opacity completion, default restoration and redraw after suspension pass on both backends
after removing DirectComposition opacity-effect groups. The HWND fallback samples the descriptor,
reaches its deadline and renders correctly without a retained backend.

An ignored offscreen diagnostic reads GPU-rendered Direct2D color-matrix pixels for four coverage
levels and four tint-alpha values, verifying source RGB replacement and premultiplied alpha within
1.5 byte units. This checks the documented effect semantics used by DirectComposition, not the
final native visual output. A separate WIC target executes the production fallback on overlapping
rectangles and verifies that their aggregate 0.75 coverage is tinted once, with transparent exterior
pixels. Both readback checks pass. Native WUC expression/mask submission succeeds; displayed
compositor pixels, live Settings input, actual DPI/theme changes, and total CPU/GPU/memory cost
remain pending. Geometry scales alone do not establish product DPI acceptance.

Dock and Top Bar diagnostics drive public capsule pointer/cancellation paths and command
production with a local pinned application, no real shell icon, and translucent diagnostic theme
colors. Five runs on each compositor verify Dock trigger and Top Bar Settings-button cancellation
need one execution/property submission, zero raster passes and zero scalar advances. Dock item
cancellation also ends its drag sequence: two property submissions update the item placement and
feedback together, with zero raster passes or scalar advancement. Feedback copies the item X
descriptor, uses flat groups and closes group ownership before returning. Held press/release
request no periodic capsule frames; reset stops feedback. The HWND fallback executes the same
three cases with one raster pass per cancellation. Overlay commands preserve the old rule that
feedback alpha replaces theme alpha. Actual shell-icon/DPI churn, dragging/reordering live pixels
and total product cost still require final acceptance. Existing diagnostics continue to pass.

Top Bar power-hover diagnostics drive public pointer enter/leave and reverse the motion after
60 ms. Five runs per compositor verify two executions and two property submissions, zero raster
passes and zero scalar advances after cold coverage creation. Hover requests no capsule frames;
reset and game-mode suppression stop the motion. The HWND fallback samples three explicitly
submitted frames, including the endpoint, with three raster passes. Displayed pixels, live input,
DPI/theme changes and total product cost remain pending.

Dock preview close-hover diagnostics settle unrelated header/size/presentation motion, then drive
public close enter, exit and immediate re-entry. Five runs per compositor report three executions
and three opacity-property submissions, zero raster passes and zero scalar advances. Held close
hover requests no capsule frames or content updates; force-hide clears it. The HWND fallback
executes four sampled frames, including an explicit endpoint, with four raster passes. The exit
keeps the original immediate baseline appearance; re-entry samples the interrupted descriptor.

During these runs, the independent WUC root-opacity completion check failed in all five runs:
the submitted generation remained current but its scoped-batch callback stayed zero, also after
a 700 ms hold and after explicitly clearing diagnostic presentation state. DirectComposition root
completion and all close-hover checks passed. Earlier bounded runs passed that callback check;
the failure was subsequently isolated to hidden/occluded callback delivery and resolved by
canonical endpoint reconciliation, described below. These observations do not establish native
displayed-pixel or latency acceptance.

Stage close-hover diagnostics settle geometry and tile hover, then enter and reverse the close
feedback through public pointer input. Five runs per compositor verify two executions/four tint
property submissions, zero raster passes and zero scalar advances. Capsule ticks request no work
for this feedback; theme RGB/alpha endpoints, settled pointer input and game-mode retirement pass.
The HWND fallback samples three frames, including the endpoint. This scoped run explicitly skips
the previously failing WUC root-completion check with local diagnostic
`REACH_PROBE_SKIP_WUC_ROOT`; it does not resolve that failure or final product acceptance.

Further isolation of the WUC completion finding measured root submission at about 0.05 ms, before
the 50 ms curve deadline. An independent finite property-set batch on the same compositor also
delivered no callback during a 300 ms hold. At that checkpoint the cause remained unresolved; no production completion
policy was changed based on this hidden-window result. A follow-up 8px visible, nonactivating
diagnostic target reproduced the missing callback, so visibility alone does not explain it.
Agile delegates enqueue and execute on the owned/compositor dispatcher, and an explicit
`RequestCommitAsync` reports completion. These narrow checks rule out those failed hypotheses;
they did not identify the root cause or establish displayed-animation correctness. Moving the
same diagnostic target to an unoccluded topmost position subsequently allowed both the independent
batch and root callback to fire, isolating visibility/occlusion-dependent delivery in this setup.

The production adapter now waits for all canonical root motion deadlines and preserves normal
scoped-batch completion. If a WUC callback remains pending after the deadline, it submits settled
properties once, awaits an explicit asynchronous commit and then synchronizes before reporting
completion. The existing host deadline wakes this path; the commit callback wakes the existing
message loop with weak lifetime ownership. Any root-motion retarget resets completion ownership,
even when opacity identity is unchanged; the new WUC batch aggregates all remaining root curves.

Five full native runs per compositor now pass without excluding WUC completion, alongside all
feedback/clip/readback diagnostics. A separate unoccluded 8px target passes. Native checks cover
unequal root deadlines, no early completion, one endpoint property submission with no render/raster
work, cached repeated readiness, component-only retarget, stale generations, clearing a pending
commit and suspension/recreation. Debug builds and all 39 tests pass. Actual native device/fence
failures, complete live product lifecycle and total performance acceptance remain pending.

Terminal presentation error handling is now explicit in the render port. A pending commit returns
success with completion false; status/query/result failures or cancellation return an error with
completion false and latch that generation without resubmitting work. The host suspends the failed
renderer and permits ordinary capsule close cleanup; a cleared capsule deadline ends completion
queries. Retarget and suspension clear the error state.

`probe-completion-errors-retarget.log` passes five full runs per native backend plus HWND fallback.
The bounded probe injects six asynchronous failure variants per run: Error, Canceled, failed
GetResults, failed status query, failed error-code query and Error with an invalid success code.
Started remains pending; ten repeated terminal queries perform no native queries, property
submissions or render executions. Stale queries preserve current failure; component-only retarget
with unchanged opacity identity and suspension/recreation recover. A real Battery capsule with a
diagnostic renderer confirms pending close retention, one error suspension, ordinary close cleanup
and no hidden completion queries. These are controlled failure-state diagnostics, not actual GPU
device removal or total product performance measurements. Build, 39 tests, architecture, formatting
and whitespace checks pass.

The commit/fence path uses supported
[RequestCommitAsync](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition.compositor.requestcommitasync)
and a single
[IAsyncAction completion handler](https://learn.microsoft.com/en-us/uwp/api/windows.foundation.iasyncaction.completed);
it does not sample visual animation on application frames.

Fixed-cap chrome diagnostics now use `reach_render_push_stretched_chrome` and the independent
axis-scale contract. `probe-stretched-chrome-verified.log` passes five full runs per compositor
and the HWND fallback, including existing feedback/clip/completion/error diagnostics. Two resize
intent changes plus an identical descriptor reuse check produce three native executions, zero
raster passes and 18 property submissions for two-axis plain chrome or 12 for vertical notched
chrome. The identical descriptor changes no properties. Retained source area totals 230,400 bytes
at 100% and 921,600 bytes at 200%, split across nine cropped layers. The fallback checks two sampled
executions/raster passes. These are bounded capability counters, not complete feature measurements.

GPU D2D readback compares sampled patch rendering against directly sized chrome at 100/125/150/200%
DPI. Plain chrome differs by at most one channel level; top/bottom notched chrome matches exactly.
This covers borders, corners and shadows in offscreen sampled execution, not displayed retained
WUC/DC pixels. A 150% notch check initially found a constant scale of approximately 1.0000002 from
affine cancellation. Direct normalized endpoint division now preserves exact unit scale and the
same two-level tolerance passes. Geometry tests cover patch tiling and anchored placement through
motion; native checks cover reversal, unchanged descriptors, minimum/cap/content validation and
independent root-Y completion/retarget. Debug builds and all 39 tests pass. Complete feature resize,
live input and total product acceptance remain pending in the resize/reveal design and checklist.

The counters give a reproducible baseline for content reuse versus sampled visual changes. Full
application wake/layout/native metrics, cold/warm workload cost, total CPU/GPU activity, latency,
power, memory and instrumentation overhead still require product scenarios and system traces.

## Baseline prerequisites still open

- The optimized MSVC build blocker is resolved. Three aggregate resets of the shared wire state
  now clear its object representation explicitly, guarded by trivial/standard-layout assertions.
  Normal `/O2 /Ob2 /DNDEBUG` flags are retained. Isolated Release Reach/Settings/tool builds and all
  39 Release tests pass; Debug builds and all 39 tests also pass. A bounded optimized native protocol
  probe seeds owned storage with nonzero bytes, verifies writer headers/empty payload, publication,
  reader snapshots, disconnect/reset clearing and owned-resource retirement. It uses private named
  mapping/event objects and never touches the running Reach instance.
  Evidence: `build-release-storage.log`, `build-release-tests.log`, `test-release-storage.log`,
  `build-debug-storage.log`, `test-debug-storage.log`, `probe-protocol-storage.log`.
- The optimized compositor deduplication finding is resolved. Shared field-wise motion/group
  equality replaces descriptor byte comparisons throughout retained property and root ownership
  handling. Changing only object padding previously submitted four properties on each backend;
  it now submits none and does no retained raster work. Real generation changes still submit and
  preserve completion ownership. Full optimized and Debug suites each pass five runs per compositor
  plus HWND fallback, including source-pixel, completion/failure, hierarchy and feature checks.
  Evidence: `probe-padding-before.log`, `probe-equality-release.log`, `probe-equality-debug.log`.
  Comparable total workload measurements and live product acceptance remain pending.
- Windows Performance Recorder exposes CPU, GPU and DesktopComposition profiles, but starting
  the combined recording failed with `0xc5585011`, "Failed to enable the policy to profile system
  performance." A subsequent status check confirmed no recording is active. No trace was captured;
  no GPU/power/latency result is inferred from renderer counters.
- A trace analyzer was not found in the available toolkit inventory. Resolve trace capture and
  analysis capability, or a validated equivalent, before closing system-performance acceptance.

The supported recording syntax is documented in
[Microsoft's WPR command-line reference](https://learn.microsoft.com/en-us/windows-hardware/test/wpt/wpr-command-line-options).
For final comparison use the same binaries/configuration, hardware, DPI, refresh/power mode,
scenarios and collection overhead. Establish baseline variation from at least five comparable
runs and regression thresholds before comparing the migration. CPU/GPU totals include DWM and
both relevant application processes. Pending prerequisites do not close the measurement checklist
item; independent capability and implementation work can continue while they are resolved.

## Read-only system workload capture

`tools/measure_gpu_workload.py` uses the Python standard library and owned query handles on Windows.
It samples explicitly selected existing PIDs for a bounded duration, writes a new JSONL file and
closes its handles. It never launches, changes or terminates a measured process, changes performance
policy or starts a trace session. Duplicate labels/PIDs and the collector's own PID are rejected.

Select the current Reach and session DWM PIDs, then run:

```powershell
$reachProcessId = (Get-Process reach).Id
$currentSessionId = (Get-Process -Id $PID).SessionId
$dwmProcessId = (Get-Process dwm | Where-Object SessionId -eq $currentSessionId).Id
python tools/measure_gpu_workload.py --process "reach:$reachProcessId" --process "dwm:$dwmProcessId" --scenario idle-capability --seconds 30 --interval 1 --output build/gpu-composition-migration/idle-capability.jsonl
```

Include Settings with another `--process settings:PID` when that workload is being measured.
Use a new output path for each run. Supply `--revision` only when the running binaries' provenance
is known; the default is `unknown`. Metadata records available binary paths/hashes, OS, CPU count,
observer PID, collection settings, provider errors and unsupported latency measurement.

Process CPU uses creation-identity-checked native CPU-time differences, normalized to all logical
processors. Process V2 counters provide CPU and memory when direct process access is denied,
including DWM on the verified machine. That fallback requires continuous counter identity/lifetime
and preserves the original access error and data source. Missing or reset measurements stay
unavailable; incomplete combined CPU totals are not reported as zero. Private bytes and working
set remain separate. Selected raw PDH values and statuses are retained, using the
[documented wildcard counter array API](https://learn.microsoft.com/en-us/windows/win32/api/pdh/nf-pdh-pdhgetformattedcounterarrayw).

GPU utilization retains adapter/physical-engine identity and sums selected processes only within
the same engine. The per-adapter summary selects the busiest engine; separate engines must not be
added into one GPU percentage because they may operate concurrently or share underlying cores.
See [Microsoft's GPU engine explanation](https://devblogs.microsoft.com/directx/gpus-in-the-task-manager/).
System CPU and system GPU engines provide background-load context. GPU dedicated/shared memory
counters are supplemental and require independent cross-checking: Microsoft documents
[incorrect per-process GPU memory values on affected systems](https://learn.microsoft.com/en-us/troubleshoot/windows-client/performance/gpu-process-memory-counters-report-wrong-value).

The collector verifies EMI version and picowatt-hour units before reading accumulated energy.
Delta energy converts to joules with `3.6e-9`; delta time converts to seconds with `1e-7`.
Package summaries retain separate package channels and never add overlapping core/package energy.
See the [EMI contract](https://learn.microsoft.com/en-us/windows-hardware/drivers/powermeter/energy-meter-interface)
and [measurement timestamp units](https://learn.microsoft.com/en-us/windows/win32/api/emi/ns-emi-emi_channel_measurement_data).
Optional NVML board power converts milliwatts to watts; recent NVIDIA devices return a one-second
average. See [NVML power usage](https://docs.nvidia.com/deploy/archive/R550/nvml-api/group__nvmlDeviceQueries.html).
Package and board readings include the whole device's workload, not just Reach or DWM.

Samples include actual intervals, collection-call duration and separate observer CPU cost. Missed
sampling slots are skipped rather than collected in bursts. Summary statistics describe available
samples, with counts exposing missing coverage; use raw intervals for time-weighted comparisons.
Keep interval, providers and observer overhead comparable across repeated runs. Do not subtract
observer CPU from package/board power or claim unmeasured wakeup, frame-latency or total allocation
results from these counters.

Verification on 2026-10-12: the ten-second `collector-verified-final.jsonl` capture attributes Reach
through native process times and DWM through Process V2, includes both PIDs in GPU attribution,
and reads system CPU, EMI package power and NVML board power without provider errors. Collection
median was 8.04 ms, p95 48.77 ms, and observer CPU averaged 0.107% of machine capacity. Concurrent
build/test activity makes this a capability capture, not an idle baseline or efficiency result.
`collector-unavailable.jsonl` verifies explicit missing-process attribution; bounded CLI checks
verify rejected duplicate/out-of-range PIDs and durations shorter than the requested interval.
Five pure accounting tests and all 40 tests pass in isolated Debug and Release builds; architecture
checks pass. Build/test logs end in `-collector.log` under `build/gpu-composition-migration`.
Repeated controlled product scenarios, variation thresholds, latency and independent GPU-memory
validation remain pending; the full baseline and acceptance checklist items stay open.

## Bounded reveal curve capability

The shared `reach_motion_clamp` and `reach_motion_compile_at` preserve delayed fade/early saturation,
affine limits, reversal and owner deadlines. Both native backends use the same compiler, replacing
the old adapter-local curve generator. Fixed-cap scale normalization, viewport edge conversion,
root endpoint sampling and envelope geometry preserve bounds as well as endpoints.

Evidence: isolated Debug builds and all 39 tests pass (`build-bounded-motion-final.log`,
`test-bounded-motion-final.log`). Pure utility tests compare all three easings, both directions,
intermediate submission times, negative affine mapping and constant clamps; compiled polynomial
samples agree with CPU sampling within 0.000003. Geometry tests cover bounded placement/scales and
exact unit constants at 100/125/150/200% DPI.

The bounded native diagnostic passes five full runs on each compositor plus HWND fallback
(`probe-bounded-motion-final.log`, failures=0). Each bounded-reveal reversal covers opacity,
placement with a viewport-relative base offset, and a derived clip edge for each easing. Two intent
submissions and one unchanged endpoint check produce three executions, zero native raster passes,
four changed property submissions and 12,800 retained bytes. The identical endpoint check does not
resubmit properties. HWND fallback produces three finite sampled raster passes and no native
property submissions. The full suite also rechecks root completion/error handling and four-DPI
chrome readback.

These results verify shared curve execution and content reuse, not displayed compositor pixels,
complete Quick Settings integration or total product efficiency. Those acceptance gates remain open.

## Retained viewport/content hierarchy

`reach_render_group::parent_id` connects existing native container/content visuals. Source geometry
stays in feature coordinates; independent ancestor movement, stationary inner viewports and content
motion compose on the native tree. Shared CPU helpers sample the corresponding geometry, clips and
inherited opacity. Ownership/order validation runs before either execution path changes the tree.
Empty parents allocate no raster. Reparenting reuses leaf pixels, and parent content can acquire or
retire its own surface without destroying children. The old flat-only layer assembly and its `seen`
bookkeeping are removed; one planned hierarchy assembles both flat and nested groups.

Evidence: isolated Debug builds, all 39 tests, architecture/format/whitespace checks pass
(`build-group-hierarchy-accepted.log`, `test-group-hierarchy-accepted.log`). Two focused pure geometry
checks cover independent axes, parent origins, nested clip intersection, inherited opacity and
ambiguous ownership/painter-order rejection. Declaration preflight also protects fixed-cap batch
atomicity when a parent is missing.

Five full bounded native runs per compositor plus HWND fallback pass
(`probe-group-hierarchy-accepted.log`, failures=0). The nested reveal/reversal has two empty parents
and one 80x40 leaf: three render executions, zero native raster passes, eight changed property
submissions and 12,800 retained bytes. The unchanged endpoint check submits no properties. The
fallback samples three finite raster frames with no native submissions. Additional checks reparent
an actively animated child, verify its WUC parent identity, retire the unused middle node, let the
outer parent acquire/retire a raster while preserving its child, and release every node on hide.
Both empty parent surfaces remain absent. The full existing feedback, root-completion/error and
four-DPI chrome diagnostics continue to pass.

This establishes hierarchy execution, source reuse and bounded node lifetime. Complete Quick
Settings paint/layout/input integration, displayed compositor pixels and total product efficiency
remain pending. Native opacity acts on retained surfaces; preserving existing per-command fading
still requires appropriate paint partitioning when commands overlap.

## Quick Settings retained sections

The output-device and app-volume scalar tracks are removed. Canonical section parents own clip,
shared bounded opacity and four-DP slide; an independent parent moves following controls. Leaves
cache each paint separately, preserving overlapping panel/track/fill/thumb alpha. Semantic source
IDs keep unchanged titles and chevrons cached during retirement. Pointer input samples one time and
excludes clipped/transparent rows; cleanup uses one deadline. Height remains a scalar/frame driver.

Current evidence: `build-qs-text-phase-shared.log`, `test-qs-text-phase.log`, and
`probe-qs-sections-final.log`; isolated Debug builds, all 39 tests, architecture/format/whitespace
and five full native runs per backend plus HWND fallback pass with no pixel-comparator exclusion.
The actual maximum-membership capsule uses two reversal/retirement executions, zero native raster
passes, seven property submissions and no section scalar advances/periodic frame demand. Post-
retirement retained bytes are 377,760 at 100% DPI. The new pure batch-preflight test verifies atomic
ownership assignment and pixel-aligned glyph padding. Native WUC sampler getters and both backends'
text metadata confirm source-baseline compensation/nearest sampling for unscaled vertical text.
Scaling, horizontal movement, repeated content and parents with descendants keep linear sampling.

The ignored GPU source-composition comparison uses the committed former renderer as its reference.
Open/closed endpoints at 100/125/150/200% DPI differ by at most one channel value; partially clipped
saturated reveals and geometry-AA-off controls differ by at most two. Intermediate command order,
geometry, dimensions, effective clips and color intent match the former renderer. A fractional-DPI
text sampling mismatch was fixed using cached DirectWrite baseline phase, without another timer,
clock, reraster path or layout loop.

Moving a cached bitmap filters shape edges differently from rerasterizing vector edges at each
position. The mid-fade comparison therefore retains its separate bounded filtering check and manual
inspection; it does not silently broaden endpoint tolerance. At 100% a fallback SVG AA fringe differs
by at most 7 channel values across 12 channels above the strict endpoint tolerance; at 200% a rounded
track/fill edge differs by 3 across 2 channels. At 125%, after text correction, the fractional shape
edge maximum is 34, with mean error 0.036248 across the whole image and 4,046 channels above the
endpoint tolerance. The sampled images were inspected: geometry, content and clipping remain
consistent, with transient edge-filter differences. Intermediate mean channel error must stay below
0.05 and differing channels below 2% of pixel count; geometry/color/clip assertions remain separate
and strict. These checks validate this migration mechanism, not all product configurations.

`qs-retained.png` and `qs-reference.png` are offscreen sampled images, not displayed WUC/DC captures.
Live pixels, other themes/fonts, source/input churn, high refresh and total product cost remain in
final acceptance. Popup height/scroll/monitor/native-input integration now uses the same retained layout epoch.

## Quick Settings popup height, scrolling and input

The height scalar manager, track, tick/accessors, sampled render state and composition frame blocker
are removed. Shared fixed-cap popup chrome, body placement, viewport clipping and scroll translation
use the section layout epoch inside a stable session envelope. Input sampling and native hit regions
consume the same neutral motion rectangle. Source-extent changes retarget scroll bounds during
collapse even when the final popup height is unchanged; reopening clears stale scroll intent.

`probe-qs-height-focused.log` verifies both compositors and HWND fallback at 100/125/150/200% DPI and
both drop directions. Each positive-scroll collapse/reversal sequence has five render submissions:
zero raster passes on WUC and DirectComposition, five ordinary draws on fallback. The same bounded
run checks output-device source changes during collapse, reopening, compact monitors and DPI changes.
Host event conversion and generic native input publication/reset are checked separately from the
owned Win32 window's moving hit regions. New pure geometry checks compare popup motion with existing
constrained placement, including out-of-monitor anchors and independent-axis projection. Debug build,
all 39 tests and architecture checks pass (`build-height-review.log`, `test-height-review.log`).

The shared popup chrome wrapper's GPU source readback compares ordinary retained patches against
existing direct chrome at four DPI values, top/bottom notches and compact constant bodies. Maximum
channel error is one for ordinary sizes and zero for compact static bodies. This is offscreen source
composition, not displayed WUC/DC capture. Animated custom bodies below fixed-cap limits, broad live
visual/resource acceptance and comparable total CPU/GPU/power/memory/latency measurements remain open.

The final native suite (`probe-qs-height-final.log`) passes five full runs on each compositor plus
HWND fallback with `diagnostic failures=0`, including the existing completion/failure, hierarchy,
fixed-cap, bounded-motion and feedback diagnostics. No running Reach instance was terminated.

## Context Menu preview header and border hover

Preview hover publishes one canonical 0.18-second ease-in-out motion to a window-keyed header opacity
layer and border tint layer. Source bounds reuse `reach_render_retained_command_bounds` and retained
command ownership, preserving raster phase and transparent edge padding. Remove the hover scalar
track and sampled accessor; ordinary menu rows retain their immediate behavior. Force-close/reset
and new menu sessions clear hover motion. Width/reflow and native preview synchronization remain
separate migration work.

The previous capability classification followed an intermediate quadratic alpha but missed the final
paint helper: `reach_theme_color_alpha` assigns hover alpha, and `reach_render_push_bordered_background`
forces painted border alpha to one. The visible border RGB is affine in hover progress and reuses the
existing RGBA tint contract. No additional nonlinear color capability is needed for this consumer.
Clipboard's nested border RGB interpolation remains a separate pending capability.

Focused bounded native hover checks verify body/header input, leaving and changed window identity,
shared color/opacity time, idle ticks, retirement and fallback scheduling. Three native submissions
use six property updates and zero raster passes or scalar advances on each compositor. The fallback
uses four ordinary draws. Evidence is in `probe-preview-hover-focused.log`; Debug build, all 39 tests
and architecture checks pass (`build-preview-hover.log`, `test-preview-hover.log`).

GPU readback compares both fallback drawing and cached-source composition with the committed former
renderer at five hover values and 100/125/150/200% DPI, including fractional placement and a theme
hover color with non-unit alpha. Fallback pixels match exactly; cached-source comparisons differ by
at most one channel value, within the strict two-channel tolerance. A fractional-edge mismatch in
`FillOpacityMask` was resolved for contained single filled rectangles by applying their tint directly
in fallback; other coverage groups retain the existing mask path. These are offscreen comparisons,
not displayed WUC/DC capture, and do not close the total product performance/resource gates.

`probe-preview-hover-final.log` passes five full runs per compositor and HWND fallback with zero
diagnostic failures, including existing completion/failure, hierarchy, resize, bounded-motion,
Quick Settings and feedback diagnostics. No running Reach instance was terminated.

## Semantic retained descriptor equality

`reach_motion_equal` and `reach_render_group_equal` compare all named intent fields, not padding.
The renderer uses them for tint channels, clip edges, transforms, group geometry and root completion
ownership. Pure regression checks poison known padding without changing values and verify that
clocks, generations, limits, hierarchy and repetition still distinguish new intent. Both isolated
builds and all 39 tests pass (`build-release-equality.log`, `test-release-equality.log`,
`build-debug-equality.log`, `test-debug-equality.log`).

Before the fix, an optimized padding-only submission produced four native property updates per
backend with zero raster changes. Afterwards it produces zero updates and zero raster changes;
fallback performs one ordinary draw. A new generation still updates native state and completion
ownership without rerasterizing unchanged content. Reconstructed settled chrome also submits no
properties. Dock cancellation without pointer movement now updates only feedback: the diagnostic
checks all named group fields and requires exactly one changed feedback group and one property
submission, rather than counting an unchanged placement upload. Source bytes and actual movement
remain separately checked.

Full native suites pass five runs per backend plus fallback in both optimized and Debug configurations
with no exclusions (`probe-equality-release.log`, `probe-equality-debug.log`). This fixes an optimized
cache correctness/efficiency defect; it does not establish total CPU/GPU/power/latency benefit.


## Horizontal focal chrome and popup width intent

The shared stretch contract now supports five columns around a rigid focal band, or ordinary edge
caps containing the complete notch/shadow footprint. Corners, borders and the focal feature retain
unit scale; straight shoulders resize independently. Source crops partition the original area;
there are at most fifteen groups, or five with an unchanged uncapped height. No new native effect,
clock or backend is introduced. Compact/overlapping independent caps remain explicitly unsupported.

At the focal-band milestone, two-slope mappings generalized the existing peak without enlarging
`reach_motion`. The subsequent compound-fitting extension below records its added storage.
Same-epoch combinations preserve clock/easing/generation and reject active input clamps or different
breakpoints. Full-range validation includes interior extrema. Popup body/notch intent agrees with
existing constrained placement across both directions, outside/edge/center owners, tiny and oversized
widths, reversal and 100/125/150/200% DPI. These pure geometry checks establish the shared contract;
they do not migrate the Dock popup or Switcher consumers.

Debug/Release builds and all forty test groups pass. Owned full native suites run five times per
backend plus fallback (`probe-focal-full-debug.log`, `probe-focal-full-release.log`). Native focal
resize and reversal reuse cached sources, and unchanged settled submissions change no properties.
Central top/bottom and combined left/right cap readback differs from direct geometry by at most two
channel values at all four DPI scales. Displayed native pixels and total CPU/GPU/power/memory/latency
acceptance remain open. Later invalid-input/cap validation changes retain the same valid geometry
and receive focused geometry/native revalidation.

The current sandbox's MSVC PDB server rejected `/Zi` compilation with C1902. The local Debug cache
uses `/Z7 /Ob0 /Od /RTC1` and owned probe runners use `/Z7`; tracked build rules and optimized Release
flags remain unchanged. Tests use a workspace-owned TEMP/TMP directory because the sandbox temp
path could not create nested fixtures. These are validation-environment adjustments.

An earlier validation directly launched the registered executable and watchdog. That did not
follow the required installed-session protocol and does not establish valid startup acceptance.
The corrected `reachctl --install` / `reachctl --start` sequence is recorded below; never repeat
direct launch. No displayed-pixel/input acceptance is claimed.


## Animated rounded viewport bounds

`reach_render_group_clip_motion` derives a rectangle from compatible paired insets, including
nonnegative collapse. Existing WUC rounded geometry animates offset, size and independently bounded
radii; DC animates its existing clip edges/radii. No new descriptor fields, property sets, sources,
clock or application frame loop are added. Reversal reuses rasters; settled submissions mutate no
properties/tree; periodic geometry bindings stop when clipping is disabled or the layer retires.

Both builds/all forty test groups pass. Five full runs per backend plus fallback pass in Debug and
Release (`probe-rounded-motion-full-debug.log`, `probe-rounded-motion-full-release.log`); focused
final probes also check complete collapse and explicit compact radii against original geometry.
Twenty-four sampled sizes per backend cover six widths at four DPI scales. Fallback and reconstructed
cached coverage match the established scalar/static rounded viewport exactly. An additional direct
rounded-fill comparison exposes the existing mask/primitive antialias difference, at most 39 channel
values at rounded edges. `rounded-compare.png` was inspected; shape extent and interior coverage agree.
This is a consumer appearance consideration, not a relaxed viewport comparison tolerance or proof
of displayed WUC/DC pixels. Complete Dock/Switcher width fitting and compact shadow behavior remain open.

The previous direct-launched validation host exited with code 1. The registry subsequently reported
Explorer and no Reach/watchdog process. The user clarified that direct launching conflicts with
Explorer and they had killed Reach; watchdog recovery can also restore Explorer. No crash event
established a separate code fault. Future setup uses the existing folder's `reachctl --install`
with the user's first administrator approval, then `reachctl --start`. Restarts stop the watchdog
before Reach and use `reachctl --start`; direct executable/watchdog launch is prohibited. Previous
startup observations do not establish valid installed-session or displayed-product acceptance.


The corrected installed-session setup succeeded: `C:\Dev\reach\build\reachctl.exe --install` returned
zero after administrator approval, and `reachctl --start` returned zero from the same folder.
The same host/watchdog PIDs remained alive on later checks and both scheduled tasks were running;
Settings opened from the installed sibling path. Computer Use monitor capture still failed with
`0x80070057` on a refreshed-window retry, so displayed/input acceptance remains unresolved.
Only the first setup installs; subsequent validation restarts use watchdog-first termination and
`reachctl --start`. No direct launch or additional reinstall is needed.


## Compound fitting and moving constraints

Canonical motion now combines active output clamps and multiple mapping breakpoints from the same
layout epoch. Shared min/max operations flatten moving viewport constraints. Both adapters consume
the existing cubic compiler; no feature branch, second clock, native expression graph, timer or
source allocation is introduced. Capacity is bounded at eight slope breakpoints, 32 finite pieces
and 68 rotated-cycle pieces; incompatible moving epochs and excess knots reject without changing
an aliased output. Existing two-slope paths retain their fast combination path.

Focused non-UI tests compare independent popup placement/fitting/reflow math at delayed submission
in both directions and all three easings, within 0.001 physical pixel. Periodic tests verify multiple
peaks, endpoint holds, phase rotation, first crossings and interruption; capacity and malformed
metadata tests cover identity and failure without output mutation. This capability does not yet
migrate Dock/Switcher popup width, compact shadow rendering or title trimming.

The x64 size probe reports `reach_motion` growing from 96 to 152 bytes, a retained group from 1440
to 2224 bytes, and a fixed-capacity command buffer from 828032 to 1004432 bytes. This is 176400
additional bytes (172.3 KiB, 21.3%) per buffer, plus 784 bytes per retained layer descriptor and other
motion owners. Reach owns two such buffers in its heap-allocated host; Settings owns one in its
heap-allocated app. Their combined buffer increase is 529200 bytes before layer/state copies.
Unused knot storage remains inline and is ignored by semantic equality. This is a
bounded shared-capability cost, not evidence of lower total memory or energy use; final product
memory/CPU/GPU/power gates remain open.


Both builds and all forty test groups pass (`build-composed-motion-final-debug.log`,
`build-composed-motion-final-release.log`, `test-composed-motion-final-debug.log`,
`test-composed-motion-final-release.log`). Corrected native suites pass five runs on WUC and DC plus
HWND fallback in Debug and Release (`probe-composed-motion-full-debug.log`,
`probe-composed-motion-full-release.log`). Each growth/reversal pair reuses source pixels and scene
topology, with four submissions: one visual and one rounded-clip update per publish. Settled and
unchanged repeated submissions mutate no properties/tree; hiding retires periodic visual and
rounded-geometry bindings. The diagnostic initially reset constant scale on every publish, then
incorrectly expected a visual-plus-clip update to count as one submission. Fixing those diagnostic
assumptions yields zero failures; no production frame driver was added or changed for them.
No Reach process was stopped or directly launched. Displayed consumer appearance and final total
product workload acceptance remain pending; owned diagnostics do not establish those gates.


## Canonical Now Playing marquee

The Now Playing capsule now owns canonical periodic X motion. Both adapters consume the existing
shared compiler; the three legacy cycle fields, custom WUC Bezier/DC smoothstep drivers and
preserve-repeat branches are removed. Both now use the application's cubic ease-in-out curve,
intentionally resolving the old backend difference. Overflow/speed/threshold remain logical and
endpoint holds remain 1.6 seconds. Title or overflow changes restart at the readable endpoint;
unchanged source, theme repaint and font/source recreation retain the capsule's absolute phase.
Generic surface context carries renderer capability through the existing registry adapter to
Top Bar's render context, keeping unsupported marquee rendering static with no CPU motion demand.
The remaining Now Playing width animation is a separate outstanding consumer.

Both builds and all forty test groups pass. Existing deterministic coverage checks four DPI values;
a focused motion-policy check verifies phase identity, holds, changes and disabled/reopened intent.
Each focused Debug/Release native suite passes eighty cases across both compositors, two themes, four DPI
values and five repetitions. Owned synthetic media-service/capsule probes verify real source sync,
unchanged/theme phase, title replacement, disabled/reopened intent and reset. This extends existing
owned diagnostics rather than adding UI tests. Mid-travel font replacement retains the exact epoch
and generation. Five full regression runs per backend plus HWND fallback pass in both builds.
Evidence: `build-marquee-canonical-debug.log`, `build-marquee-canonical-release.log`,
`test-marquee-canonical-debug.log`, `test-marquee-canonical-release.log`,
`probe-marquee-canonical-focused-debug.log`, `probe-marquee-canonical-focused-release.log`,
`probe-marquee-canonical-full-debug.log` and `probe-marquee-canonical-full-release.log`.
No displayed appearance or total CPU/GPU/power conclusion follows from these structural checks.


The x64 size probe now reports a 2208-byte retained group and 1000832-byte command buffer, down
16 and 3600 bytes respectively after removing the legacy cycle fields. The capsule adds one
152-byte canonical motion owner; the larger compound-motion storage from the earlier milestone
still requires the final product memory assessment.

The initial validation restart command was rejected by automatic approval because termination
required authorization recognized in the current instruction. It did not execute. Read-only
inspection outside the sandbox showed the existing host/watchdog; sandbox enumeration had returned
no matching processes and is not authoritative for desktop lifetime checks. After explicit approval
for one restart, watchdog-first termination and installed-folder `reachctl --start` succeeded.
Reach PID 18572 and watchdog PID 10584 remained alive on later checks; installed host/Settings hashes
match the checked Debug builds and both scheduled tasks are running. The existing installation was
reused. Settings opened from the installed folder and the task-opened window was closed normally.
Capture still fails with `IGraphicsCaptureItemInterop.CreateForMonitor ... 0x80070057` after refreshed
selection, so no displayed-pixel/input acceptance is claimed. Future lifetime checks must inspect
the desktop process view outside the sandbox; never restart based on a sandbox-empty process list.

## Owned caret source and execution comparison

The Release diagnostic compares the existing compound textbox at blink deadlines, a separate
caret source at the same deadlines, and that source with native step opacity. Each backend runs
five six-second phases per method in rotated order. The input-transparent diagnostic window is
32 by 32 pixels with root opacity 0.01; the editor source is 180 by 32 pixels. It does not open
Settings, send input or change the installed Reach instance. These conditions intentionally limit
desktop disturbance and also limit what the measurements establish.

Both backends pass 128 offscreen pixel comparisons covering two fonts, four DPI scales, two themes,
selection, placeholders, alignment, multilingual text and clipped carets. Composing the isolated
stroke over the caret-hidden textbox differs from the original compound source by at most one
channel value; 24 cases per backend have empty/clipped ink. Each ordinary phase performs eleven
application submissions. The compound source repaints eleven times; the separate source repaints
zero times. Native step opacity performs zero application submissions or raster passes during the
phase. Estimated retained source bytes are 23,040 versus 23,440, an increase of 400 bytes (1.74%).
Every phase retires its sources successfully. This prototype is not shipped caret integration.

The workload collector now includes absolute `qpc_seconds` alongside its relative timestamps,
allowing native phase boundaries to be joined to raw samples. For this capture, retain only sample
intervals wholly inside each phase after excluding its first and last 0.5 seconds. There are 571
usable intervals, 19 per phase except one with 20; all report valid selected-process GPU coverage.
Caret CPU uses process times; DWM CPU uses the recorded Process V2 fallback after direct access was
denied. Each table entry is the median of five interval-weighted phase means. CPU percentages use
all sixteen logical processors. GPU uses the busiest selected-process engine on the active adapter,
not a sum of independent engines. Package and board power cover the whole machine's package/GPU.

| Backend | Method | Caret + DWM CPU % | GPU engine % | Package W | GPU board W |
| --- | --- | ---: | ---: | ---: | ---: |
| WUC | Compound deadline | 0.4732 | 0.6554 | 31.9378 | 22.5289 |
| WUC | Separate deadline | 0.6374 | 0.6592 | 31.8756 | 22.4967 |
| WUC | Separate native step | 0.5140 | 0.6906 | 31.9299 | 22.4871 |
| DC | Compound deadline | 0.5343 | 0.7227 | 31.9323 | 22.7932 |
| DC | Separate deadline | 0.6992 | 0.6781 | 32.0029 | 22.7428 |
| DC | Separate native step | 0.4523 | 0.7309 | 31.8727 | 22.7593 |

Phase ranges overlap substantially: combined CPU is 0.329–0.781%, GPU engine utilization
0.423–0.854%, and background system CPU varies from 0.715–2.712%. One DC compound phase has a
26.628 W board-power mean while the other phases are around 22–23 W. Per-phase caret CPU is mostly
below the process-time measurement resolution, with occasional 15.625/31.25 ms increments.
Collector calls take 4.6–83.8 ms; observer CPU phase means are 0–0.0206%. The capture therefore
establishes removal of application/raster work and a small source-memory cost, but does not establish
a total CPU/GPU/power improvement or regression. It does not measure presentation latency, full
application command/layout work, displayed blink timing, battery behavior or a fully visible editor.

Evidence under `build/gpu-composition-migration`: `caret_comparison_probe.inc`,
`probe-caret-comparison-pixels-release.log`, `caret-comparison-phases.jsonl`,
`caret-comparison-native.log`, `caret-comparison-system.jsonl` and `caret-comparison-summary.json`.
The native execution prototype uses WUC step easing and DC constant cubic pieces with repetition;
the canonical compiler at the comparison revision still models continuous travel. Production integration must reuse
shared textbox layout/ink geometry and canonical intent, preserve exact discrete blink timing,
focus/edit resets, visibility retirement and fallback scheduling, then repeat the comparison with
real visible product workloads. Independent caret composition and the final CPU disposition remain
open; this bounded comparison does not close the repetition or product-acceptance gates.

## Canonical discrete repetition

`reach_motion_repeat_at` now accepts zero travel with positive endpoint holds. CPU sampling switches
at the hold boundary; rotated compilation emits two or three constant pieces with explicit next
boundary values. WUC applies final-step easing to discontinuous boundaries; DC repeats constant
cubics. Both reuse existing property publication, semantic equality and lifetime retirement.
There is no extra motion storage, timer, epsilon-duration fade or adapter-owned blink clock.
Missing holds become a constant endpoint. Mapping/range/activity and crossing queries inspect the
visited endpoints; skipped intermediate values report no crossing. Finite retargeting begins at the
current discrete value and retires the prior generation. Continuous loader/marquee curves preserve
their existing compilation.

Focused pure-clock coverage verifies exact binary-representable boundaries, rotated/future-start
compilation, affine/mapped ranges, unreachable crossing, constant/invalid holds and interruption.
It also compares twelve absolute boundaries for each existing 0.55/0.45 and 0.53/0.53 caret duty
cycle against `reach_blink`. Exact absolute hold/cycle endpoints retain their boundary value despite
floating-point subtraction, without epsilon-duration motion or phase tolerance.
Owned native checks cover four delayed submission phases per backend, unchanged binding/source
reuse, reset to the Wi-Fi duty cycle without repainting and retirement. Native waiting performs no
application submission. WUC animation controllers are present while active and absent after hide.
These are compiler/binding checks, not displayed blink timing or an end-to-end textbox migration.
The shared caret source contract, real consumer integration, fallback scheduling and product
cost/appearance acceptance remain open.

Both isolated builds and all forty test groups pass. The initial test invocation used the sandbox's
default temporary directory and failed the two temporary-file fixtures; repeating with the
established workspace TEMP/TMP passes. Full native suites pass five runs per backend plus HWND
fallback in Debug/Release. After the absolute-boundary refinement, both complete test suites and
focused native periodic/discrete suites pass again. Architecture, formatting and whitespace pass.
Evidence: `build-discrete-{debug,release}.log`, `test-discrete-{debug,release}.log`,
`probe-discrete-full-{debug,release}.log` and `probe-discrete-focused-{debug,release}.log` under
`build/gpu-composition-migration`. Installed validation and the real caret source/consumer path are
separate checks; compiler success does not establish displayed blink behavior or product efficiency.

Installed validation succeeded after one explicitly approved watchdog-first restart through
`C:\Dev\reach\build\reachctl --start`. Reach PID 7644 and watchdog PID 6792 remained alive on a later
desktop process check; installed Reach/Settings hashes match the checked Debug binaries. The
initial automatic-approval rejection did not execute. The user then updated the active goal to
authorize subsequent goal validation restarts without further permission questions. The first
installation remains in use; no reinstall or direct host/watchdog launch was performed.

## Independent production caret sources

Settings Account, Power and Wi-Fi fields and Launcher now publish canonical discrete caret opacity
from their existing `reach_blink` owner. Focus/edit reset increments its generation; fallback
sampling retains that generation/epoch. Shared textbox command production adds an optional
`caret_group_id` reference to an ordinary retained opacity descriptor. The adapter resolves only
the stroke ink from the same bounded cached DWrite layout used to paint text, creates a child of
the isolated editor source and preserves its natural raster limits, ancestor transforms and clips.
The editor stays a single-command, tint-free source; validation rejects missing, shared or ambiguous
caret ownership before native plan construction. A generic line primitive paints the resolved
stroke without creating a second text layout or a textbox-sized surface. Body caching ignores the
caret index when the body does not paint it.

Supported Settings and Launcher paths suppress caret advancement and deadline publication. They
perform no blink-driven application command build, layout, rasterization or submission. Native
mode/source selection stays inside the feature/render contracts; only the existing registry seam
passes Launcher's generic renderer capability. Offscreen fields publish no part; minimized/suspended
rendering retires sources and native bindings. Font recreation retains the absolute epoch/generation.
Unsupported renderers retain the existing visible-field deadline scheduling and compound paint
path. Necessary text/layout/ink resolution stays on the CPU at content/input changes and reuses the
existing 32-format/128-layout caches rather than allocating a new layout on every reconciliation.

Pure non-UI coverage verifies projection identity across deadline sampling, same-time reset,
retirement and part ownership/preflight. Both builds and all forty test groups pass. Owned actual
Settings producers cover three caret pages, two themes and four DPI values; only the focused visible
field owns a part and native scheduling waits indefinitely for other work. Launcher producer,
capability switching and absent native caret deadline pass; existing focus/edit/page/blur/radio/
minimized/offscreen and two-wake-per-cycle fallback checks still pass. The scheduler fixture clears
its deliberately switched native render graph before checking the isolated fallback clock.

The previous committed textbox drawing function is the reference for 128 cases per backend, with
two fonts/themes and four DPI scales, placeholders, selection, multilingual text, alignment and
clipped ink. Production layout plus separately composited stroke differs by at most one channel
value. A further 96 actual native source graphs per backend match the retained baseline pixels
exactly, including natural editor raster clipping. These checks pass in Debug and Release. A full
cycle submits/rasterizes nothing; unchanged submission mutates no properties/tree; focus reset
changes one opacity property with zero raster passes. Font recreation preserves the exact intent,
and suspension retires every source/binding. The full pre-cache-refinement regression suites pass
five runs per backend plus HWND fallback; both complete test suites and focused source/pixel suites
pass after cached-layout reuse. No new UI tests were added.

The x64 command grows from 1016 to 1024 bytes, its fixed buffer from 1000832 to 1004800 bytes
(3968 bytes, 3.875 KiB), and `reach_blink` from 40 to 48 bytes. Three app buffers therefore add
11904 bytes before other state. Motion/group sizes stay 152/2208 bytes. The bounded 180 by 32 editor
scene retains 23040 estimated source bytes versus 23480 with the production caret: 440 additional
bytes (1.91%). This excludes native object, allocation granularity and driver/cache memory; it is
not a total-memory result.

Evidence under `build/gpu-composition-migration`: `build-caret-sources-{debug,release}.log`,
`test-caret-sources-{debug,release}.log`, `probe-caret-sources-{debug,release}.log`,
`probe-caret-consumers-{debug,release}.log`, `probe-caret-sources-full-{debug,release}.log`,
`caret_reference.cpp` and `caret-size.cpp`. Displayed blink timing, moving/scaled compositor pixels,
input/latency and full product CPU/GPU/power/memory acceptance remain open.

The final owned Release comparison uses the real production part for the native method. It repeats
five six-second phases per method/backend in rotated order with the same small, input-transparent
32 by 32 scene at opacity 0.01. After excluding each phase's first/last 0.5 seconds, 571 complete
intervals have valid process/DWM/GPU coverage. DWM CPU again uses the recorded Process V2 fallback.
Each table entry is the median of five interval-weighted phase means; CPU uses all sixteen logical
processors and GPU uses the selected-process busiest engine. Package/board power is whole-device.

| Backend | Method | Caret + DWM CPU % | GPU engine % | Package W | GPU board W |
| --- | --- | ---: | ---: | ---: | ---: |
| WUC | Compound deadline | 0.5549 | 2.1607 | 33.5914 | 24.0356 |
| WUC | Separate deadline prototype | 0.5782 | 2.5317 | 33.4216 | 24.1469 |
| WUC | Production native step | 0.6787 | 2.8244 | 33.7461 | 24.1279 |
| DC | Compound deadline | 0.6371 | 2.6441 | 33.5119 | 24.0247 |
| DC | Separate deadline prototype | 0.6167 | 2.1297 | 33.9500 | 23.9726 |
| DC | Production native step | 0.6838 | 2.6824 | 33.6324 | 24.0592 |

Compound phases perform eleven application submissions/repaints; production phases perform zero
of either, with the same cached body source. Total workload is inconclusive: GPU ranges overlap
across methods (0.925–2.927%), background CPU varies 1.704–7.969%, observer CPU phase means are
0.370–0.411%, and board/package outliers reach 27.060/37.645 W. In particular, the higher WUC native
GPU median is not evidence of an efficiency win, and this capture does not establish equivalence
or a meaningful regression threshold. It does not include complete application builds/layout or
normal visible product presentation. Keep final workload/latency/power gates open and repeat with
controlled visible product scenarios before final acceptance.

Raw evidence: `caret-production-phases.jsonl`, `caret-production-native.log`,
`caret-production-system.jsonl`, `caret-production-summary.json`. The initial capture omitted
`REACH_WORKLOAD_REPORT`, disabling diagnostic counters and failing counter assertions. Its files
are retained as `caret-production-invalid-*` and excluded from these results. The corrected capture
enables counters, completes all thirty phases and reports zero diagnostic failures.


The installed validation restart used the updated goal authorization without another permission
question. Watchdog-first termination, checked-binary replacement and installed-folder
`reachctl --start` succeeded. Reach PID 19168 and watchdog PID 17156 run from the installed folder;
Reach/Settings hashes match the final checked Debug builds. The existing installation was reused.
This startup/lifetime check does not establish displayed caret timing or full product interaction.


## Width-dependent retained text sources

The shared text/viewport relationship prepares DirectWrite trim states and selects retained raster
rows with canonical finite lookup. It preserves ordinary font size, character ellipsis and
alignment without a second clock. Static fields retain one ordinary source. Active fitting prepares
needed blocks, retaining prior block identities for retargets. Source owner namespaces survive
command reordering. The existing 128 MiB nominal aggregate source limit, 16,384-pixel dimensions
and 449 native-layer bound remain in force. Incremental staging bounds temporary source-command
expansion as well; it does not build an entire oversized graph before selecting fallback.

Oversized/unsupported title graphs retain ordinary text pixels and report renderer-owned sampling
through the render port. Both application loops can sample those records without arranging a
capsule or rebuilding commands. Actual source rasterization and renderer execution remain counted;
one final sample clears demand. This is an explicit capacity fallback, not a product efficiency
claim or exemption for normal preview width tracks.

Both complete builds and forty Debug/Release test groups pass. Owned production checks cover 288
static comparisons on WUC/DC, two production font modes and four DPI values, plus displayed finite
forward/reverse endpoints, unchanged sources/tree, command reorder, color/font changes, scissoring,
source-only fallback and resource retirement. Static sources match ordinary reference pixels;
active atlas output matches an independent translated glyph state within the two-channel bound.
The earlier prototype isolates fractional filtering from glyph selection: legacy edge differences
reach 133, while flat interiors/exteriors have zero error. The inspected comparison and failed
initial matrix remain recorded; no legacy tolerance was silently raised. Complete intermediate
consumer appearance and controlled product cost/latency remain open.

The command grows 1,024 -> 1,032 bytes; buffer 1,013,800 -> 1,017,768 (+3,968 / 0.39%). Motion and
retained group storage remain 152 and 2,248 bytes. Two render callbacks add sixteen bytes per port.
Recipe caches, sampled-source records, internal owner maps, retained source commands and atlas
pixels add further bounded CPU/GPU storage. Earlier small prototype scenes use approximately
0.5--1.34 MiB including a 281,600-byte background; current static production fields return to an
ordinary source. These estimates exclude driver/native object allocation and do not establish total
memory equivalence. Full Dock/Switcher fitting and the final comparison remain pending.

Evidence under `build/gpu-composition-migration`: `text_width_probe.cpp`,
`text-width-layout-release.log`, `title_width_native_probe.inc`, `text-width-size.cpp`,
`probe-title-width-atlas-reference-release.log`, `probe-text-width-final-{debug,release}.log`,
`probe-text-width-full-{debug,release}.log` and the text-width build/test logs. Final scoped bounded-staging
checks also pass (`probe-text-width-bounds-{debug,release}.log`): fifty simultaneous title fields use
446 native layers and 55,942,400 nominal bytes, selecting six sampled sources. Final serial ctest
runs pass all forty groups in each build. The installed watchdog-first/reachctl restart is verified
with matching Reach/Settings hashes; the text-width design records its process lifetime evidence.


## Switcher fitting producer — 2026-10-15

Switcher now publishes retained rounded fills, body placement, proportional cards and centered
width-text viewports inside a stable session envelope. The registry forwards canonical native
destination rectangles through the existing generic overlay path. Its scalar width manager,
periodic width tick/redraw and sampled card render geometry are removed. One cleanup deadline
publishes settled sources; monitor/DPI changes cut stale intent, and surviving window identities
retain source tokens. The existing close fade continues width motion and hide retires it.

`probe-switcher-width-final-debug.log` is an owned diagnostic of the actual Switcher capsule,
tracking service, producer and both native renderers: four DPI scales, two themes, source removal
and restoration, interruption, proportional card/native rectangles at five points in the epoch,
minimized-source retirement, monitor/DPI cuts and source ownership cleanup. Motion-only intervals
record zero scalar advances, layouts, command builds and raster passes. Repeated native frames
also record zero raster/property/tree changes. The ordinary three-card scene uses nineteen native
layers (twenty with the diagnostic backdrop). It delivers one endpoint redraw/update and then no
further capsule work. This fixture uses controlled source metadata; displayed live DWM alignment
remains in the final interaction matrix.

A second snapshot freezes the real production descriptors at the middle of shrink and growth phases and compares
displayed WUC/DC pixels with the established static Switcher renderer using identical logical
geometry. Each eight-case phase per backend has zero error in flat interiors/exteriors.
Antialias/glyph-edge channel differences reach 151 during shrink and 113 during growth. `switcher-width-inspection-{0,1}.png` was
inspected: geometry, border thickness, corners and label fitting agree; glyph edges show the
expected fractional-source filtering difference. This bounds the implementation evidence without
claiming final product appearance or total cost acceptance. The backdrop makes actual product
pixel coverage explicit rather than accepting arbitrary captured desktop alpha.

Pure ownership checks verify the shared animated fill's parent/ID/range rejection and stationary
output scissor. Both builds and all forty test groups pass; the existing popup finite/retarget
matrix reruns after compact body fills begin sharing the same primitive. Final native regression
and installed-validation evidence follows with the consumer milestone.

Final consumer evidence: `probe-switcher-width-growth-{debug,release}.log` explicitly verifies
intermediate growing-card/title widths and interruption from active growth. Both report zero
failures; growth adds one retained text block in the ordinary diagnostic scene. The full renderer
suites in `probe-switcher-width-full-{debug,release}.log` pass five runs per compositor plus HWND
fallback. `probe-switcher-popup-regression-{debug,release}.log` passes the shared popup finite/
retarget matrix. `probe-switcher-popup-scissor-final-release.log` also passes combined repeated,
finite-retarget and whole-output scissor capture. The first combined scissor run exposed a private
diagnostic error: clearing its buffer removed the clip, but only the reference reapplied it. The
corrected diagnostic restores the same clip on every submission; no tolerance was relaxed.

The checked Debug binaries are copied into the existing installed folder after stopping watchdog
PID 17284 and waiting, then stopping Reach PID 12588 and waiting. Startup uses only that folder's
`reachctl --start`. Subsequent enumeration verifies watchdog PID 18188 and Reach PID 13112 from
`C:\Dev\reach\build`. Reach SHA256 is
`7A27DB97098D2E44CC0CA91A60DC05BD3799D17C682F5937DC8755C99320BB54`; Settings SHA256 is
`BD52E6A121096CD59E8A8C3BAA369A1AF6476A843CB8451D7A25B70CADDA394F`.
This closes the scoped implementation milestone; displayed live native alignment and aggregate
CPU/GPU/power/memory/latency acceptance remain open.


## Dock width/card/title fitting — 2026-10-15

Dock's Context Menu preview capsule now derives width, survivor reflow, monitor-clamped body/notch,
proportional card geometry, title widths and close/icon geometry from one canonical layout epoch.
A stable session envelope replaces width-driven HWND/content layout. Native destination rectangles
and event-time input/hold snapshots share the same descriptors. Visible-source tokens survive
reorder; changed source extents retarget, monitor/DPI constraints cut, and close freezes the entire
layout. One cleanup deadline retires it. The scalar width manager/tick/frame demand and sampled
render rectangles are removed; ordinary menus keep their existing path.

The full producer exposed three appearance issues before acceptance. Settled fitted text and vector
sources now preserve physical raster origin under fixed unit-scale ancestors; moving unchanged
sources can retain that origin and realign at a legitimate endpoint submission. Static childless
solid clipped fills bake their rounded source coverage while moving geometry keeps native clips.
WUC stretch pieces now select hard quad edges so the existing source alpha joins without visible
seams. The preview border also reuses the established pixel-aligned bordered-shape recipe when
geometry is fixed. The first fractional-DPI border mismatch reached 131 because the replacement
had omitted that rounding policy; the corrected source preserves it. No tolerance was raised to
accept that defect. Rounded-fill baking preserves parent clips when descendants are present.

`probe-dock-static-title-alignment-debug.log` compares static fitted titles under fractional parent
translations with independent physical-position text. Both renderers, two font modes, four DPI
scales and two alignments produce 32 displayed comparisons with zero channel error.
`probe-dock-width-static-matrix-final-debug.log` compares the complete Dock producer against the
established renderer: four DPI scales, both themes and five hover states, forty cases per backend.
Flat interiors/exteriors and title regions retain the existing strict limits; remaining inspected
corner/glyph antialias edges remain within the established 64-channel shape bound. Its owned
windows and explicit backdrop ensure the comparison contains actual product pixels.

The former private cached-source consumer approximation omitted the production clipping/content
transform and physical-raster preparation. It is replaced in full native runs by the complete
owned-window producer comparison, retaining the independent static renderer and strict flat/title
checks. The shared offscreen source helper remains available for its other primitive diagnostics.
This upgrades the evidence to actual WUC/DC presentation rather than treating the approximation's
unmodeled edge filtering as a product oracle.

`probe-dock-width-motion-compact-debug.log` verifies 48 actual producer cases across both renderers,
four DPI scales, both notch directions and left/center/right anchors. The initial source has 0.1
aspect, explicitly exercising compact shape-shadow chrome before growth. Five points in the epoch
verify equal preview heights, source aspect, complete moving-body containment and native/card
geometry. Exact event timestamps verify interruption continuity. Repeated native frames change no
rasters/properties/tree and capsule ticks dirty no content. Endpoints deliver one cleanup; closing
and force-hide freeze/cancel layout, and empty scenes retire rasters, sampled text and owner maps.

The qualified sixteen-source case in `probe-dock-width-limits-qualified-debug.log` starts compact
and grows to wide cards under controlled 16,000px monitor constraints. It uses 186 command groups,
270 native layers, 133,218,740 nominal source bytes and one sampled title source, staying below the
existing 224-group, 449-layer and 128 MiB source bounds. Source-only title sampling changes no
layout/commands/native properties/tree. A constrained monitor/DPI cut and complete-card paging
expose every source; the last page renders exactly one enabled previous arrow. Cleanup preserves
uninitialized disabled navigation fields, and force hide cancels an active layout generation.
The first maximum fixture had untitled identity-less snapshots, which the tracking service correctly
excluded; qualifying those controlled snapshots fixes the fixture without changing product policy.

Both builds and all forty test groups pass. Focused Release static/motion/maximum runs also pass.
Full native regressions and installed validation pass; final shared title/popup cross-checks are
recorded below.
These are controlled producer/resource fixtures, not proof of displayed live DWM alignment, real
mixed-monitor behavior, aggregate CPU/GPU/power/latency or independent GPU-memory acceptance.


The capsule descriptor grows from 24,880 to 34,952 bytes (+10,072); its layout grows from 3,792 to
13,872 bytes and each stored card from 224 to 656 bytes. The additional space owns canonical
rectangles rather than full command buffers. Event-time placements are separate small snapshots.
Render-buffer and render-port ABI sizes do not change in this consumer milestone. Independent
physical GPU-memory and aggregate process-memory acceptance remains in the final measurements.


The solid fill relationship is explicit through `rect_fit_to_clip`, emitted by the motion-fill
helper and checked as one dedicated raster owner with no independent descendants. It occupies the
previous four-byte padding after the command type: command/group/buffer sizes remain
1,032/2,248/1,017,768 bytes. Native fixed-fill preparation is restricted to that relationship;
arbitrary viewport clips preserve their unchanged-source animation contract. A broader rounded
reversal check caught the overly general first optimization, and the explicit ownership restores
its zero-raster result without relaxing the check. CPU fallback samples the same fill geometry
directly while retaining ancestor clipping and tint. The complete static fallback matrix in
`probe-solid-clip-dock-fallback-debug.log` passes all eighty comparisons with zero flat/title error
and at most two channel values overall. Generic rounded reversal and combined popup repeat/finite/
scissor capture also pass in `probe-solid-clip-generic-popup-debug.log`.

The HWND session envelope stays stable, while settled/frozen compact chrome may reduce its source
capture to the current width. Active retargets retain the larger source envelope; endpoint/close
events can release its unnecessary shadow area without adding periodic work.


Settled compact popup and common motion backgrounds now use the canonical ordinary cached source
instead of retaining a shape-shadow graph whose geometry is constant. Active compact width ranges
still use the graph, and transition completion changes ownership only on the existing cleanup event.
This reduces static source/layer cost and preserves the original static chrome pixels.

The final compact fallback check isolates popup chrome from deliberately tiny card content. Both
shared-device and WARP/readback shadow execution match the ordinary chrome reference within
seventeen channel values at the curved edge. The first diagnostic incorrectly compared clipped
zero-width retained titles with the old title overflow and forced its ordinary shadow reference
into a device-less mode; those are diagnostic errors, not accepted shadow regressions. Ordinary
complete-card static title/fill comparisons remain separate and keep their original strict limits.
`probe-static-chrome-motion-final-debug.log` passes all forty-eight motion cases and the compact
fallback checks with zero failures. No temporary renderer instrumentation remains.


Dock milestone acceptance update, 2026-10-16: `probe-dock-final-full-{debug,release}.log` each passes
five complete native runs per backend plus the HWND fallback checks with zero failures. The final
`probe-dock-final-{static,fallback,motion,limit,switcher}-{debug,release}.log` suites all pass. Dock
static scenes cover both themes/four DPI scales; motion covers four DPI scales, two directions and
three anchors. Maximum membership retains the existing byte/layer bounds and releases all owners.
Both forty-test suites, architecture, formatting and whitespace checks pass. The final Debug text
and combined rounded/popup repeat/finite/scissor checks pass. The matching Release text/popup batch also passes with zero failures. These shared cross-checks
retain the original strict source-reuse, ownership and clipping limits.

Deployment stopped watchdog PID 7480 and awaited its exit, then stopped Reach PID 1044 and awaited
its exit. Checked Debug Reach/Settings binaries and PDBs were copied into the existing `build`
folder; only that folder's `reachctl --start` was used. Read-only verification found watchdog PID
1444 and Reach PID 14304 alive from `C:\Dev\reach\build`, both created 2026-10-16 at 11:24:12 local.
Reach SHA256 is `9B39AB44A921F4631201805AD4BA76CB11E091D04296CA6FC26975AC3D1DF653`;
Settings SHA256 is `4CB97FB5B7921339A251DF733A4161CAF7CE0CC21D4EAD60C98E896F1AC9155B`.
Live source alignment, native refresh variation and total product cost remain open parent gates.


## Launcher retained results and root pivot - 2026-10-16

Implementation removes Launcher's result scalar manager, cached expansion value, periodic tick
redraw and frame demand. One canonical owner drives compact background height, result clipping,
per-paint reveal opacity and a 4dp slide. Separate paint sources preserve overlap alpha; flattening
the full result scene under one opacity was rejected after the independent reference showed changed
colors. Stable row namespaces keep unrelated rows independent of selection and scrolling.
Event-time hit geometry follows the current body and slide and excludes unrevealed content. One
endpoint cleanup reduces the source graph and returns the capsule to idle. The existing search
completion generation/open checks still reject stale or hidden results; service policy is unchanged.

Optional root center X/Y descriptors preserve the existing physical zoom pivot while body height
changes. Generic frame stamping, host inverse input, native nonclient hit testing, CPU fallback and
native preview projection share those coordinates. Both drivers bind the ordinary scalar compiler
to center properties. Visible centers participate in root completion; once opacity is fully zero,
an otherwise finished close retires an invisible center without extending activation latency.
Suspension stops the bindings. Source/group command sizes remain 1,032/2,248 bytes. The buffer grows
by 304 bytes to 1,018,072 bytes (about 0.03%); the descriptors are stored once, not in every group.

`probe-root-center-lifetime-debug.log` verifies displayed marker positions, event-time reversal,
zero source/property/tree changes between events, transparent close completion and suspension on
both native backends. The later focused center runs also exercise generic native input publication,
moving-center Windows hit testing, host inverse input and native preview forward projection.
The pure ownership check rejects ownerless/repeating centers and verifies default-axis inheritance
and clearing. Existing Launcher expectations now sample the canonical input bounds rather than the
old per-frame visible-bounds snapshot; no tracked UI test was added.

`probe-launcher-verified-{matrix,fallback,error}-debug.log` passes real producer/deadline/input/source
checks and complete pose comparisons across both themes, four DPI scales and both compositors;
ordinary CPU fallback and the search-error row use the same intent. Matching focused Release runs,
settled comparisons, root-center lifetime and Switcher/popup shared regressions also pass.
Both builds and all forty test groups pass. Complete native regressions and installed verification
also pass, as recorded below. These scoped scenes do not establish full product acceptance.

Appearance evidence separates fixed source pixels from expected fractional movement. Settled
sources match the previous renderer within two channel values, with zero flat-color error. During
motion, flat pixels retain the strict three-channel-value limit. The general non-text edge cap stays
64; a separately calculated 1.5-pixel band around the rounded outer/inner contours covers the
half-pixel difference from the old integer height rounding. Observed high-contrast perimeter error
reaches 65. Constant axes now reuse the canonical pixel-aligned rectangle recipe, fixing the wider
corner discrepancy. At one 200% DPI reveal pose, cached glyph sampling can choose an adjacent
physical baseline: the measured fractional phase plus slide lies almost exactly at a half-pixel
boundary. Inspected glyph edge differences reach 128 there; the scoped intermediate text bound is
150, matching the earlier retained-text acceptance class. Text metrics, font size and painter order
are unchanged; settled text keeps its strict two-value gate. The first undifferentiated 64-value
image cap exposed these two causes; it was replaced by the spatial/settled checks above, not treated
as a passing whole-image comparison. Images and raw readbacks remain in the ignored diagnostic
folder (`launcher-second-inspection.png`, `launcher-corner-inspection.png`).

Source-memory comparison excludes the blue diagnostic backdrop: its presence changes the old
ungrouped source union and otherwise biases the comparison. The five-visible-row, twelve-candidate
scene at 100% DPI uses 5,010,304 estimated bytes during the actual reveal and 2,321,344 bytes after
cleanup, versus 1,792,068 for the settled previous renderer. At 200% DPI the constrained two-row
scene uses 7,201,824 bytes during reveal and 3,272,672 after cleanup, versus 2,576,368 previously.
The 125% DPI active case reaches 7,859,268 bytes. All remain inside the existing byte/layer limits,
and hiding releases every source. The extra per-paint sources and compact shadow graph have a
measured storage cost; fewer application frames alone do not accept it. Independent GPU-memory,
whole-process memory, total CPU/GPU, power and latency comparisons remain final acceptance work.

One endpoint diagnostic originally mixed GetTickCount granularity with the canonical QPC clock and
could check a few milliseconds early. It now waits against the actual motion deadline; production
scheduling was unchanged. Final pure build/architecture/format/whitespace checks are recorded with
the complete regression and deployment evidence when that milestone finishes.


Launcher final regression update: `probe-launcher-full-corrected-{debug,release}.log` each passes
five complete native runs per backend plus HWND fallback checks with zero failures. The initial
full Debug run caught five copies of one default-root endpoint regression: the new center helper
was resetting a center even when no center animation existed. Endpoint reconciliation now skips
absent/constant center updates; the original one-property-submission bound is unchanged.
`probe-launcher-root-endpoint-{debug,release}.log` and
`probe-launcher-root-center-corrected-{debug,release}.log` pass both ordinary and explicit-center
completion/lifetime paths. The complete forty-test suites pass in both configurations after that
fix (`test-launcher-endpoint-{debug,release}.log`), along with architecture, formatting and whitespace.


Installed Launcher milestone validation stopped watchdog PID 1444 and awaited exit, then stopped
Reach PID 14304 and awaited exit. Checked Debug Reach/Settings binaries and PDBs were copied into
the existing `C:\Dev\reach\build`; startup used only that folder's `reachctl --start`.
Read-only lifetime verification found watchdog PID 12248 and Reach PID 15588 alive at those paths,
both created 2026-10-16 at 12:29:39 in the client Casablanca timezone. Installed Reach SHA256 is
`4156071E961A4AC6AB0FFC6F7BD9F6DDF8611C59FB11D4860405FA09F30236C8`; Settings SHA256 is
`55DBB7ACA3FF1E535E54D7E91F1BEBE2D64E019CAE01139E471419E327EE5891`. No reinstall or direct host/
watchdog launch was used. Final live product, total cost and memory acceptance remain parent gates.


## 2026-10-16: Stationary scrollbar drag scheduling

Clipboard's capsule and Settings' application loop previously treated a held scrollbar as periodic
frame demand. The pointer paths already update offsets directly, set dirty/relayout on movement,
and release capture on up/cancellation. Remove only those stationary flags from the frame predicates;
actual size/smooth-scroll motion, completion/progress notifications and renderer fallback keep their
existing scheduling. Settings installed-app dragging was already event-driven; all five drag families
are checked for consistency. No geometry, rendering command, easing, scroll timing or input policy
changes are included in this milestone.

Ignored `stationary_drag_probe.cpp` includes the actual Settings application implementation and
calls the actual Clipboard capsule pointer path. Its before variant compiles pre-change HEAD source
snapshots for both frame predicates. `probe-stationary-drag-{before,after}-{debug,release}.log` all
pass with zero failures. Both stationary cases return true at 300/300 scheduling opportunities before
and 0/300 after. These are deterministic predicate counts, not measured desktop wakes or CPU/GPU
performance. Move events still change offsets immediately and request repaint; capture/subscription,
release and Clipboard cancellation remain intact. All five Settings drag flags, six Settings smooth
scroll targets, progress/completion/service/fallback wake causes and Clipboard active scrolling/size
animation are checked. No temporary test window or user desktop capture was needed.

`build-stationary-drag-{debug,release}.log` and `test-stationary-drag-{debug,release}.log` record successful
Reach/Settings builds and all forty tests in both configurations. Architecture, formatting and
whitespace checks pass. No tracked UI tests were added. Clipboard/Settings retained sizing and
scrolling and overall live/performance acceptance remain open.


Installed validation for the idle-drag milestone stopped watchdog PID 12248 and awaited exit,
then stopped Reach PID 15588 and awaited exit. Checked Debug Reach/Settings binaries and PDBs were
copied into the existing build folder; startup used only `C:\Dev\reach\build\reachctl --start` from
that folder. Watchdog PID 576 and Reach PID 15328 were verified alive at the build paths, created
2026-10-16 12:44:37 Casablanca (+00:00). Installed Reach SHA256 is
`0C16D4F4299195E21682301E22CFF445147A053BCC7F0BD6BD36181CAE0836DA`; Settings SHA256 is
`FE7A26DF33806152D9704648B4D8B9B1C302B93A5BB2AF20F44EF5DCA330FB62`. No reinstall or direct
Reach/watchdog launch was used.


## 2026-10-16: Shared retained image fitting

The Clipboard width prerequisite now has a shared image-fitting contract. Its adapter uses actual
bitmap dimensions, retains full source pixels once, and changes centered crop or stretch geometry
through existing native group properties. Core command/group/buffer storage is unchanged. A small
extension of the existing pure fitted-source ownership test covers image ownership, constant-axis
snapping, inherited scissor, atomic unsupported-fade rejection and shape-shadow ancestry rejection.
Clipboard's scalar sizing and scrolling are not migrated by this capability milestone.

The ignored `image_fit_probe.inc` uses only its own generated bitmap and input-transparent window.
`probe-image-fit-budget-{debug,release}.log` record 160 displayed source comparisons per configuration:
four DPI values, crop/stretch, five widths and moving/fixed sources on both compositors. Fixed source
pixels differ by at most one channel value; complete CPU fallback pixels match ordinary drawing
exactly. Intermediate raw channel differences reach 94 at a half-pixel edge, while each channel stays
within one value of the 3x3 reference-neighborhood range. Surrounding pixels match exactly. The
125% center-crop pair is inspected in ignored `image-fit-inspection.png`; the continuous edge and
centered crop preserve the picture, with the former rounded-width edge differing within one pixel.
This spatial sampling check is scoped evidence and does not establish whole-product acceptance.

Unchanged active submissions change no raster, property or tree counters. Reversal reuses its image
source. A changed bitmap/aspect invalidates exactly one raster. Independent crop clocks use renderer
sampling without capsule layout and stop at their deadline. A nine-source 2048x2048 stress case falls
back for two sources and retains 120,440,512 nominal surface bytes on each backend, below 128MiB;
all sources, fit ownership and pending sampling retire on clear while the clock is active. These
surface counters do not include every bitmap-cache or driver allocation. Full product memory,
latency, power and CPU/GPU acceptance remain open.

Both final builds and all forty tests pass (`build-image-fit-origin-{debug,release}.log`,
`test-image-fit-origin-{debug,release}.log`). `probe-image-fit-full-final-{debug,release}.log` each
passes five complete native runs per backend plus HWND fallback with zero failures after the
collapsed-image/atomic-parent changes. Final source-origin normalization is isolated to the new image
helper; `probe-image-fit-origin-{debug,release}.log` repeats the complete image matrix using fractional
capture origins and the existing HWND fallback checks. All 164 displayed comparisons per build pass,
including both collapsed axes. Settled error remains at most one, moving neighborhood error one,
and outside error zero. Capture origins are normalized to a physical-pixel basis without moving the
requested destination, preserving bitmap snapping in native and CPU paths. Architecture, formatting
and whitespace checks pass. No tracked UI tests were added, and no installed Reach restart was needed
for an as-yet-unconsumed shared primitive.


## 2026-10-16: Clipboard retained item width

The consumer removes its item-width scalar track and sampled layout-width API. One canonical 160ms
ease-out owns normal 19dp gutter changes, current-width input geometry and one cleanup deadline.
Height and smooth scrolling remain open. Private owned diagnostics exercise real capsule relayout,
exact-time reversal, close hit testing, no width-only frame/relayout, one endpoint cleanup, forced
hide, source retirement and the extremely narrow horizontally clipped image/SVG fallback. Normal
width submissions need no renderer sampling; the narrow fallback ends on the same finite deadline.

The owned producer compares both compositors at 100/125/150/200%, both themes and five progress
positions, with partially visible mixed text/image items and two image-aspect regimes. A separate
maximum-membership wide-glyph fixture preserves native horizontal clipping and the ordinary CPU
fallback's existing unclipped ink. This pre-existing backend difference is not repaired by the
migration. Settled comparisons remain strict; moving rounded borders and glyph clip edges are
measured separately from unaffected pixels. The temporary fixtures are input-transparent and
capture only their own windows and generated image source.

The capsule grows from 16,040 to 16,048 bytes; render command/group/buffer sizes remain unchanged
at 1,032 / 2,248 / 1,018,072 bytes. A controlled four-text-item scene without a blue capture backdrop
retains 2,139,744 nominal surface bytes in the former renderer, 2,188,864 settled after migration,
and 5,216,416 while expanded (16/28/96 producer groups and 21/29/145 native layers). A five-item
width capsule uses 5,222,432 bytes while active and returns to 2,188,864 at cleanup. These are
surface counters, not all bitmap-cache/driver allocations. The finite extra source memory remains
part of the required whole-product CPU/GPU/power/memory/latency acceptance.

Initial Debug command construction is about 0.06ms settled and 0.27ms expanded; first expanded
submission is about 10ms and an identical warm submission about 1.4-1.7ms in the owned scene.
Warm unchanged width submissions alter no raster, property or tree counters; exact-time reversal
rerasterizes no surviving sources. These isolated timings are not repeated product acceptance.


`build-clipboard-width-final-{debug,release}.log` and
`test-clipboard-width-final-{debug,release}.log` pass both builds and all forty tests after the final
fallback behavior change. `probe-clipboard-width-final-{debug,release}.log` and
`probe-clipboard-width-final-wide-{debug,release}.log` each report zero failures, totaling 320
displayed poses. Ordinary native intermediate raw differences reach 143 at filtered edges, with
3x3 channel-neighborhood error at most nine. Wide-glyph clipping reaches 50 on one moving right-edge
column; away from a 1.5px band around those text-width cuts, neighborhood error is at most twelve.
Settled native poses differ by at most three channel values. Complete CPU endpoints match exactly;
intermediate CPU differences reach 28 only in the 1.5px rounded-border sampling band and are zero
outside it in the final fixtures. The ignored side-by-side `clipboard-width-wide-edge-detail.png`
inspects the right-edge coverage rather than widening a global image tolerance. The separate
120,120-case-per-build capacity proof retains exact custom slice selection.

Release construction is about 0.02-0.05ms settled and 0.09-0.12ms expanded; first expanded submission
is about 7ms and an identical warm submission about 0.4-0.6ms in this scene. Architecture, formatting,
whitespace and obsolete-width-symbol checks pass. No tracked UI tests were introduced.


`probe-clipboard-width-full-{debug,release}.log` each passes five complete native runs per backend
plus HWND fallback with zero failures. The isolated `probe-width-contract-{debug,release}.log`
rechecks physical fitted-text alignment at four DPI values/two fonts and both alignments, stable
source reordering/color/font replacement, and the new vertical-ink mode's oversized/capacity
fallback. Both compositors retain 446 layers and 60,848,864 nominal bytes in the fifty-source stress
scene, sampling six text sources within the existing bounds; source-only work ends at the canonical
endpoint and clearing retires every owner. These checks pass in both configurations.


Installed validation stopped watchdog PID 576 and awaited exit, then stopped Reach PID 15328 and
awaited exit. Checked Debug binaries/PDBs were copied into the existing build folder; startup used
only that folder's `reachctl --start`. Watchdog PID 8412 and Reach PID 10304 are alive from
`C:\Dev\reach\build`, created 2026-10-16 18:39:17/18 Casablanca (+00:00), verified afterward.
Installed Reach SHA256 is `DC78C5E3F3EB6219708AED44E31B8C0C5D23C8C31DD0B50BBB821C9C04E02EAB`;
Settings is `A8797B094507C29DD461C4D1CDE299957816C790528EFB713510E20BC0733F5D`.
No reinstall or direct Reach/watchdog launch was used. This verifies installation/lifetime, while
full live interaction and total product acceptance remain open.


## 2026-10-16: Semantic Battery/Bluetooth deadline scheduling

Battery's saver request and Top Bar's Bluetooth absence hold retain their two-second policy
lifetimes through absolute `reach_motion_now_seconds` deadlines published by existing generic
surface geometry. These are CPU semantic timers; the renderer owns no new effect. Frame-delta
accumulation and wait-only frame/update demand are removed. Battery merges the pending timeout with
popup completion, acknowledgement and reset cancel it, and expiry reconciles once. Bluetooth grace
starts on the first observed absence, including a zero-delta status event; repeated absence does
not extend it, and recovery/reset cancel stale work. No timer, polling loop or composition branch
was introduced.

The ignored `semantic_deadline_probe.cpp` owns its capsules and a generated service snapshot; it
performs no OS power/radio actions and shows no windows. Before migration, 100 simulated waiting
updates requested 100 Battery frames and updates and 100 Bluetooth-grace frames, with no semantic
wake deadline (`probe-semantic-deadline-before-debug.log`). Final Debug and Release probes request
zero frames/updates and publish future wakes. A large frame delta does not expire either timer;
after real deadline passage, one reconciliation clears both wakes and further ticks remain idle.
Service acknowledgement, zero-delta absence, repeated absence, recovery and reset pass with zero
failures (`probe-semantic-deadline-after-{debug,release}.log`). This is scoped scheduling evidence,
not a whole-product power/CPU measurement.

Both builds and all forty tests pass (`build-semantic-deadline-{debug,release}.log`,
`test-semantic-deadline-{debug,release}.log`). The existing Battery timeout test now verifies the
absolute wake and lack of recurring requests, then waits for its published deadline instead of
simulating policy time by animation-frame deltas. No new UI test was added. Renderer code and
visual command production are unchanged by this milestone; the preceding full native regressions
remain applicable to the retained primitives. Final architecture/format/whitespace and obsolete
semantic-counter searches pass.


Installed/live follow-up for this semantic milestone is pending. At 2026-10-16 22:04 Casablanca,
read-only enumeration finds no Reach/watchdog/service process, the current-user shell registration
is `explorer.exe`, and no Reach scheduled tasks are registered. The allowed installed-folder
`reachctl --start` therefore reports that Reach is not installed. The stop sequence found no Reach
or watchdog process to terminate; checked binaries were copied into the existing build folder.
No reinstall, shell-registration write or direct Reach/watchdog launch was attempted. Earlier
18:39 width-milestone startup verification remains historical evidence, not a claim about the
current session. Source/build/owned-policy verification is complete; current installation state
must be resolved before live product acceptance can pass.

## 2026-10-16: Settings Power selection colors and independent textbox ink

Power's five option transitions now publish canonical 220ms ease-out intent. Previous/selected
policy still resets each new transition. Retained opaque border/background coverage and label ink
use shared UI selection helpers; custom textbox ink shares the same intent while placeholder,
suffix, selection highlight and caret opacity keep their own paint. The old scalar manager,
per-update sampled Power values and periodic application frame predicate are removed. One earliest
visible endpoint folds the finite source graph back into ordinary paint; hidden Power pages and
minimized Settings windows publish no Power cleanup wake. This is color animation, not row reveal.

The custom background's existing nested mixes contain a quadratic in selection. Nine sampled
values compile into eight segments through the existing canonical hinge/combine operations; no
new timeline or motion descriptor is introduced. Ignored Debug/Release color-mapping probes check
960,096 extreme-color/direction/sample combinations each. Worst approximation is 0.219172 of one
8-bit channel, with at most eight compiled segments and no failures. Unsupported nonopaque paint,
geometry or UI capacity uses ordinary drawing sampled from that same finite intent.

An optional exclusively owned `text_color_group_id` separates textbox glyph coverage from editor
background/selection/placeholder paint. The caret inherits color and its independent opacity;
empty/unpainted parts allocate no raster. CPU fallback samples only textbox text color. Both native
backends share the planner and reuse source pixels across color retargets. The shared 128 MiB/layer
budget can replace duplicate painted editor/ink sources with renderer-owned active-only source
sampling, keeping native caret opacity and avoiding feature layout/command production. The owned
12-editor budget scene retains 20 layers and 128,000,000 nominal bytes while sampling four sources;
source-only sampling stops at the canonical endpoint and hide clears all records. Final independent
textbox probes pass Debug/Release (`probe-textbox-ink-final-{debug,release}.log`): 320 total displayed
poses across four DPI values/two fonts, filled/empty fields, selection/placeholder/caret and ancestor
clip/opacity. CPU reference output is exact. Native differences peak at 8 (WUC) and 12 (DC), confined
to glyph ink; other and locally flat pixels retain the three-channel limit. Independent color/caret
motion, zero initial text alpha, unchanged reuse, retarget, font replacement, suspension, retirement
and resource fallback are covered. Glyph contrast still requires product review.

The complete Power producer is compared with an ignored copy of the former producer, driven by the
same canonical sample time. The matrix includes all five rows, preset-to-preset, preset-to-custom
and custom-to-preset transitions, focused custom selection/caret, five poses, four DPI values, both
themes and both fonts. The final Debug/Release matrices (`probe-power-selection-aligned-debug.log`,
`probe-power-selection-final-release.log`) cover 960 displayed comparisons across both backends:
CPU error at most one channel, native at most 17
inside glyph ink, and other/locally flat pixels within three. Settled constant paint stays within
three. The measured 17-channel pose is separately captured for visual inspection. These are native
comparisons against native former output and separate CPU comparisons against CPU former output;
pre-existing ancestor-opacity differences between backends are not misclassified as migration error.

The first full-producer matrix exposed fractional source padding at 125% DPI, causing a 103-channel
glyph shift. Integral physical-pixel padding fixes the local DWrite phase while preserving unclipped
label overflow. Failing exploratory logs are excluded from final evidence. Remaining small glyph
contrast differences from tinted coverage are explicitly marked R9; they are not declared identical
or accepted solely because an isolated fixture passes.

At 100% DPI the five-row 1100x1500 scene uses 14,575,408 former/settled nominal source bytes and
15,667,336 during preset color motion. The largest tested active scene at 200% uses 18,085,440 versus
15,328,080 former bytes. Active layer count peaks at 83; the ordinary 100% scene uses 38. Values include
the full scene backdrop and measure nominal surfaces, not actual driver allocation. Source-only
waiting performs zero application render/raster/property submissions; unchanged intent and same-source
reversal reuse pixels and the visual tree. Cleanup removes finite bindings and hide retires sources.
The shared command grows from 1032 to 1040 bytes; group stays 2248 and fixed buffer grows by 3968 to
1,022,040 bytes. Shared UI textbox state is 176 bytes.

Debug/Release real Settings scheduler probes report zero requests across 100 active checks and one
220ms endpoint wait; hidden/minimized, final cleanup, repeated cleanup and unsupported-renderer
endpoint behavior pass. Both product builds/all forty test groups pass after the physical-alignment
fix. No tracked UI tests were added. Five full native runs per backend plus HWND fallback pass
in each configuration (`probe-power-selection-full-{debug,release}.log`, zero failures). Final
architecture, formatting, whitespace and obsolete Power-API searches pass. No Reach restart, deployment, reinstall, shell/task
repair or OS power action was performed; live editing/focus/DPI acceptance and total product CPU/GPU,
power, latency and memory cost remain open in review R9 and the broader acceptance gates.
