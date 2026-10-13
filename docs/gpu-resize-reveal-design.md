# GPU resize and reveal

Status: fixed-cap and focal chrome, bounded curves, viewport/content hierarchy and animated group insets implemented; complete feature consumers and the
remaining compact-shape/notched-width capabilities are pending. This does not close the migration
capability or final performance gates.

## Implemented mechanism

Resize eligible chrome through retained patches over a fixed raster source. Keep content in its own
unscaled groups, using existing movement, opacity, tint and viewport descriptors for reveal.
`reach_render_push_stretched_chrome` partitions a source into nine ordered groups, or fifteen when
an independent fixed-width focal band is declared. The four
corners keep their pixels, straight edges stretch only along their length and the center stretches
on both axes. No text, icons or live thumbnails enter the stretched source.

`reach_render_group::scale` supplies both axes unless `scale_y` is explicitly published. The same
absolute motion describes compositor execution, fallback, interruption and geometry sampling.
Root completion includes the vertical axis; hit testing, native input regions and preview
projection use one sample time, and native envelopes accommodate both axes.

The stretch descriptor owns dimension curves, fixed source insets and normalized X/Y anchors.
Left/top anchors are 0, centers are 0.5 and right/bottom anchors are 1. Dimension changes derive each
patch's displacement and scale, preserving the declared anchor through interrupted growth or
shrink. Stable IDs occupy the source ID through source ID + 8, or + 14 for focal chrome.
`reach_render_stretch_patch_count` exposes that allocation. Empty edge/corner patches are skipped.

Optional integer `focal_left`/`focal_right` source boundaries retain a notch or another rigid feature.
`focal_x` describes its destination center relative to the resized source's left edge. The five
columns keep both corners and the focal band rigid while stretching the two straight shoulders.
An unchanged height can use zero top/bottom caps, producing only five actual groups. A notch held
entirely inside a near/far cap can instead use the ordinary three-column layout. Validation includes
its shadow footprint. Overlapping independent caps or nonpositive shoulder ranges remain rejected.

Physical source coordinates, dimensions and insets must be integer pixels. Scale DPI metrics and
resolve pixel-aligned source chrome before publishing it; do not treat these as fixed DP values.
Destination motion can be fractional. Derive endpoint sizes and constraints from current physical
layout and preserve the feature's final border alignment.

The source is rasterized once per content/theme/font/DPI/source-geometry change. Width/height-only
intent submits native property changes over those cached crops. Existing raster comparison,
clip/tint ownership, retirement, painter order and renderer memory limits remain canonical. Source
patch area totals the original source area; there are up to fifteen native layers and finite property
curves, rather than full-source copies. CPU owns intent, constraints and input sampling.

This uses the existing retained renderer on both backends instead of introducing separate brush,
atlas, effect or clock systems. The additional node and submission cost is bounded, with shared
benefit for multiple surfaces. That architectural rationale still requires final product cost
measurements; fewer raster operations alone do not prove a total GPU/CPU benefit.

## Bounded reveal curves

`reach_motion_clamp` bounds a derived curve without changing its owner's start, easing, generation
or duration. Affine mapping carries and reverses its limits when needed. This preserves section
reveal's delayed fade and early saturation: opacity is `clamp((progress - 0.10) / 0.65, 0, 1)`;
its slide is derived from that bounded opacity, rather than raw progress.

`reach_motion_compile_at` emits at most eight cubic/constant segments, split at the easing midpoint,
mapping center and limit crossings. Both native backends consume that shared compiler; the old adapter-local
compiler is removed. Native final keyframes use bounded endpoints. CPU fallback/input samples the
same descriptor, and normalized fixed-cap scale carries its bounds without losing exact unit scale.
Visual saturation does not shorten the owner's completion deadline. Starting or retargeting creates
a new ordinary intent; a retarget samples the current bounded visible value and clears derived limits.

`reach_motion_hinge` derives two slopes meeting at one center from ordinary canonical intent.
`reach_motion_peak` uses that representation for a bounded tent. `reach_motion_combine` adds affine
terms sharing the same clock, easing and generation; constants need no common clock. Active input
clamps and incompatible mapped breakpoints are rejected instead of approximated. `reach_motion_range`
includes the interior extremum and output limits; resize validation and scale normalization use it.

`reach_popup_width_motion` derives monitor-clamped body bounds and notch position from one width
epoch. Its piecewise placement preserves existing `reach_popup_place` geometry at either monitor
edge, outside anchors, tiny widths and both drop directions. It uses physical margin and DPI metrics;
rendering, native placement and input consumers still need end-to-end migration.

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

## Animated rounded viewports

Rounded group clips support canonical inset motion on both backends. The shared
`reach_render_group_clip_motion` derives origin and nonnegative width/height from paired insets.
Each axis must combine exactly through the existing same-epoch motion contract; incompatible
epochs, active paired clamps or breakpoints are rejected before native mutation. Different axes
can own independent epochs. Ordinary rectangular clips retain independent edge motion.

WUC animates geometric-clip offset, size and corner radii. DC animates rectangle-clip edges and
radii. Each radius is limited to half its current axis size, preserving compact rounded geometry
without scaling child text or icons. Clip disable, replacement and repetition retirement stop the
native bindings. Existing descriptor activity/fallback sampling supplies lifetime and deadlines;
no property set, timer, source buffer or animation owner is added. Rounded root clips remain unsupported.

Pure geometry checks cover reversal, all finite easings, paired insets, collapsed extents and invalid
epochs. Owned native checks cover compact motion/reversal, settled deduplication, repetition and
retirement. Offscreen fallback/cached coverage matches the former static viewport at six widths
and four DPI scales. A direct rounded primitive has different edge antialiasing from the established
viewport mask; the inspected comparison records this as a consumer appearance consideration.
This solves animated rounded clipping, not compact notched shadows or complete popup width fitting.

## Validation and limits

Source-command validation accepts rounded/notched fills and shadows. A moving axis must keep enough
fixed pixels for every corner and its shadow extent. Source and destination centers remain positive;
unsupported compact sizes are rejected instead of squeezing corners. Horizontal notch resize
requires the notch and its shadow to fit a rigid focal band or edge cap; vertical resize keeps the
fixed notch, its corner geometry and its shadow.

The helper validates capacity, source geometry and stable-ID collisions before appending anything.
It copies each command with its source crop, intersecting existing scissors, and retains any outer
group viewport/tint/opacity. A moving source placement or additional source scaling is rejected;
compose surface motion through the existing presentation contract and derive the resize anchor from
resolved feature placement.

The HWND fallback samples the same finite geometry only while motion is active. It retains existing
fallback renderer behavior; the primitive does not add hardware shadows to a renderer lacking that
capability.

Small shapes whose caps would overlap need an appropriate shared native-geometry mechanism or a
measured, specifically justified CPU responsibility. Horizontal notch motion uses fixed focal or
combined edge-cap geometry with anchored/clamped positioning. Transitions across overlapping-cap
regimes and compact animated chrome remain unresolved.
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

## Quick Settings section consumer

Output-device and app-volume sections use canonical retained progress. Following controls have an
independent movement parent; each section retains its own clip and shared delayed/saturated fade
and four-DP slide. Individual command leaves preserve panel, slider-track/fill/thumb and glyph paint
order/alpha without duplicating curves per leaf. Semantic ID ranges preserve unchanged title and
chevron sources when section content retires. The output-row press overlay keeps its independent
viewport and feedback semantics. Input samples progress and slide at one event time, excludes
transparent/clipped regions and constrains slider starts to the visible content area. Cleanup wakes
at one section deadline; no section-only scalar advancement or periodic frame demand remains.

Source composition matches the previous renderer's endpoints at 100/125/150/200% DPI within one
channel level; partially clipped saturated reveals and geometry-AA-off controls stay within two.
Intermediate commands match geometry, clip rectangles and color intent. Bitmap filtering during
fractional movement differs from rerasterizing vector edges at each position; this is recorded
separately from geometry/endpoint checks. Baseline-compensated text sampling avoids the observed
half-pixel text softness at 125% DPI. The bounded intermediate comparison and manual offscreen
inspection retain the ordinary linear filter for shape edges. This does not establish displayed
WUC/DC pixels, all fonts/themes, high-refresh behavior or final total-product efficiency.

## Quick Settings height and input consumer

Height and both section progress descriptors derive from one event-time layout curve. A stable
session envelope grows on intent when required and stays allocated through collapse/reversal;
ordinary motion does not resize the native window every frame. Shared popup placement derives
monitor-clamped body Y and height, while retained fixed-cap chrome preserves the border, corners,
shadow and unchanged-width notch. A body parent moves the chrome and viewport together; the viewport
shrinks its bottom clip and an independent scroll parent moves the cached content. Constant ordinary
heights keep the same chrome source patches. A compact constant body uses the existing static shape;
animated custom geometry below the cap limits remains a pending generic capability.

Scroll maximum derives from the same section/height epoch. Wheel steps remain input policy and
change its upper clamp directly. Changes in either panel's source extent retarget the layout even
when the collapsed target height is unchanged, preventing cached hit geometry from diverging from
the published scroll descriptor. Reopening resets both the offset and its descriptor. DPI, monitor,
anchor, direction and border changes reconcile the placement and cut obsolete geometry.

Capsules publish an optional neutral `reach_rect_motion` for their input bounds. Generic composition
uses one timestamp for root inverse presentation and rectangle sampling. It also projects the same
motion into native client space; Win32 hit testing samples it without a window-update timer.
Nonretained publication uses a valid static input region and clears obsolete motion when absent.

Focused native diagnostics cover positive scroll, collapse/reversal, both exclusive panels, source
changes during collapse, reopening, compact monitors, environment changes, host event conversion,
client input publication/reset and Win32 hit testing across four DPI values. Chrome source readback
matches the ordinary direct drawing within one channel value for both notch directions and compact
static sizes. Collapse/reversal submits five intent/deadline frames with zero raster passes on both
compositors; fallback draws the same descriptors. These checks do not establish displayed compositor
pixels or full product cost. The broader resize/reveal and product acceptance gates stay open.

## Launcher integration requirements

The current Launcher starts at `layout.search_box.height` with a corner radius of half that height
(`launcher_render.cpp`). The collapsed body is exactly a pill; its corner/shadow caps do not leave
an independent stretch band. The existing fixed-cap helper requires a positive center and enough
space to isolate shadow/corner pixels, so applying it directly would distort or reject the opening
geometry. Resolve compact rounded chrome with shadow on both supported backends before claiming
an end-to-end Launcher resize migration. Preserve border thickness and source proportions.

Launcher also publishes a root opening scale while its visible body height follows results
expansion (`launcher_state.cpp`). Its current root scale center therefore changes with body height.
A stable retained envelope must preserve this physical pivot during overlapping open/result
motions, through a supported animated pivot or another verified compositional construction; fixing
it to the full target height would change appearance and event projection. Geometry, native input
and root inverse sampling must share that solution. The result fade/slide, row clipping, scrolling,
error rows, caret deadlines and source changes remain part of the complete consumer migration.
These are confirmed requirements, not a selected new renderer mechanism or completed consumer.
