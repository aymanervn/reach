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

- Features draw with semantic roles in `reach_theme` (`include/reach/core/theme.h`): `surface` (base, raised, card, item, well), `text` (primary, secondary, tertiary, quaternary, inverse), `fill` (primary for tracks, secondary for controls, tertiary for disabled controls), `border`, `divider`, `on_color`, `danger`, `pressed_overlay`, `artwork_scrim`, `status`, `accent` and `shadow`. A feature gets its own consumer-named token (`<consumer>_<element>[_<state>]_<role>`) only when no role fits; never add one that equals a role.
- Border-bearing backgrounds are opaque; Now Playing keeps its own translucency.
- The color scheme adapts to fixed text, never the reverse. Text, glyph and knob colors keep their tokens on every background. Contrast is fixed by `reach_theme_resolve`: `accent.fill` at ≥4.5:1 with `text.primary`, `accent.text` at ≥4.5:1 on the elevated surface. New color sources feed the same resolver.
- Every color is authored in OKLCH in `tools/theme_palette.py`, which generates `src/core/theme_palette.h`; the architecture check fails when the header is stale. Features, apps and adapters take colors only from `reach_theme` tokens and never write literal colors, alphas or mix amounts. White tint bases for retained color feedback and image modulation are not colors.
- Grays share one tint (`NEUTRAL_TINT`) and come only from the per-mode steps in `NEUTRALS` (`tools/theme_palette.py`): surface base, raised and elevated, plus divider and an opaque border. Accents never tint grays.
- Text, glyphs and placeholders use one base per mode (`DARK_TEXT`, `LIGHT_TEXT`) at a strength from `TEXT_STRENGTHS`: primary, secondary, tertiary (placeholders, disabled), quaternary. Accent contrast judges translucent text as drawn over the fill (`reach_theme_contrast_met`).
- Areas behind content use `FILL` at a per-mode strength from `FILL_STRENGTHS` (`fill` roles). Shapes that read like glyphs (thumbs, dots, muted fills, toggle-off tracks, small dividers) use text strengths. Knobs are pure white.
- Colors come from the nine families in `HUE_FAMILIES`, each with one hue and deep, bright and ink levels. Fills use deep in dark mode and bright in light mode; colored marks the reverse; small colored text in light mode uses ink. Red buttons with white glyphs stay deep in both modes.
- Accent washes use one of `ACCENT_STRENGTHS` (faint, tint, selection, muted) on the resolved accent fill or text; `reach_theme_resolve` derives the `accent` roles, and controls take any blend amount from the theme (`accent.tint_amount`, passed as `reach_ui_selection_item_style.selected_tint`).
- Palette values are hand-tuned and stored in OKLCH; tint and hue stay shared, and lightness steps are chosen by eye, never generated by formula. Output stays 8-bit sRGB.
- Glass bounds keep primary text at ≥4.5:1 and secondary text at ≥3:1 (`reach_theme_glass_limit`). Tertiary and quaternary text (disabled, placeholder, hints) are excluded.

## Shapes and text

- A rounded rectangle inside another uses the outer radius minus the gap between their edges (including borders), clamped at zero: `reach_nested_radius` in `include/reach/core/geometry.h`, from the actual inset. Never pick an independent radius token for nested shapes. Half-height capsules are not nested corners.
- Every rounded border uses the pixel-aligned opaque outer fill plus inset inner fill (`reach_render_push_bordered_background`). There is no rounded-rectangle stroke command.
- `reach_theme.border_thickness` is a DP metric. Padding and content metrics describe the inner box; outer size and placement account for both border sides (`reach_theme_border_content_rect`).
- All UI text uses the tokens in `include/reach/core/typography.h`: xsmall 10, small 11, medium 13, large 15, xlarge 20, heading 22 (dp). Weight and color express hierarchy; features never use numeric text sizes.
