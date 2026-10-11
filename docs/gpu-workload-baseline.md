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

The counters give a reproducible baseline for content reuse versus sampled visual changes. Full
application wake/layout/native metrics, cold/warm workload cost, total CPU/GPU activity, latency,
power, memory and instrumentation overhead still require product scenarios and system traces.

## Baseline prerequisites still open

- The isolated optimized MSVC build reaches an internal compiler error `C1001` in the existing
  `src/adapters/windows/window_management/reach_service_shared_state_win32.cpp` (aggregate
  initialization sites around 396/621). Explicit `/EHsc` removed initial exception warnings but
  did not resolve the optimizer failure. Debug Reach and Settings compile and all 39 tests pass.
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
