# Switcher live window previews

Status: implemented and accepted, 2026-10-11. Full isolated build and automated validation pass;
the user reports that the feature works well with no observed regressions.

## Behavior

Switcher presents one entry per window as a horizontal strip of live DWM previews. All previews
share a common height, starting at 125dp, and each width follows its source aspect ratio. Portrait,
landscape, and wide windows retain their proportions without cropping. Monitor constraints and
width animation uniformly fit the visible row; the selected entry remains visible across forward,
backward, and wraparound navigation. Titles and selection chrome sit outside native preview content.

Window ordering, cancellation, ordinary activation, non-activating presentation, and game-mode
behavior remain in the existing capsule/action paths. Minimized windows and unavailable frame
geometry use icon placeholders. Native registration failure leaves a usable titled card without
aborting presentation. No window is restored merely to obtain preview pixels.

## Shared architecture

Switcher and Stage publish sources and placements through `reach_feature_native_overlay_ops`.
The generic presentation runtime reconciles native relationships through the existing
`reach_window_thumbnail_port`; features never own native handles or call DWM. No feature depends
on Stage, and no Switcher-specific host branch was added.

Thumbnail creation now carries an explicit destination. Each runtime releases only its own IDs,
including on destination replacement. Adapter entries have independently allocated stable addresses,
so the Desktop helper's entry pointer survives joins and departures and simultaneous targets do
not compete for a single Stage-sized pool. Adapter destruction releases remaining relationships.
The obsolete mutable target and global surface-teardown operations were removed end to end.

Window tracking caches frame geometry during its existing event-driven refresh and includes geometry
changes in its refresh report. Both Stage and Switcher consume that snapshot. The corrected variable
width requirement therefore needs no new contain-fit option or source-query port: native destination
rectangles already have the source aspect ratio. The app-control service retains its live frame query
for direct window manipulation.

Generic native placement applies client shadow/content offsets and the retained presentation's
translation, centered scale, and opacity using the existing motion clock. Switcher schedules frames
while its native placement must animate and retains relationships through close presentation. Settled
previews remain live through DWM without a capture worker or polling. Stage keeps its own placement,
front-source promotion, Desktop helper, and strict close synchronization.

## Verification

Focused non-UI regressions cover equal heights and distinct source proportions, monitor fitting
during width animation, forward/backward/wraparound selection visibility, minimized/restore/resize
membership, cached geometry without repeated OS reads, two-target ownership and teardown,
destination replacement without a generation change, and presentation coordinate/opacity resolution.
Existing Stage reconciliation and failure-recovery assertions remain covered.

The isolated output is `build/switcher-previews`. The full build, all 39 ctest tests, architecture,
formatting, and whitespace checks pass. No running Reach process was restarted or terminated.
Live acceptance is based on the user's reported testing. Automated coverage provides additional
checks for layout constraints, shared ownership, and presentation transitions.
