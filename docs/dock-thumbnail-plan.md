# Dock instance thumbnails

Status: implemented, 2026-10-11. Live appearance and mixed-monitor DPI checks remain pending.

## Presentation

Replace the Dock hover popup's vertical title rows with one horizontal row of window previews.
Keep one card per supplied instance, including the single-window case. All preview rectangles share
one height; each width is that height multiplied by the source frame's aspect ratio. Preserve the
whole visible window frame without stretching or cropping.

Start at **140dp preview height**. This gives a 16:9 window an approximately 249dp-wide preview
and a 1:2 portrait window a 70dp-wide preview. Use 12dp inner padding, 12dp between cards, an 8dp
gap between header and preview, and 16dp screen-edge clearance. Keep the existing popup notch and
theme border/radius/shadow tokens. The popup should remain centered over its Dock owner when space
allows, shifting inside the monitor near either edge.

Each card has a 24dp header above its preview with an ellipsized window title and a close control.
Use the shared medium typography token, reserve the close control's space independently of hover,
and keep the existing hover emphasis and activation/close semantics. Preview width determines
card width; long titles never widen or distort the preview. Constrained narrow headers prioritize
the close control and ellipsize the title. Place every control and highlight outside the native
preview rectangle, since DWM content composites above the popup's drawn content.

Show an icon placeholder using the icon service for minimized windows or unavailable geometry.
Use valid cached source proportions when available, otherwise a square placeholder. Never restore
a window to obtain pixels. Preserve best-effort native registration behavior and usable titles and
actions if a source closes or DWM cannot register it.

## Fitting and overflow

Keep the native DP size when it fits. Available space is derived from the current hosting monitor,
Dock anchor, popup direction, screen clearance, border, notch, header, and padding. Reduce the
common preview height only for vertical constraints or a single source too wide for the available
row. Every preview retains its aspect ratio and common height after fitting.

When more cards exist than fit horizontally, page through complete cards. Add a compact 24dp
navigation footer with previous/next controls and a visible range indicator, only when overflow
exists; reserve that footer before resolving the vertical fit. Mouse wheel advances or retreats
through the same range. Keep card height stable between settled pages and make every supplied window
reachable. Avoid partially visible DWM previews: the renderer's scissor does not clip native
thumbnail relationships. Publish only the current page's preview sources and placements.

The Dock currently carries up to 16 instances, but the hover route and Context Menu truncate them
to the command menu's 8-item capacity. Give window-preview state the full window payload capacity
(`REACH_MENU_MAX_WINDOWS`) independently of command-menu capacity. This feature does not increase
the Dock's existing 16-instance model limit.

## Ownership and reuse

The current path is Dock hover -> semantic `reach_menu_request` -> interfeature route -> Context
Menu's window-list mode. Keep that capsule and its popup activation, anchoring, pointer capture,
dismissal, and close lifecycle. Implement preview layout/rendering as a private window-preview
component within Context Menu, distinct from its ordinary command-row presentation. Dock never
knows the popup implementation, and the popup never reads Dock internals.

Attach window tracking and the icon service through the existing feature registry dependency seam.
Read frame geometry, minimized state, and current titles from window tracking's event-driven cache.
No new OS query or capture service is needed. Register the popup's read-only preview list through
`reach_feature_native_overlay_ops`, reusing `reach_window_thumbnail_port` and the generic native
presentation runtime already used by Stage and Switcher. Native handles remain runtime-owned.

Extract only the shared pure geometry operations from Switcher into a common preview-layout
helper: valid aspect/fallback resolution, common-height fitting, and packing a contiguous range of
variable-width items. Inputs are geometry and constraints; outputs are dimensions/range/row width.
Each capsule keeps its own metrics, chrome, interaction, anchor, and navigation policy. Switcher
still ensures that its keyboard-selected window is visible; the Dock popup uses explicit paging.
Do not couple the features or move their behavior into composition.

One arranged card model supplies thumbnail placements, drawn chrome, card/header/close hit regions,
and page controls. Advance native membership generation only when visible source membership or
eligibility changes. Geometry and hover changes reuse surviving relationships. Preserve native
previews through close presentation and request frames while their shared transition is active;
settled content remains live through DWM without polling.

## Refresh and input

Keep the existing Dock-to-popup pointer corridor and global event-driven pointer observation.
Reanchor must update the retained anchor button, bar edge, direction, hover margin, and observed
hold bounds from the same current layout context as the cards. Today reanchor moves the popup but
the hold region still reads the opening request's anchor and opening DPI-scaled margin.

Window-change notifications refresh source geometry, minimized eligibility, and titles, remove
departed windows, and close an empty popup. Dock model changes republish the hovered owner's
window payload through the existing public callback seam. Updating the same owner's payload must
preserve its open transition rather than reopening the popup; changing owner uses the existing
replacement contract. An owner that disappears closes its popup. Preserve or clamp the current
page after departures. Reconcile pressed targets by window identity so a shifted card cannot
activate or close another window.

Right-click command menus and the power menu retain their existing row presentation. Remove the
superseded title-list width measurement, vertical instance scrolling, and duplicate window-entry
conversion once the preview mode is connected end to end. Command-menu scrolling remains separate.

## Resolution and DPI, including Switcher

All proposed metrics are logical DP values. Convert once with the hosting layout context's current
DPI scale; compute preview width from the dimensionless source aspect ratio. Native placement,
drawn content, hit regions, notch, border, and pointer corridor consume the same resolved geometry.
Do not size from raw resolution alone or from the source app's DPI. Use physical hosting-monitor
bounds for fitting, and retain floating-point geometry until native placement rounds to pixels.

| Windows scaling | Dock nominal preview height | Switcher nominal preview height |
| --- | ---: | ---: |
| 100% | 140px | 125px |
| 125% | 175px | 156.25px |
| 150% | 210px | 187.5px |
| 200% | 280px | 250px |

Actual heights may be reduced to fit the available space. At 4K/200%, logical size remains the
same as at 1080p/100%; changing resolution without changing DPI preserves native DP size unless
constraints require fitting. Do not shrink an entire multi-window collection just to show every
instance at once.

Switcher already starts at 125dp, scales its chrome with the resolved DPI, bounds its layout to the
monitor, preserves source ratios, and fits the animated width. Its current tests cover nominal
layout and a constrained 600x360/200% monitor. No confirmed sizing defect warrants changing its
accepted default. Keep 125dp and its navigation/animation behavior while migrating only the shared
geometry calculation. Add fractional-DPI and resolution coverage for the common helper and verify
Switcher remains aligned during display changes and width animation.

The host already invalidates monitor/layout/surface geometry on display and DPI notifications.
Use that path for both consumers. Update the popup's native-transition frame demand and report
card/placement changes even when the popup's outer bounds are unchanged. Monitor selection remains
the existing hosting-monitor policy. The local analysis has an older open finding about the monitor
adapter's DPI source; verify the host-provided scale on mixed-DPI displays rather than introducing
feature-specific DPI calls or assuming that finding has been resolved.

## Implementation sequence

1. Extract and validate the shared pure preview geometry while retaining Switcher behavior.
2. Introduce the private popup card/range model, full 16-window payload handling, DP metrics,
   monitor/anchor fitting, and complete-card paging.
3. Add the popup's public preview publication, registry service attachment/notifications, and
   native-overlay declaration using the existing port and runtime.
4. Connect card/header/close/page interaction, live source and owner payload refresh, and current
   anchor/DPI pointer observation. Preserve the popup's activation and dismissal contracts.
5. Remove the old instance-row path and update architecture documentation to the implemented state.

## Verification

Use a small number of focused non-UI tests for shared geometry/range packing and source-membership
reconciliation: equal heights, portrait/wide aspect ratios, fractional DPI, constrained monitors,
all 16 entries reachable, page clamping after a departure, and stable surviving source IDs.
Retain existing Switcher, Stage, popup lifecycle, and shared ownership coverage. Build in an
isolated output, run the full test suite and architecture/formatting/whitespace checks.

Live checks: one and several instances; left/right Dock edges; minimized/restore/resize/close;
page navigation; pointer travel between Dock and popup; same-owner refresh and rapid owner changes;
opening/closing native alignment; resolution or scaling changes while open; 100/125/150/200% scaling
at 720p, 1080p, 1440p, and 4K; mixed-DPI displays and offset monitor origins; Switcher Alt-Tab,
game mode, and overlap with Stage. Do not terminate a running Reach instance without the user's
explicit authorization for that termination.

## Implementation verification

Implemented the shared geometry helper, private card layout/rendering, native publication,
full-capacity paging, source/owner refresh, and current pointer observation. Removed the old
title-width measurement port from Context Menu, duplicate entry conversion, and instance-row
presentation. Icon-retention notifications protect visible fallback icons while the popup is idle.

The isolated `build/switcher-previews` build succeeds. All 39 tests pass on the final full run;
the architecture, formatting, and whitespace checks pass. Two focused non-UI layout/membership
tests cover source proportions, common height, every instance reachable, fractional DPI,
offset/constrained monitors, minimize/restore/resize, same-owner additions, last-page departures,
and current-anchor pointer retention. Switcher coverage includes 125/150/200% scaling.

An earlier full run exposed an unrelated timing-sensitive App Control test assertion; its focused
rerun and the final full run pass. Recorded the finding in local analysis without changing it.
Live appearance and the mixed-monitor DPI-source check remain pending. The running Reach instance
has not been terminated or replaced.

The follow-up width animation uses the existing animation manager, with a 180ms ease-in-out
track for source, owner and page width changes. While expanding into a wider row, all previews
and gaps uniformly fit the current width; source proportions and equal preview heights remain
intact, returning to the nominal height when settled. Drawn controls and popup height retain their
resolved sizes. See `docs/dock-popup-width-animation-plan.md` for the implemented scope.
