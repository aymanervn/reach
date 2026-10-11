# GPU resize and reveal

Status: fixed-cap chrome, bounded curves, viewport/content hierarchy and animated group insets implemented; complete feature consumers and the
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

## Bounded reveal curves

`reach_motion_clamp` bounds a derived curve without changing its owner's start, easing, generation
or duration. Affine mapping carries and reverses its limits when needed. This preserves section
reveal's delayed fade and early saturation: opacity is `clamp((progress - 0.10) / 0.65, 0, 1)`;
its slide is derived from that bounded opacity, rather than raw progress.

`reach_motion_compile_at` emits at most four cubic/constant segments, split at the easing midpoint
and limit crossings. Both native backends consume that shared compiler; the old adapter-local
compiler is removed. Native final keyframes use bounded endpoints. CPU fallback/input samples the
same descriptor, and normalized fixed-cap scale carries its bounds without losing exact unit scale.
Visual saturation does not shorten the owner's completion deadline. Starting or retargeting creates
a new ordinary intent; a retarget samples the current bounded visible value and clears derived limits.

Pure tests compare all three easings in both directions, mid-flight submission, constant clamps,
negative affine mapping, retarget continuity and native polynomials against CPU samples. Native
submission/reversal diagnostics cover movement, opacity and clip edges on both compositors and
finite HWND fallback. Complete section viewport/content integration and displayed compositor pixels
remain pending; this curve capability alone does not migrate Quick Settings.

## Viewport and content hierarchy

A render group can declare `parent_id` without introducing another renderer or animation owner.
Every group's source geometry remains in the same feature coordinate space. Its content transform
moves its own raster and descendants; its clip remains stationary relative to that group's parent.
An outer content viewport, following-row movement and an inner reveal viewport can therefore nest,
while leaf content slides independently. `reach_render_command_buffer_group_transform_at`,
`reach_render_command_buffer_group_clip_at` and `reach_render_command_buffer_group_opacity_at`
provide the matching event-time CPU geometry and inherited opacity.

Both backends retain the hierarchy using existing container/content visuals. Empty parent nodes
allocate no drawing surface. Raster content is compared and cached independently of parent motion;
reparenting updates native ownership and coordinate origins without repainting unchanged sources.
A parent can acquire or retire its own raster while retaining children. Native Multiply opacity
applies inherited alpha to individual retained surfaces; consumers must still partition overlapping
paint when preserving per-command alpha, rather than flattening an entire reveal into one bitmap.
DirectComposition's [opacity modes](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual2-setopacitymode)
and [child ordering](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-addvisual)
define those semantics. Windows Composition uses ordinary container/sprite visuals, with no
flattening LayerVisual introduced.

Parents must be declared before children with finite positive bounds. Subtrees occupy contiguous
painter-order ranges; a parent's own raster precedes its children. Validation rejects cycles,
missing owners, repeated/interleaved ranges and parent coverage tint before either renderer mutates
native ownership. Tint remains a leaf coverage capability. Root presentation cannot have a parent.
`reach_render_command_buffer_check_group` preflights individual declarations and fixed-cap batches,
so invalid parent references cannot silently append ungrouped chrome commands.

This capability supports independent viewport/content motion. Complete section paint partitioning,
popup geometry/monitor constraints, deadline reconciliation and input integration remain consumer
work, and displayed compositor pixels remain part of final acceptance.

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
