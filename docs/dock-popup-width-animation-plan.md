# Dock preview popup width animation

Status: implemented, 2026-10-11. Uses the existing animation manager; compositor migration is deferred.

## Behavior

Animate width changes while the hover preview popup is presented: owner replacement, source
additions/departures, source-aspect changes, and page changes that produce a different row width.
The private width track uses the same 180ms ease-in-out timing as Switcher. Retarget from the
track's current value during rapid changes. Initial presentation uses its target width with the
existing shared opening fade/translation. Closing freezes width for the existing close transition.
Force close, reset, and entry into ordinary command/power mode retire the width track.

The animation manager owns the only width timeline. Context Menu reads its current value during
preview arrangement, places that width with the shared popup anchor/clamping helper, and publishes
one card model for rendered content, native placements and hit regions. Existing finite frame demand
advances the track; no new timer, animation driver, or compositor capability is introduced.

Compute page membership using the target layout rather than the intermediate animated width. This
avoids repeated page repacking and native relationship churn during motion. Fit the entire current
row into the animated inner width with the shared pure preview row-scale helper. Apply one scale to
preview widths/heights and gaps, preserving source proportions and equal preview heights. Center
fitted previews vertically in the resolved preview area. Headers, text, close controls, border,
notch, and popup height retain their resolved sizes; narrow headers prioritize the close control.
Settled previews return to their nominal 140dp height unless monitor constraints require fitting.

Pointer retention follows the current placed bounds and anchor through the existing observation
hook. Source identity still guards press/release activation against owner and membership changes.
Monitor or DPI changes cut to their newly constrained layout, avoiding stale physical-pixel width
or content outside the new monitor. Ordinary command and power menu rows retain their presentation.

## Reuse and deferred work

Use `reach_animation_manager` rather than adding another animation abstraction. Share only the
pure row-fit operation with Switcher; both capsules retain their own width and navigation policy.
Native thumbnail handles remain owned by the generic runtime and surviving sources are reused.

Commit `03f952b` added retained X/Y/opacity/uniform-scale motion, while width-dependent layout and
native geometry remained hybrid. This feature does not extend that contract or change the renderer,
thumbnail port, native adapter, or animation manager. A later GPU migration can address horizontal
resize, stretchable chrome, and DWM synchronization as separate shared-capability work. A fully
compositor-driven width transition is not claimed here.

## Verification

The isolated `build/switcher-previews` build succeeds; all 39 tests, the architecture checker,
formatting, and whitespace checks pass. Focused non-UI coverage checks intermediate
width, reversal and repeated retarget continuity, complete preview fitting, preserved aspect ratios,
common height, stable native membership during geometry changes, nominal settled height, close
freezing, fresh-presentation reset, and constrained monitor/DPI changes. Existing preview paging and
DPI checks settle the animation before checking final geometry. Switcher coverage remains intact.

Live animation appearance, native alignment, and mixed-monitor DPI checks remain pending. The
running Reach instance has not been terminated or replaced.
