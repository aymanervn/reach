# GPU resize and reveal

Status: fixed-cap chrome and animated group insets implemented; complete feature consumers and the
remaining compact-shape/notched-width capabilities are pending. This does not close the migration
capability or final performance gates.

## Implemented mechanism

Resize eligible chrome through retained patches over a fixed raster source. Keep content in its own
unscaled groups, using existing movement, opacity, tint and viewport descriptors for reveal.
`reach_render_push_stretched_chrome` partitions a source into at most nine ordered groups. The four
corners keep their pixels, straight edges stretch only along their length and the center stretches
on both axes. No text, icons or live thumbnails enter the stretched source.

`reach_render_group::scale` supplies both axes unless `scale_y` is explicitly published. The same
absolute motion describes compositor execution, fallback, interruption and geometry sampling.
Root completion includes the vertical axis; hit testing, native input regions and preview
projection use one sample time, and native envelopes accommodate both axes.

The stretch descriptor owns dimension curves, fixed source insets and normalized X/Y anchors.
Left/top anchors are 0, centers are 0.5 and right/bottom anchors are 1. Dimension changes derive each
patch's displacement and scale, preserving the declared anchor through interrupted growth or
shrink. Stable IDs occupy the source ID through source ID + 8. Empty edge/corner patches are skipped.

Physical source coordinates, dimensions and insets must be integer pixels. Scale DPI metrics and
resolve pixel-aligned source chrome before publishing it; do not treat these as fixed DP values.
Destination motion can be fractional. Derive endpoint sizes and constraints from current physical
layout and preserve the feature's final border alignment.

The source is rasterized once per content/theme/font/DPI/source-geometry change. Width/height-only
intent submits native property changes over those cached crops. Existing raster comparison,
clip/tint ownership, retirement, painter order and renderer memory limits remain canonical. Source
patch area totals the original source area; there are up to nine native layers and finite property
curves, not nine full-size copies. CPU owns intent, constraints and input sampling.

This uses the existing retained renderer on both backends instead of introducing separate brush,
atlas, effect or clock systems. The additional node and submission cost is bounded, with shared
benefit for multiple surfaces. That architectural rationale still requires final product cost
measurements; fewer raster operations alone do not prove a total GPU/CPU benefit.

## Validation and limits

Source-command validation accepts rounded/notched fills and shadows. A moving axis must keep enough
fixed pixels for every corner and its shadow extent. Source and destination centers remain positive;
unsupported compact sizes are rejected instead of squeezing corners. Horizontal notch stretching is
rejected; vertical resize keeps the fixed notch, its corner geometry and its shadow.

The helper validates capacity, source geometry and stable-ID collisions before appending anything.
It copies each command with its source crop, intersecting existing scissors, and retains any outer
group viewport/tint/opacity. A moving source placement or additional source scaling is rejected;
compose surface motion through the existing presentation contract and derive the resize anchor from
resolved feature placement.

The HWND fallback samples the same finite geometry only while motion is active. It retains existing
fallback renderer behavior; the primitive does not add hardware shadows to a renderer lacking that
capability.

Small shapes whose caps would overlap need an appropriate shared native-geometry mechanism or a
measured, specifically justified CPU responsibility. Horizontal notch motion needs fixed notch
geometry and anchored/clamped positioning; the current nine-patch helper cannot stretch it safely.
Complete popup integration must also preserve monitor clamping, envelope/viewport geometry and
input alignment. Root presentation clips remain unsupported. Do not assume these remaining cases
are solved by independent scale alone.

## Evidence and next consumers

Focused pure geometry checks cover all nine patches, centers/bottom anchors, uniform inheritance,
independent Y override, compact-size rejection and exact unit scale at 100/125/150/200% DPI.
Debug builds and all 39 tests pass. Bounded native growth/reversal checks verify unchanged source
rasters and descriptor reuse on Windows Composition and DirectComposition, plus finite fallback.
Root checks verify independent Y deadlines/retarget and existing completion/failure ownership.

GPU D2D readback compares the shared sampled patch execution with direct endpoint geometry for
plain, top-notched and bottom-notched chrome at all four DPI scales. Corners, borders and shadows
match within two channel levels. This is offscreen geometry evidence, not displayed WUC/DC pixels
or full product performance. A 150% DPI check initially caught a non-unit constant axis caused by
affine cancellation; direct normalized endpoint calculation fixed it without relaxing tolerance.

First integrate Quick Settings height/sections, whose ordinary popup geometry can preserve fixed
caps and an unchanged-width notch. Remove its scalar/tick/render paths as the whole consumer moves.
Preserve independently moving section viewports and content, following-row placement and event-time
input. Launcher, Clipboard, Settings, bar controls and native-preview surfaces follow with their
specific compact, notch, constraint and lifetime requirements; do not copy feature implementations.
