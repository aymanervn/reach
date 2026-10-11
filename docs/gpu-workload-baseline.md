# GPU composition migration: workload baseline

Status: counters verified; full product performance baseline and acceptance pending.
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
the cause of the changed WUC result remains unresolved. This is an open completion-verification
finding, not evidence of native displayed-pixel or latency acceptance.

Stage close-hover diagnostics settle geometry and tile hover, then enter and reverse the close
feedback through public pointer input. Five runs per compositor verify two executions/four tint
property submissions, zero raster passes and zero scalar advances. Capsule ticks request no work
for this feedback; theme RGB/alpha endpoints, settled pointer input and game-mode retirement pass.
The HWND fallback samples three frames, including the endpoint. This scoped run explicitly skips
the previously failing WUC root-completion check with local diagnostic
`REACH_PROBE_SKIP_WUC_ROOT`; it does not resolve that failure or final product acceptance.

Further isolation of the WUC completion finding measured root submission at about 0.05 ms, before
the 50 ms curve deadline. An independent finite property-set batch on the same compositor also
delivered no callback during a 300 ms hold. The cause remains unresolved; no production completion
policy was changed based on this hidden-window result. A follow-up 8px visible, nonactivating
diagnostic target reproduced the missing callback, so visibility alone does not explain it.
Agile delegates enqueue and execute on the owned/compositor dispatcher, and an explicit
`RequestCommitAsync` reports completion. These narrow checks rule out those failed hypotheses;
they do not identify the root cause or establish displayed-animation correctness.

The counters give a reproducible baseline for content reuse versus sampled visual changes. Full
application wake/layout/native metrics, cold/warm workload cost, total CPU/GPU activity, latency,
power, memory and instrumentation overhead still require product scenarios and system traces.

## Baseline prerequisites still open

- The isolated optimized MSVC build reaches an internal compiler error `C1001` in the existing
  `src/adapters/windows/window_management/reach_service_shared_state_win32.cpp` (aggregate
  initialization sites around 396/621). Explicit `/EHsc` removed initial exception warnings but
  did not resolve the optimizer failure. An explicit byte-clear rewrite with compile-time layout
  guards also failed at the same optimizer location; that unsuccessful source change was removed.
  A subsequent isolated `/O2 /Ob1` build still failed at line 396. Standard `/Ob2` flags were
  restored in the isolated cache; no compiler workaround was added to product source.
  Debug Reach and Settings compile and all 39 tests pass.
  Resolve optimized-build verification separately before claiming production performance results.
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
