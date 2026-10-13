# Compact retained shadow design

Date: 2026-10-13. Status: retained shadow group implemented; shared chrome/consumer integration and acceptance pending.

## Decision

Use one alpha source and a compositor effect graph for shapes whose corners, notch or shadow caps
cannot remain rigid across the complete resize range. Keep the existing fixed-cap retained path
where it covers the complete range. Both paths consume canonical motion; choosing one must not
create another animation clock or introduce a capsule-specific branch in composition.

The first WUC prototype captured a caster, a DropShadow output and an inverse-alpha mask into three
visual surfaces. Replace it with a single caster visual surface and a GaussianBlur → ColorMatrix →
Composite graph. A second surface brush over that same surface carries the shadow offset; the
unshifted brush is the cutout input. This avoids two surfaces and the empirical DropShadow-radius
mapping. DirectComposition uses a shifted caster, ShadowEffect and destination-out composite with
an explicit identity cutter. Both use half Reach's blur metric as standard deviation.

Windows' effect and brush contracts support the required graph and source inputs:
[composition effects](https://learn.microsoft.com/en-us/windows/apps/develop/composition/composition-effects),
[brush combinations](https://learn.microsoft.com/en-us/windows/uwp/composition/composition-brushes),
[composite semantics](https://learn.microsoft.com/en-us/windows/win32/direct2d/composite).
The owned fixture's affine-effect description returned E_INVALIDARG during WUC factory creation;
the supported surface-brush offset path succeeds. Do not generalize that failure to every affine
effect. The public render contract should expose the required shadow semantics, not an arbitrary
platform effect graph.

## Production ownership

- The neutral retained group carries optional existing Reach shadow metrics. A group's source
  subtree supplies alpha; its visible output is the shifted, blurred, colored shadow with the
  unshifted alpha removed. Preserve ordinary ancestor translation, scale, opacity and output clips.
- The adapter owns the caster container, source surface/brush, effect factory/brush or DC graph,
  native output visual and retirement. Child membership, painter order and sources remain under
  the existing retained planner. Shadow parameters invalidate the effect; time alone does not.
- Bound capture/filter extents to the maximum source geometry plus resolved shadow padding. Include
  additional surfaces in resource accounting and limits. Preserve reuse on geometry retargets.
- The fallback uses D2D source/cutout effects with the same event-time geometry. It shares a hardware
  context when possible, otherwise uses WARP and bounded transfer into the HWND target. Shapes are
  rectangles, clipped/notched rounded rectangles, triangles and lines; own paint, output tint,
  nested filters and presentation-root filters are rejected before rendering. Capture/source
  bounds share the existing 128 MiB budget; fallback temporary bitmap bounds are checked separately.
- Shared popup chrome builds body and notch alpha from the same canonical rectangle/radius/notch
  intent as its visible fills and borders. The circular compact clip option is implemented in
  818e85b. Preserve edge notch placement, zero-area collapse and both drop directions.
- Finish Dock/Switcher consumers, DWM destinations, controls, title trimming, runtime demand and
  obsolete-track removal through their full data paths. A renderer helper alone does not complete
  those checklist items.

## Production verification

Production owned-window checks follow canonical child motion without application submissions or
source repaint. Root retargets, material changes, disable/re-enable and reparenting preserve source
identity; empty scenes retire effect ownership and nominal bytes. An oversized capture fails before
allocation and a later valid publication recovers. Both native backends and shared-hardware/WARP
fallback pixels are checked across four DPI scales and both notch directions. Actual HWND capture
also passes after its target DPI was corrected from the observed desktop default 120 to 96.

The source recipe extends its notch one physical pixel inside the body while preserving the exterior
triangle slopes. Separate antialiased masks meeting at a fractional edge otherwise leave a partial
alpha seam and produce an interior shadow. That join belongs in shared chrome; the filter consumes
actual subtree alpha. Header/body/complete consumer appearance still requires its own verification.

The descriptor adds 32 bytes per group: 2,216 → 2,248; buffer 1,006,600 → 1,013,800 (+7,200 / 0.72%).
A Release host-layout fixture exceeded the default 1 MiB stack after this increase. Its exit code was
0xC00000FD. Remaining test-local render buffers now use heap ownership; production buffers already
belong to heap-owned applications. No test behavior was added for that allocation correction.

## Pixel and motion evidence

The private native_capture_probe.inc captures only its own input-transparent diagnostic HWND,
using CreateForWindow and a bounded free-threaded frame-pool wait. It does not capture user apps.
Native pixels are compared with the established cached shadow and an exact full-shape D2D reference
using the same Reach geometry, offset, blur, color and interior cutout.

Static checks cover three widths, no notch and both notch directions, at 100/125/150/200% DPI on
both backends. The initial cached-only tolerance fails two plain DC cases at 150%: four channel
values differ outside the geometry edge band. At those locations, the native graph differs by at
most two from the exact full-shape reference; cached versus exact differs by at most three.
The sized/nine-slice cache introduces a fractional raster phase followed by bitmap interpolation.
That difference must remain visible in the evidence; it is not a native geometry failure or proof
that product appearance is accepted.

Displayed wide → narrow → wide checks cover both notch directions and four DPI values on both
backends. Forty-eight captured states pass their pixel checks with zero application submissions or
retained-source repaints after intent publication. This proves the effect follows animated source
clips/placement without an application frame loop. It does not establish intermediate-frame
presentation latency, complete popup chrome appearance, arbitrary notch anchors or product input
alignment. Color-matrix output must use constant RGB with transformed alpha under premultiplied
mode; that mode unpremultiplies before the matrix and premultiplies afterwards.
See [color-matrix alpha modes](https://learn.microsoft.com/en-us/windows/win32/direct2d/color-matrix).

A further 48 displayed states use RGB (0.2, 0.6, 0.9), alpha 0.45. They pass with the same normalized
coverage bounds: exact-reference errors reach four channel values away from edges, cached-reference
errors five, and edge errors 36. Their contrast against the fixture background is 1.51 times the
black case after opacity. The initial black-specific absolute limits therefore failed; the corrected
fixture scales coverage limits by that contrast, retaining the original black bounds. This verifies
the tested RGB/alpha transform, not every color space or final consumer appearance.

## Bounded workload comparison

Release owned fixture, Windows 11 build 26100, 16 logical processors, RTX 4070. Each backend runs
three six-second phases per method with rotated order. Canonical width repeats between 100 and
20 pixels with 0.18-second travel and 0.4-second endpoint holds; body height is 60 pixels and notch
height 8. Cached repainting submits only changed widths and waits through holds. The graph publishes
one repeat intent and waits. The measured graph capture is 136 × 100 pixels, not the full window.

The synchronized collector covers all twelve phases with 276 valid, fully contained 0.25-second
intervals. The table reports the median of three phase means. CPU includes fixture plus DWM as a
percentage of all logical processors; GPU is their busiest combined selected engine. Power is whole
package/board data, not process attribution.

| Backend / method | CPU % | GPU engine % | Package W | Board W |
| --- | ---: | ---: | ---: | ---: |
| WUC cached repaint | 0.6342 | 0.9068 | 33.7445 | 23.7892 |
| WUC single-source graph | 0.6284 | 0.7116 | 33.7581 | 23.6523 |
| DC cached repaint | 0.6454 | 0.7340 | 32.5577 | 25.6884 |
| DC single-source graph | 0.4925 | 0.6759 | 32.8723 | 22.3983 |

Each cached phase performs 70 submissions and 70 retained raster passes. Every graph phase performs
zero of both. Recorded drawing sources are 54,400 bytes for cached repaint versus 21,376 for the
graph. The WUC visual surface has a further nominal 54,400 bytes; graph sources total 75,776 nominal
bytes before unreported effect/driver storage. Cached shadow bitmaps add 179,376–187,664 bytes in
this capture. These are source estimates, not total GPU allocation measurements.

The results support pursuing the shared capability, not final efficiency acceptance. CPU/GPU phase
ranges overlap; package means span 32.37–38.18W and board means 22.38–27.28W. Process private-memory
means span roughly 66–72MB across backend recreation and caching. Background activity and power
variation prevent attribution of small differences. Repeating holds can differ from finite product
motion; the fixture omits DWM thumbnail placement, text, complete chrome and user input. Cold setup,
large popup sizes, finite retargets, idle retirement, latency and final controlled product CPU/GPU/
power/memory acceptance remain open.

Private evidence: shadow-graph-phases.jsonl, shadow-graph-comparison-system.jsonl,
shadow-graph-comparison-summary.json, shadow-graph-native.log, probe-shadow-displayed-animation.log
and probe-shadow-displayed-color-coverage.log
under build/gpu-composition-migration. An earlier collector started late; its shadow-graph-system.jsonl
is excluded from the table. Initial reference runs that redirected only command_target are also
excluded because shadow drawing uses d2d_context.
