# GPU composition migration: live-preview design

Status: supported capability selected and adapter operation checks complete; consumer migration,
live visual verification and total workload acceptance remain pending.

Dock, Switcher and Stage retain the shared `reach_window_thumbnail_port`. DWM owns live source
pixels. Retained rendering owns Reach's chrome, controls and eligible visual properties. Native
placement samples canonical absolute-time motion only while necessary; event-time samples also
serve input and interrupted transitions. No second capture service or animation implementation is
introduced.

## Evidence and limits

The public [DWM thumbnail properties](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/ns-dwmapi-dwm_thumbnail_properties)
contain immediate destination/source rectangles, opacity and visibility. The
[update API](https://learn.microsoft.com/en-us/windows/win32/api/dwmapi/nf-dwmapi-dwmupdatethumbnailproperties)
applies those values to a registered relationship; it exposes no curve or retained-visual binding.
This is the supported capability used by Reach's adapter. It does not imply DWM source pixels
are rendered on the CPU.

[CreateSurfaceFromHwnd](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositiondevice-createsurfacefromhwnd)
wraps a layered window's rasterization. Its source must remain layered, and source destruction or
an empty/offscreen window stops output. It is not a general export of arbitrary source-window DWM
thumbnails into Reach's composition tree. Changing another app's window styles is unacceptable
for this migration. Windows capture would require different source eligibility, protection,
minimization, frame-delivery and resource semantics; introducing that parallel pipeline has no
established benefit for this feature and is outside the selected implementation.

The bounded native diagnostic uses only its own hidden source and destination windows. Five runs
verify both existing thumbnail planes, three simultaneous relationships from one source, stable
port IDs, 300 unchanged placement requests with zero DWM updates, subpixel changes that quantize
to the same destination with zero updates, and a changed pixel destination with one update.
Painter-order replacement registers/releases one native handle while preserving the public ID
and cached placement. Destroying the source rejects subsequent cropped placement and replacement;
relationships can still be released, and stale IDs reject double release.

Diagnostic source and raw evidence are local ignored files under
`build/gpu-composition-migration`: `thumbnail_probe.cpp`, `run_thumbnail_probe.ps1`,
`thumbnail-capability.jsonl` and `thumbnail.thumbnail.json`. Hidden API checks do not verify live
pixels, minimized/protected sources, displayed ordering or total CPU/GPU cost. Those remain in the
acceptance matrix; this design decision does not close that gate.

## Consumer implementation requirements

- Preserve the current eligibility and relationship policy, stable source IDs, simultaneous
  targets, painter order, source-loss handling and ownership-based release.
- Publish endpoint layout and canonical property descriptors from capsules. The generic runtime
  samples native placement at one physical timestamp; controls, borders and live previews follow
  that timeline. Do not implement capsule-specific branches in composition.
- Keep native synchronization distinct from content invalidation. Advancing thumbnail geometry
  alone must not rebuild unchanged commands or raster content, or repeatedly arrange endpoint
  layout. Rendering a changed title, source, theme, DPI or constraints remains legitimate work.
- Stop native animation frame demand at the endpoint and when hidden. Unchanged static previews
  receive updates only on real events. Adapter property caching remains the final defense against
  redundant DWM submissions, rather than permission to call the adapter indefinitely.
- Keep active-only CPU geometry for DWM destinations, cropping, opacity and necessary HWND
  movement. Record this CPU disposition explicitly for preview-linked effects. Retain GPU-driven
  independent feedback, reveal, backdrop and chrome whenever the shared capabilities support it.
- Stage's existing backdrop fade is coupled to the desktop preview host: its native-overlay item
  supplies `background.a`, and the behind-target DWM adapter applies whole-window alpha through
  `SetLayeredWindowAttributes`. It is not a separately drawn Stage background. Migrate its timing
  and invalidation with the active-only native path, preserving host/source ordering and the
  synchronized reveal; do not infer independent GPU opacity from the former ledger entry.
- Preserve Stage close/landing synchronization and cancellation. CPU/native completion is semantic
  work; it must not be replaced by a visual-only deadline that activates the source too early.
- Account for integer destination and byte-opacity quantization. Native updates and compositor
  commits are asynchronous APIs; one absolute curve does not promise atomic cross-API presentation.
  Verify alignment under interruption and refresh variation before accepting consumers.

Final acceptance compares Reach/Settings plus DWM cost, latency, power and resource lifetime. If
that evidence demonstrates a material regression, revise the implementation before closing the
checklist; this capability selection alone is not an efficiency claim.
