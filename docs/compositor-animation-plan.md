# Compositor animation migration plan

Date: 2026-10-10. Status: migration implemented; code checks and live acceptance completed on
2026-10-11. Performance measurements remain pending.

The subsequent implementation request defined completion as implementing the shared mechanism,
migrating the listed consumers, removing superseded mechanisms, checking architecture, and compiling.
It also prohibited adding tests. Existing tests were adapted to the new contracts; no tests were
added. The user confirmed all items work. CPU/GPU traces and input-latency measurements remain
unmeasured.
The measurement checklist below is retained for that follow-up and is not evidence of a performance
result.

Improve responsiveness and reduce CPU work by retaining drawn content and animating visual
properties in the Windows compositor. Validate total GPU cost and memory alongside CPU savings.
Preserve current motion, interaction, and lifecycle behavior unless a product change is explicitly
approved. Findings and source evidence are in `docs/repo-analysis.md`, under the 2026-10-10
animation entry and the resolved 2026-09-16 Now Playing marquee entry.

Scope includes Now Playing artwork and width transitions, Top Bar tray dragging/reordering,
Dock dragging/reordering, System HUD, the Settings page selection indicator, shared surface
transitions, and Launcher's presentation fade/translation/scale. Bar reveal with native window
push, Stage native-thumbnail choreography, and layout-changing expansion remain hybrid: optimize
their rendering where the shared infrastructure applies, but retain CPU geometry/synchronization.
Other Settings controls and repository-wide cleanup are outside this migration.

1. Establish a baseline and acceptance measurements.

   Record the active renderer, monitor refresh, DPI, power mode, visible item count, and hardware.
   Measure Reach and Settings CPU time, wakeups, layout calls, command production, actual draws,
   swap-chain presents, compositor-only commits, visual count, retained surface bytes, and cache
   hits/misses. Instrumentation should be opt-in and aggregate rather than emit a log each frame.
   Use Windows performance traces for input-to-presentation latency and Reach plus DWM GPU work;
   compositor work must not disappear from the accounting just because Reach CPU falls.

   Capture idle with/without marquee, Now Playing appearance/disappearance, both bars' reorder and
   release snaps, a held stationary drag, repeated Settings page selection, each shared popup,
   Launcher, and System HUD with repeated actions. Repeat at 60 Hz and the available high-refresh
   rate, with battery saver on/off. Use an integrated GPU when available; record unavailable
   hardware scenarios rather than claim they passed.

   Success means compositor-only motion causes no periodic application draw/layout/present,
   a steady HUD hold and stationary drag cause no animation-driven continuous frames, CPU work
   falls in affected scenarios, input latency does not regress, and total GPU frame time/power
   does not materially regress beyond repeat-run variation. Retained memory must have an enforced
   bound and stabilize after repeated open/close, reordering, and page changes.

2. Add one shared retained-visual contract and finish the scheduling path.

   Extend the platform-neutral render command model and renderer port to support ordered groups
   of commands with stable identities, content revisions, local bounds, clipping, parent groups,
   and independent position/scale/opacity. Separate content invalidation, visual-property changes,
   and native/layout geometry changes. A property transaction must commit without entering the
   full-window clear/draw/present path. Group identity must survive item reorder; array index is
   insufficient. Changing geometry must not automatically restart unrelated animations.

   Add finite and repeating animation descriptions with start/target, duration, easing, generation,
   and cancellation/retarget semantics. Preserve existing easing curves; the current marquee has
   its own curve and must not silently change. Keep the existing Now Playing animated-text command
   during infrastructure development, then migrate its retained storage into the canonical group
   ownership and remove the specialized duplicate ownership in that migration.

   Implement equivalent group operations in Windows.UI.Composition and the existing DirectComposition
   backend used by Settings/fallback. Preserve the static marquee behavior of renderers without
   composition; define a capability-based fallback for finite motion rather than duplicating feature
   implementations. Any necessary CPU fallback belongs in the shared presentation layer and requests
   frames only for its duration.

   Features keep animation intent, visibility, identity, gesture/model policy, and semantic actions.
   The adapter owns native visuals, surfaces, animation objects, and native completion events.
   Generic surface runtime routes completion by identity/generation and owns no feature-specific
   animation branches. Settings consumes the same rendering and timing contracts in its own runtime.

   Add a generic monotonic deadline contract for lifecycle holds and finite-animation fallback.
   Both event loops wait for the earliest existing idle deadline, feature deadline, or native event.
   A due deadline must trigger a state update even when no surface requests animation frames.
   Do not use accumulated per-frame delta for a HUD hold: the application can now sleep through it.
   Native completion events are used where supported; the DirectComposition path uses a one-shot
   completion/reconciliation mechanism tied to submitted timing and commit state. Elapsed duration
   alone is not proof that a frame was presented. Ignore stale completions after retarget or hide.

   Keep hit testing and pointer transforms consistent with animated presentation. Sampling animation
   state for a pointer event or retarget must not require a permanent CPU animation tick. Validate
   rapid interruption against actual compositor timing, rather than assume CPU and compositor clocks
   start on the same frame. Cancellation, close completion, and resource disposal have explicit paths.

   Use tightly sized surfaces and a small number of visual groups. Preserve painter order by splitting
   static command runs around retained groups where needed. Invalidate for content, DPI, font, theme,
   asset replacement, and backend lifetime changes. Bound caches, detach hidden groups, stop inactive
   repetitions, and release retired content. Reuse existing icon/shadow caches where valid. Existing
   general graphics-device-loss recovery remains a separately recorded renderer task; this migration
   must correctly retire and recreate its own resources within supported backend lifecycle paths.

3. Migrate System HUD and the Settings selector as the first consumers.

   System HUD draws its body on presentation/value change and lets the compositor animate opacity.
   Replace its always-open frame request with deadlines and completion delivery. Preserve the complete
   1.5-second lifetime from the most recent successful action, with closing fade inside that deadline,
   game-mode visibility, pointer transparency, and continuous retargeting after another action.
   Completion or deadline hides the surface and detaches its visuals; stale events cannot hide a newer HUD.

   Settings switches content and destination tint immediately, then moves one retained rounded
   selection highlight using its existing 220 ms timing. Put the highlight above the sidebar background
   and below icons/labels; a simple overlay above the whole-window bitmap cannot preserve that order.
   Remove navigation-track ticking and navigation-only whole-window dirty/layout work. Resize, theme,
   DPI, rapid page changes, and independent async page updates must work during motion.

   These consumers exercise both Windows renderer paths, finite completion, and ordered composition
   before the more complex bar migrations.

4. Migrate draggable icons in Top Bar and Dock through the same item-group mechanism.

   Retain one small visual group per visible item, keyed by tray identity or Dock application/order key.
   Include the relevant icon chrome, fallback glyph, running indicator, and feedback in the appropriate
   ordered group; redraw or update those when their content changes. Keep each bar's background separate.

   Pointer events update the dragged group's position directly. Coalesce redundant visual commits without
   adding smoothing lag to pointer tracking. CPU still owns drag threshold, capture, grab offset,
   clamping, reorder threshold, model updates, and persistence. The dragged group remains above neighbours.
   Crossing a slot retargets displaced groups from their current displayed positions; releasing runs
   the existing 120 ms snap. New drags, cancellation, item disappearance, and order updates retire or
   retarget the correct group, with no jump or activation after dragging begins.

   Remove gesture-tracking-only frame requests, CPU reorder/snap ticks, and bar-wide redraws caused
   solely by item movement. Preserve Dock pin persistence and Top Bar's process-local tray order.
   Dock insertion/removal slot reveal, capacity fitting, and width changes remain a separate geometry
   path; simple reorder must not trigger that path unnecessarily. Validate largest supported item counts.

5. Optimize Now Playing and shared drawing resources.

   Preserve the compositor marquee, clipping, text appearance, timing, hidden behavior, and restart
   behavior on track changes. Separate its content key from transform/clip changes so unrelated bar
   motion does not regenerate text or restart the loop.

   Add a bounded cache for processed cover artwork, keyed by image generation, output/crop geometry,
   blur, contrast, DPI, and relevant styling. Cache the processed pixels, not just effect objects.
   Reuse the shared shadow cache and add bounded text-format/layout caching for unchanged text.
   Clear dependent entries on icon retirement, font/DPI changes, and backend lifetime changes.

   For the width transition, compute required CPU layout and neighbour positions while reusing retained
   content. Animate or update group bounds, clips, and positions where they preserve appearance. Keep
   text/icon aspect ratios and corner radii correct; scaling the complete control is not an acceptable
   replacement for width changes. When a width-dependent crop truly changes, rebake only affected
   artwork at required geometry, not every unrelated bar element. Count rebakes to assess the benefit.

   Preserve the current adaptive/battery frame policy for remaining CPU animations. Compositor cadence
   is independently managed by Windows; do not claim that Reach's 60 Hz cap controls it. Verify available
   platform rate controls before promising parity. Any proposed battery-saver reduction of continuous
   marquee motion needs a separate explicit product decision; do not change visible behavior silently.

6. Migrate shared surface presentation and Launcher.

   Migrate the common transition used by Quick Settings, Battery, Context Menu, tray overflow,
   Clipboard, and Switcher. Apply translation and opacity to a correctly ordered presentation group,
   including its background, chrome, shadow, and live backdrop handling. Use a stable native envelope
   where needed to avoid clipping compositor translation; keep native input regions and hit transforms
   consistent. Retarget open/close without restarting from an endpoint and hide only after close completion.

   Migrate Launcher's fade, translation, and scale to the same presentation capability, retaining its
   maximum envelope and live search/text editing updates. Render changes to content when they occur,
   while presentation motion itself causes only compositor work. Check raster text quality across scale
   and settle; redraw at the final size if necessary without a visible jump.

   Content height/width animations can still need CPU layout, but should reuse unaffected groups. Native
   bar position/window push and Stage thumbnail placement continue through their verified synchronization
   paths. Use the measurements to identify unnecessary draws there; do not replace live HWND/DWM work
   with snapshots or independent timelines as part of this migration.

7. Verify each migration and remove superseded paths.

   Use dedicated commits for infrastructure and each complete consumer migration. Remove obsolete CPU
   tracks, frame predicates, rendering branches, and duplicate retained ownership as each consumer lands.
   Search for stale paths and verify both Windows backends. Keep architecture composition generic and
   update `docs/architecture.md` only after the implementation establishes the new behavior. Update the
   findings inventory as issues are resolved.

   Per the subsequent user instruction, do not add tests. Adapt existing focused tests as needed.
   Run relevant Reach/Settings builds, existing `ctest`, architecture validation, and formatter/whitespace
   checks. Record unavailable manual and performance validation explicitly.

   Repeat the baseline traces and report per-scenario CPU, draws, wakeups, total GPU time, latency, and
   memory. Validate repeated/rapid input, stopped pointer, capture loss, hide/game mode, theme/DPI/resize,
   content updates mid-animation, 60 Hz/high refresh, and battery saver. Do not terminate a running Reach
   instance for validation without the user's explicit authorization for that single termination.

Technical references:

- Microsoft: [DirectComposition animation](https://learn.microsoft.com/en-us/windows/win32/directcomp/animation).
- Microsoft: [DirectComposition best practices](https://learn.microsoft.com/en-us/windows/win32/directcomp/best-practices-for-directcomposition).
- Microsoft: [Windows.UI.Composition animation batch completion](https://learn.microsoft.com/en-us/uwp/api/windows.ui.composition.compositionscopedbatch.completed).

Implementation verification:

- Shared ordered groups support static property updates, finite cubic motion, repeating marquee
  motion, absolute deadlines, retargeting, generation-aware close completion, and finite CPU fallback.
- Migrated System HUD, Settings selection, Top Bar tray reorder/snap, Dock reorder/snap, shared
  Quick Settings/Battery/Context Menu/tray/Clipboard/Switcher presentation, and Launcher presentation.
- Removed the specialized animated-text command/layers and the migrated CPU tracks, frame predicates,
  sampled presentation fields, and Launcher-specific pointer-transform path.
- Preserved CPU geometry for Now Playing width, Dock insertion/removal fitting, bar reveal/native
  window push, and Stage thumbnails; retained content reduces unrelated raster work in those paths.
- Added bounded processed-cover and text caches, bounded retained surfaces, dependent invalidation,
  hidden-resource retirement, and disposal of unused swap-chain buffers.
- Isolated build: `cmake --build build/compositor-animations --parallel 4`; includes Reach, Settings,
  support executables, and the existing test targets. `ctest` passed all 39 existing tests.
- `python tools/check_architecture.py` passed. The user confirmed the rendering, drag, and slider
  fixes work on 2026-10-11. Runtime CPU/GPU savings have not been measured. Regression verification
  did not restart or terminate Reach.

Regression verification on 2026-10-10:

The shared DirectComposition scene reversed sibling order: its opaque background covered later
groups. Inserting subsequent children above existing siblings fixes this for all consumers.
Windows Composition also rejected the completion callback without free-threaded marshaling, and
rejected a residual animation duration below one millisecond. Both submission defects are corrected.
The original command-only test suite did not verify these native behaviors.

| Area | Verification performed | Result |
| --- | --- | --- |
| Shared painter order | Actual DirectComposition insertion calls checked against producer order; old flag rerun as a negative control | Old flag: 23 failures; corrected flag: none |
| Native raster output | Real buffers, real SVG/font resources, GPU surface readback on WUC and DirectComposition | Nontransparent pixels; successful native execution |
| Now Playing | Artwork, processed cover, clipped long title, repeating motion, cancellation, cover retirement | Passed; unchanged marquee frames did not rerasterize |
| Top Bar and Dock | Tray/control/text groups and three Dock items; finite property motion and retained content reuse | Passed on both backends |
| Shared popup presentation | Clipboard, tray overflow, Quick Settings, Context Menu, Switcher, Battery; open/close and native completion | Passed on both backends |
| Launcher | Fade/translation/scale descriptors, cached content, hide/recreate and geometry changes | Passed on both backends |
| System HUD | Brightness, muted volume and media payloads; retained opacity presentation | Passed on both backends |
| Settings | Native selector layer and all navigation pages | Passed on both backends |
| Resource lifecycle | Unchanged buffers, property-only updates, suspend/recreate, 1x/1.5x layout/DPI, dependent cover invalidation | Passed; motion-only changes caused no raster draws |
| WUC callback and timing | Completion registration/notification; 100-microsecond residual duration | Prior callback/duration errors reproduced; fixed paths passed |
| Builds and existing checks | Reach/Settings build, 39 existing tests, architecture and whitespace | Passed |

The diagnostic source and logs are temporary, ignored build artifacts; no tests were added to the
repository. `build/compositor-animations/retained-verification.log` records the final native probe;
`retained-verification-before.log` records the ordering negative control. Later probes used hidden
diagnostic windows. The Reach process and its installed executables were left untouched.

After the user-authorized launch on 2026-10-11, the user confirmed that all items render. High-refresh/
battery-saver behavior, capture-loss/rapid-input visual behavior, Stage native thumbnails, and total
Reach plus DWM CPU/GPU cost still require runtime validation. Native API success and cached draw
counts are not measurements of total application performance.

### Dock and tray drag follow-up on 2026-10-11

Dock pickup sampled the item's animated position after starting the gesture and assigning the new
drag key. Its position lookup consequently returned the previous drag X, initially zero, shifting
both the icon and the pointer's grab offset. Pickup now samples before modifying drag state. The
tray's ordinary drag worked, but grabbing a different icon during an active release snap could
inherit that snap's position; the same ordering correction covers this case.

A temporary diagnostic under the ignored build directory reproduced 384 failures in 1,507 checks
before the fix. The corrected implementation passed 2,395 checks including additional reorder
continuity coverage. It exercised actual gesture/capsule APIs and Dock retained command groups for
all Dock and inline tray positions, both directions, reversals, edge clamping, interruption,
cancellation, no accidental activation, 1x/1.5x DPI, offset monitors, and a crowded Dock. No windows
were opened. Native motion requested no CPU frames after press feedback settled. Reach/Settings
builds, all 39 existing tests, architecture, formatting and whitespace checks passed.

The drag fix is built in `build/compositor-animations`. This follow-up did not restart Reach or
replace the running instance's installed binaries. The user subsequently confirmed it works on
2026-10-11.

### Shared slider clipping follow-up on 2026-10-11

The retained renderer translated clipped rounded fills but left their mandatory clip rectangles in
the original coordinates because their optional image-clip flag was unset. A negative popup-shadow
origin shifted fill relative to mask, cutting off its lower portion. The renderer now localizes the
clip for `CLIPPED_ROUNDED_RECT` as well as for optional image clipping.

Brightness and master sound already share their pill layout, pill renderer, common progress-bar
renderer, and level-from-X calculation. The correction applies to both and to the other users of
the same clipped-fill command. Brightness is hidden when the service reports it unavailable; a
read-only live snapshot confirmed that state, and the Windows WMI brightness query returned
`Not supported`. Verification seeded both available and unavailable states without changing live
capability policy.

A hidden native diagnostic rendered actual Quick Settings/popup commands and read GPU pixels at
the top, middle and bottom of slider fills. WUC and DirectComposition, dark/light themes, 1x/1.5x
DPI, brightness visibility, muted alpha blending, empty tracks and five levels were checked. The
old renderer failed 384 of 720 pixel checks; the corrected renderer passed all 720. Repeated
unchanged buffers did not rerasterize. Reach/Settings builds, all 39 tests, architecture, formatting
and whitespace checks passed. No registered UI tests were added.

Corrected binaries are in `build/compositor-animations`, including the preceding drag fixes.
Reach was not restarted or terminated, and installed binaries were not replaced. The user
subsequently confirmed all items work on 2026-10-11. Performance measurements remain pending.

### Final code audit on 2026-10-11

Removed the unused Dock animation-manager lookup, command executor transform argument and identity,
tray renderer argument, processed-cover margin field, and sampled HUD opacity API. The existing HUD
test now reads the same capsule presentation descriptor consumed by the renderer. Searches found
no surviving migrated CPU track IDs, specialized animated-text path, sampled presentation fields,
or Launcher pointer-transform seam.

Compiler syntax and warning checks covered all 45 modified source files. Surviving warnings predate
the migration and are recorded in the local analysis notes for separate cleanup. Final verification
includes the isolated full build, all 39 existing tests, architecture, formatting, whitespace,
and the retained-renderer, drag, and native slider diagnostics. This audit did not restart or
terminate Reach.
