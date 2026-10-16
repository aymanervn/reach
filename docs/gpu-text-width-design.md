# Retained text during width fitting

Status: shared capability and Dock/Switcher consumers implemented. Complete live/product
appearance and total-cost acceptance remain pending.

Dock card titles and the selected Switcher label are laid out at the fitted card width. Preserving
only a moving clip cuts glyphs rather than reproducing DirectWrite character trimming. Centered
text also changes its alignment origin continuously. Scaling a pretrimmed label or fading between
endpoint labels changes existing appearance and is outside the selected approach.

## Selected source representation

DirectWrite retains CPU text shaping and trimming. At content/font/envelope changes, obtain cluster
advances and ellipsis metrics from the same format/layout used for painting. Candidate trimming
boundaries are cumulative cluster advances plus ellipsis width, with the full untrimmed metric
width included so trailing whitespace does not delay the final state. Between boundaries the
rendered glyph output is stable after subtracting its leading/center/trailing alignment anchor.

The private `text_width_probe.cpp` records actual glyph-run identities and normalized origins over
144 font/DPI/alignment/string cases, including Arabic, CJK, emoji, ligatures and trailing spaces.
Its first normalization used the wrong DirectWrite alignment enum order; that run is excluded.
Adding the untrimmed metric width resolves the trailing-space boundary omission. The corrected
`text-width-layout-release.log` reports 4,236 states, zero missing boundaries and zero origin drift
across the sampled intervals. This establishes a bounded recipe, not production pixel acceptance.
The public [cluster metrics](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/ns-dwrite-dwrite_cluster_metrics)
and [text-layout rendering contract](https://learn.microsoft.com/en-us/windows/win32/api/dwrite/nn-dwrite-idwritetextlayout)
provide the layout data and renderer callbacks used by that diagnostic.

The prototype packs seven layout states into each retained raster block. Every block has a
stationary single-line clip. Canonical width selects a row through discrete Y translation; other
blocks move outside their clips. Center/trailing anchors use ordinary affine X translation. An
outer viewport follows the current width. Text is never scaled. State rows share source identity
across finite retargets; retirement releases the blocks. The native planner owns this source
expansion, analogous to its existing textbox caret parts; features publish only a generic
text/animated-viewport relationship.

## Canonical step mapping

`reach_motion_lookup` projects a finite monotone source map into up to eight threshold changes,
using the existing motion storage, epoch, generation and native segment compiler. This adds no
clock, descriptor fields, timer or native execution system. Mapping/clamp thresholds resolve into
the source domain; affine output changes preserve crossing times. Compilation emits constant
pieces with explicit next values, using the existing WUC final-step and DC constant-cubic paths.
Exact crossing time selects the following state along traversal, including decreasing motion.
Unsupported decreasing source maps, nested lookup, repetition, excess capacity and ambiguous
continuous/discrete combinations reject before mutation. A constant output owns no activity.

Focused non-UI checks cover both directions/all easings, delayed submission, mapped/clamped source
geometry, range extrema with equal endpoints, visited/unvisited crossings, interruption, affine
values, capacity, atomic rejection and exact midpoint submission. Both complete builds and all
forty Debug/Release test groups pass before the last validation-only additions. Both complete native regressions pass. Complete feature consumers and product resource/performance
acceptance remain pending.

## Appearance and cost gates

Initial integer-translation owned checks passed on both backends with nonempty glyph captures,
source/tree reuse, no motion-only application frames/rasters and complete retirement. The expanded
fractional-position matrix fails its legacy cached-text bound: centered glyph-edge differences
reach over 100 channel values. The side-by-side `title-atlas-inspection.png` shows fractional bitmap
filtering; this run is not accepted or silently assigned a larger tolerance. An independent native
translation of each selected glyph state matches within one channel value, distinguishing state
selection from fractional bitmap filtering. Flat legacy interiors/exteriors match exactly. The
production static path uses one ordinary source, matching its reference directly. Moving source
checks keep the strict two-channel native-reference limit.

Prototype source bytes include all precomputed rows: the small owned scene reports roughly
0.5--1.34 MiB including its 320 by 220 background. This is more than one ordinary label source.
The production design must bound preparation and source memory, account for all simultaneous
labels and failure/recreation, and compare cold/finite/repeated product workloads before choosing
the final GPU/CPU disposition. Reuse the renderer's 128 MiB aggregate source budget and 16,384-pixel
dimension limit. Do not claim a product efficiency win from fewer application frames alone.


## Ownership and invalidation

`reach_render_push_width_text` associates one ordinary TEXT command with a dedicated rectangular
viewport. Its right inset carries canonical width; left/top/bottom insets remain zero. Source
bounds and clip equal the text envelope, and the full motion range must fit that envelope. Core
preflight rejects missing/colliding owners, ambiguous source sharing and unsupported geometry.
The helper forces ordinary ellipsis behavior. CPU fallback resolves the same layout width at the
shared frame timestamp.

The native planner expands this relationship into its own raster blocks. Stable owner namespaces
keep source IDs independent of command order; hiding, source removal and font recreation retire
namespaces with their visuals. Preparation uses a bounded 32-entry text-layout recipe cache and
the existing format/layout cache. Static fields use one ordinary source; active fitting prepares
only blocks that can become visible or were already retained. Same-envelope retargets reuse those
sources and their tree. A command-level scissor wraps the complete output before internal alignment
and row translation. Source dimensions, aggregate nominal bytes and native layer counts retain the
existing renderer bounds. Over-budget or unsupported curves use ordinary text raster content.

The renderer reports its active source-sampling demand through the generic render port and owns
the corresponding source records. Both application loops can sample those records directly when
there is no other dirty work. This updates only the necessary text raster, with no capsule arrange,
command production, native property submission or tree rebuild. One final sample clears demand;
source retirement and suspension clear the records. The canonical width remains the only clock.
The existing HWND fallback still samples normal command-buffer motion. This does not give any
remaining CPU path final product-cost acceptance.

The first production matrix exposed a DirectComposition inheritance bug: a static text source's
nearest-neighbor interpolation survived when its visual became an empty parent. New atlas children
inherited it. Retiring that raster now restores inherited interpolation mode, matching the
[documented inheritance contract](https://learn.microsoft.com/en-us/windows/win32/api/dcomp/nf-dcomp-idcompositionvisual-setbitmapinterpolationmode). The corrected Debug
production matrix passes 288 static comparisons and displayed forward/reverse checks on both
backends, two production font modes and four DPI values. It also checks nonempty glyph captures,
source/tree reuse, ownership retirement and the aggregate-budget fallback. The expanded prototype
Release comparison passes against independent native translation with at most one channel value;
flat legacy interiors/exteriors have zero error. Fractional centered legacy glyph-edge differences
reach 133 and remain an explicit filtering/appearance consideration. The earlier four 'nonempty'
failures were an invalid >20-pixel oracle: the tested clipped ellipsis contains 18 visible pixels.
The corrected check requires positive actual glyph coverage and preserves the strict pixel checks.

The new source flag grows a command from 1,024 to 1,032 bytes and the fixed buffer from 1,013,800
to 1,017,768 bytes (+3,968). Motion/group storage remains 152/2,248 bytes. Render operations add two
callbacks (16 bytes). Recipe/source-record/native object, atlas and driver costs still need the
complete consumer comparison. Debug/Release builds, all forty test groups, complete native regressions and installed startup
validation pass. Consumer integration and final product acceptance remain pending.

## Consumer integration constraints

Dock should use one canonical layout progress owner for width and membership reflow. Derive width
from its sampled current width to the new target, then derive constrained popup/body/notch intent
and row fit from that epoch. Fit remains clamp((width - twice border/padding) / raw row width, 0, 1).
Each card keeps aspect-derived width and the same fitted preview height. Combine its base fitted X
with an affine survivor correction that starts at the sampled old global position and ends at zero;
clamp against the moving viewport through the existing same-epoch min/max operations. Capture
interruption at event time, not the previous raster frame. Source and constraint changes remain
CPU layout events.

Use a stable render envelope as Quick Settings already does. Body placement, cards, chrome, title
viewports and controls publish retained descriptors inside it. Native destinations add the envelope
origin to the same card curves. Input samples those rectangles at event time; visible/hold bounds
sample the body, while input presentation receives the canonical body rectangle. Preserve paging,
source identity, notch direction/monitor constraints, close freeze and one endpoint cleanup deadline.
Remove the width scalar manager, ticks, sampled-render fields and width-driven frame demand.

Card fill/border geometry must preserve border thickness under fitting, header corner radius and
compact control/icon proportions. Shared animated clip/fill or rigid-cap primitives should carry
that geometry; never scale the entire card's text, border and controls as one bitmap. Title layout
width is card width minus the clamped close width and padding/gap. Switcher uses the same text
relationship for centered labels, with its independent 125dp nominal preview height and rounded
non-notched chrome. Both complete consumers and total-cost/appearance gates stay open.


Final staging enforces the existing byte/layer limits after each source range is prepared, bounding
temporary source-command expansion before later fields are considered. Normal scenes with no
width-text sources keep their prior planner path. The final bounded source checks rerun reorder,
color/font/scissor, source-only terminal sampling and both capacity conditions in Debug/Release.
The final broad renderer suites already pass five runs per compositor plus HWND fallback in both
builds; the staging refinement changes only preparation of the new width-text source family.


## Final shared-capability verification

Both builds and forty test groups pass after the final staged-bound refinement:
`test-text-width-staging-{debug,release}.log`. Formatting, architecture and whitespace pass.
`probe-text-width-full-{debug,release}.log` reports zero failures for five runs per native backend
plus HWND fallback. `probe-text-width-final-{debug,release}.log` verifies production sources;
`probe-text-width-bounds-{debug,release}.log` repeats final allocation/ownership/source-only paths.
These owned diagnostics are private evidence, not new tracked UI tests or completed consumers.

The forced fifty-title scene ends with 446 native layers and 55,942,400 nominal source bytes,
using six sampled text sources; both existing limits remain intact. Color replacement repaints
sources without restarting native intent. Font recreation preserves the width epoch/generation.
Command reorder preserves source IDs and rasters. Whole-output scissoring matches the independent
native reference; source-only capacity sampling performs no layout/command/property/tree work and
clears its demand at the final sample. Empty scenes retire every layer, source record and owner map.

Installed validation on 2026-10-15 stops watchdog PID 12840 and waits, then stops Reach PID 17900
and waits. Checked Debug binaries replace the installed copies, and the existing installed-folder
`reachctl --start` owns startup. Later desktop enumeration verifies watchdog PID 17284 and Reach
PID 12588 from `C:\Dev\reach\build`. Reach SHA256 is
`A629E1AF34ACF741D52E4304DBA2AF14D0D402A447CA8AC662E1B24B3E437D77`; Settings SHA256 is
`E2E71B6724AE7528D5FB8DAEF09332CE565AFC7CBCC070D5F03BC4DCEF9D9064`.
Startup/lifetime evidence does not establish full displayed interaction or product efficiency.


## First fitting consumer

Switcher now consumes the shared text contract through centered label viewports parented to its
canonical card/body geometry. Growing-card producer captures at four DPI scales and both themes
preserve logical width, size, truncation and alignment on both native renderers. Flat regions match
the established static renderer exactly; fractional glyph edges have the inspected filtering
difference recorded in the workload baseline. The scalar width path is removed; one endpoint
cleanup switches settled titles back to ordinary sources. Dock also consumes the contract for
leading titles and independent close-control fitting. Final displayed live/product-cost acceptance
remains open.


Settled consumer sources preserve physical raster origin under fractional fixed translations;
independent displayed text comparisons match exactly across both fonts, alignments and four DPI
scales. Moving unchanged raster content can reuse that origin until the next legitimate settled
submission. The controlled sixteen-card Dock case reaches the existing byte bound with one sampled
title source and verifies source-only updates without layout/command/property/tree work. See the
workload baseline for producer, source-limit and paging evidence; complete live/product acceptance
remains open.
