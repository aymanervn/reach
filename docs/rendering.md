# Rendering

## GPU and CPU

- Every surface defaults to retained GPU-composited presentation (`retained_presentation` in `include/reach/composition/surface_runtime.h`). A surface stays on the CPU path only for a measured performance reason or a confirmed bug; record that reason in `docs/pending-work.md`, plus a workaround comment for a bug.
- Prefer compositor-driven visual animation when appearance holds and total cost, latency, power and memory do not regress. Small gains count when the change is simple or helps several consumers.
- Layout decisions, input policy and native operations stay on the CPU. Each CPU-driven visual animation needs an explicit reason.
- A CPU path chosen for sync with CPU-placed content, exact pixels or rarity is design, not debt. Stage tiles are the model case. The Top Bar reveal slide is the same case: it moves pushed app windows each frame (`reach_top_bar_bar_position_frame`), so it stays one CPU path even when nothing is pushed. Top Bar pill widths and all Dock motion have no such reason and belong on the GPU.
- Reuse `reach_motion`, retained render groups and the presentation/native-overlay contracts. Never add a parallel animation mechanism.

## Motion

- On a composed or mapped motion (`reach_motion_affine` on a mapped motion, `reach_motion_combine`, `reach_motion_min`/`max`, `reach_motion_clamp`, layout-progress channels), `.from`/`.to` are the underlying progress, not the output. Read the settled value with `reach_motion_value_at(m, reach_motion_deadline(m))`. Use `.to` only on plain motions.
- Structurally fixed positions come from metrics, DPI and page only. Never derive them from data or runtime size, clamp them, or default them to 0 and update them later.

## Scrolling

- Every scrollable view uses `features/common/scroll_view`. `reach_scroll_view` owns the offset motion, wheel/drag/glide policy, scrollbar and retained-or-CPU presentation. Features never keep their own scroll motion or redraw per frame to scroll.
- Windowed lists rebuild rows on scroll events over the motion's full `reach_motion_range` and re-evaluate hover when scrolling settles. `reach_scroll_view_value_at` is for event-time hit testing and row windows, not per-frame drawing.
- The only exception is the Settings app, which keeps CPU-sampled scrolling. Never copy that pattern into a shell surface.
- Keep scroll fixes scoped to the feature the user names. The user will later build a renderer-backed scroll container; do not start that rework.

## Theme

- Surface background tokens name their consumer even when values match. Border-bearing backgrounds are opaque; Now Playing keeps its own translucency (`reach_theme` in `include/reach/core/theme.h`).
- The color scheme adapts to fixed text, never the reverse. Text, glyph and knob colors keep their tokens on every background. Contrast is fixed by `reach_theme_resolve`: `accent` fill at ≥4.5:1 with `primary_text`, `accent_text` at ≥4.5:1 on the raised surface. New color sources feed the same resolver.

## Shapes and text

- A rounded rectangle inside another uses the outer radius minus the gap between their edges (including borders), clamped at zero: `reach_nested_radius` in `include/reach/core/geometry.h`, from the actual inset. Never pick an independent radius token for nested shapes. Half-height capsules are not nested corners.
- Every rounded border uses the pixel-aligned opaque outer fill plus inset inner fill (`reach_render_push_bordered_background`). There is no rounded-rectangle stroke command.
- `reach_theme.border_thickness` is a DP metric. Padding and content metrics describe the inner box; outer size and placement account for both border sides (`reach_theme_border_content_rect`).
- All UI text uses the tokens in `include/reach/core/typography.h`: xsmall 10, small 11, medium 13, large 15, xlarge 20, heading 22 (dp). Weight and color express hierarchy; features never use numeric text sizes.
