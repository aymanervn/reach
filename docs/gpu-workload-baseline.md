# GPU composition migration: workload baseline

Status: counters verified; full product performance baseline and acceptance pending.

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

Canonical two-slope mappings generalize the existing peak without enlarging `reach_motion`.
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

Live startup uses the registered shell path `C:\Dev\reach\build\reach.exe --launch` and the matching
watchdog. The validation host was observed alive with its launch session held open. Computer Use
returned no targetable Reach surface at that observation; a completed launcher command alone had
not established durable child lifetime. No displayed-pixel/input acceptance is claimed. Restart
order and installation-folder reuse are recorded in the checklist; this scope does not reinstall
or change shell registration.


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
