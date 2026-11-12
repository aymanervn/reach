# Reach Architecture

Dependencies flow inward: outer folders may include inner ones, never the reverse.
Features and services depend on **ports** (interfaces), never on **adapters**
(implementations); only composition and tools know concrete adapters.

```
core ← protocol ← ports ← services ← features
                    ↑          ↑          ↑
                 adapters ────────────────┘   (implement ports; touch the OS)
                    ↑
              composition (wires everything, runs the app)
                    ↑
                  tools (executables)
```

## core

Primitives and neutral shared data — geometry, color, results, ids, render commands,
theme, app/window/media/config data. No state, no policy, no OS. Includes nothing but
the standard library. Surface background colors are semantic theme tokens rather than
value-based aliases: Dock, top bar and switcher each read their named background, while
all popup capsules share the popup background contract.

Rounded border-bearing shapes use `reach_render_push_bordered_background`: it pixel-aligns
the outer shape, emits an opaque border-color fill, then emits an opaque background fill
inset by the border thickness. Features append content after that pair. Normal primitive
antialiasing stays enabled, and there is no general rounded-rectangle stroke render command.
Line, arc, and intrinsic vector strokes remain separate drawing semantics.
Border-bearing layouts treat their padding and content metrics as inner-box geometry and derive
their outer size and constrained content rectangle from the runtime DPI-scaled theme border.

## protocol

Cross-process contracts — Reach Service messages, shared-memory layout, kernel object
names, version constants. Includes `core`.

## ports

Abstract interfaces for every external boundary — renderer, surface, input, monitor,
OS controls, filesystem, clipboard, media, icons, the Reach Service client. The
`window_thumbnail` port abstracts live window previews (DWM thumbnails on Windows);
`screen_hotspot` abstracts the platform capability for an invisible rectangular
trigger window. Composition presents those windows as definition-declared edge
reveals; the Win32 adapter registers its window class idempotently so further
instances cost nothing. The `system_controls` port also exposes system-surface and
application theme modes. Display config stores each as Follow Reach, Light, or Dark;
composition resolves Follow Reach against the active Reach theme, while the Windows
adapter owns personalization persistence and change broadcasts.
`terminal_launcher` opens an interactive terminal with an optional opaque command. The Windows
adapter resolves Windows Terminal's configured default profile from its own settings, opens that
profile in the user's home directory, and appends encoded command-execution arguments without
selecting a shell executable. The shell remains interactive after the command. The adapter also
publishes the resolved executable as the terminal icon reference and falls back to an interactive
Command Prompt in the same directory when `wt.exe` is absent from `PATH`; launcher features never
construct platform command lines.
The media
port separates fast core-state reads from generation-checked cover reads so image I/O
cannot block transport state. Interfaces only. Includes `core`, `protocol`.

## adapters

The Windows implementations of `ports`; the **only** layer that touches the OS.
Includes `ports`, `protocol`, `core`, and platform SDKs. Vector-icon resource lookup degrades a
missing executable-local SVG to a skipped glyph; an absent optional asset never fails the frame.

## services

Shared in-process capabilities with state/cache/policy — config, icons, search,
system status, Now Playing, … Includes `ports`, `protocol`, `core`. The config service owns
the live configuration snapshot, publishes typed mutations before persistence, rebases pending
mutations on the latest stored snapshot, coalesces background writes by generation, and
reconciles external-change reloads. Features consume snapshot copies and never access the
config-store port directly. Window tracking owns the naming policy for a running app
(`reach_window_tracking_app_display_name`: executable stem, window title as fallback);
the top bar's current-app pill reads it from there. Switcher and Stage label each window from its
snapshot title without prepending the app identity. Window tracking also caches visible frame
geometry alongside each refreshed window snapshot through the window-manager port. Geometry
changes publish window-world changes, and both preview capsules read those cached frames without
querying the OS during layout or animation. Pinned apps do not persist a title; the Dock derives
its icon fallback from the executable path and never treats config as a live label. When
`reachctl` creates `reach.ini`, it imports the current Windows taskbar order once, including
shortcut arguments, original shortcut paths, resolved executable paths, and application
identities. Existing configuration is never overwritten, and an empty pin list remains empty.
Application identity is shared across pin import, pin mutation, window tracking, and Dock
reconciliation: two records match when either their nonempty normalized executable paths or their
nonempty AppUserModelIDs match. Window tracking closes transitive matches into deterministic
application groups. The original shortcut remains the launch and icon source. Packaged pins launch
through their application identity; Reach never needs access to their protected installation
directory. Now Playing
publishes atomic core media generations immediately, enriches them with the latest
generation's cover asynchronously, owns transport serialization and cover lifetime,
and masks every transport control while a command is settling. A new core generation
temporarily retains the previous cover; a missing or failed current cover replaces it
with the UI placeholder. A media-to-no-media transition retains the last snapshot for
four seconds, then refetches and publishes disappearance only if absence is confirmed.
Cover acquisition waits for a 300-millisecond quiet period on the latest media
generation, coalescing provider thumbnail bursts without delaying core state. The snapshot carries
the source application's AppUserModelID with that core state.
System status serializes blocking system-control reads and brightness writes on its system worker.
Relative brightness commands accumulate against the latest queued target, publish the successful
target through the cumulative system snapshot, and never call the WMI-backed port on the UI thread.
The wallpaper service owns the cached applied path, per-monitor reconciliation, configuration
seeding, and the ports that control the desktop wallpaper and its Reach-owned presentation surface.

## features

Self-contained UI capsules — dock, launcher, switcher, quick settings, clipboard,
settings, context menu. Each owns its state, layout, animation,
hit-testing, render composition, and interaction behind
create/update/handle_event/append_render_commands entry points, and returns semantic
actions instead of calling ports. Includes `services`, `ports`, `protocol`, `core` —
never another feature's internals.

Every capsule also implements the uniform hooks in
`reach/features/feature_capsule.h` (`reset`, `tick`, `is_open`, `force_close`,
`presentation_visible`, `on_game_mode`, `needs_frame`, `wants_pointer_move`, `handle_pointer`,
`pointer_sequence_active`, `handle_event`);
composition orchestrates through these, so adding a feature costs no
feature-specific composition code. `handle_pointer` carries the complete
down/up/move/wheel/leave/cancel/context stream, with primary, secondary, and middle
button identity on down/up, so cleanup semantics do not fall back to feature-specific
host branches.
The Dock is fully migrated to this contract: it owns press/release, item
feedback, drag/reorder, middle/context actions, and cancellation. The Top Bar
owns the same for its hosted buttons — power, tray icons, tray overflow, quick
settings, language — plus its private Now Playing input. Composition translates
only their semantic actions and cross-feature popup policy. Raw hit types and
row/index results are feature-private; both bars expose only a semantic
pointer-region query for the global popup mouse hook.
Dock item activation is decided only by the shared pressable's release result.
Primary and middle release converge on one item-target publisher and emit the
generic `OPEN_TARGET` action; middle release adds only the new-instance flag.
Dock width is enclosed to a physical pixel and the subpixel remainder is split
between its capsule-owned outer insets, keeping the first and last controls equally
spaced from the border at every DPI.
Pressable controls that can become horizontal reorders compose the shared
`reach_draggable` gesture and `reach_horizontal_reorder_target` helper. The common
layer owns threshold and gesture lifecycle semantics; each capsule retains its item
identity, model mutation, layout, animation, rendering and persistence policy. Dock
persists pin moves through its existing semantic action, while top-bar tray order is
visual state retained only for the current process.
The Dock also owns its geometry (`reach_dock_local_point` /
`reach_dock_rect_to_screen` / `reach_dock_layout_to_screen`), converts the
screen-space pointer stream to dock-local coordinates itself, performs the
animated item rebuild (snapshot/build/rebind) as one op, and assembles the
context-menu command list for its items from its pin state and window service;
the command vocabulary lives in `reach/core/menu_commands.h` so the dock and
the context_menu display capsule share it without a feature→feature edge.
It supports 96 configured pins and 96 running app groups in one 192-item model.
Its native height, icon and gap metrics are maxima: layout receives the hosting
monitor width through `reach_dock_layout`, represents appearing and dying slots
as normalized reveal units, and uniformly scales every internal dimension only
when the animated one-row content would otherwise overflow. There is no minimum
Dock width, height, icon size or scale. Layout reserves 32dp at each horizontal
screen edge and 12dp below the Dock; fitted heights remain bottom-anchored so the
bottom margin is invariant as the Dock scales. Host-owned reusable command buffers
are sized from these capacities so the worst-case Dock does not truncate render
commands or place the enlarged buffer on a frame's stack.
The Launcher is also fully migrated: it owns result and pinned-app presses,
scrolling and scrollbar capture, cancellation, and context-hit semantics through
`handle_pointer`. Composition translates launch/open/reveal actions and retains
focus restoration plus transient-surface policy.
Launcher rows carry typed actions independently from their display fields. A leading `!` switches
the capsule synchronously into terminal-command mode, cancels indexed search, and publishes one
selected row whose opaque payload is every UTF-16 code unit after the prefix. Removing the prefix
returns to normal search; composition submits terminal actions through `app_control`.
Its open/close transition keeps one maximum-size native envelope and applies one
centered proportional scale to the complete capsule alongside the existing fade and
Y tracks. Pointer input is inverse-mapped through the same scale, and closing reverses
all three tracks from their current values.
Attaching the first search results keeps that native envelope fixed while the visible
wrapper expands downward on a feature-owned track. The capsule publishes both rectangles
through the generic `surface_geometry` hook. Result commands use the shared cropped reveal
treatment also used by Quick Settings expansion sections.
The Clipboard is fully migrated as well: it owns item, close, clear, hover,
scroll, scrollbar-capture, leave, and cancellation behavior. Composition handles
only restore/provider calls, external resource release, and transient-surface
policy for the semantic actions it reports.
Quick Settings owns tile, slider, output-device, expansion, drag/capture, release,
and cancellation behavior through the same hook. Composition translates its
semantic actions into audio and system-control calls and retains popup policy.
Brightness and master sound share the pill layout and renderer, the common progress-bar renderer,
and the level-from-X calculation. Brightness contributes a row only when its service capability is
available. The retained Windows renderer localizes mandatory clipped-fill masks together with
their geometry, independently of optional image clipping.
Quick Settings also attaches the system-status service directly (the
launcher→search precedent): snapshot take/apply runs inside the capsule
(`reach_quick_settings_process_changes`); its pending service work folds into
`needs_frame`. That hook schedules capsule work only; it never implies that a closed
native surface is presented. A bluetooth toggle is resolved by the port, not by a timer: the
adapter raises `REACH_SYSTEM_CONTROLS_CHANGE_BLUETOOTH_REQUEST` alongside
`..._CHANGE_BLUETOOTH` when the requested set has run to completion, and the
capsule clears its pending tile on the snapshot carrying that reason — or earlier,
on any valid read that already matches the target. The system-controls watcher fires
on a port thread, so composition keeps the atomic change-flag accumulator and —
because the top bar's network readout needs fresh state whether or not the panel
is open — composition, not the capsule, turns the drained flags into the
`reach_system_status_refresh_system` request. Capsule state is never written
off-thread. GPU lifetime stays in
composition: audio applies retire the replaced session/device render icons
and the host drains and releases them.
A refresh probes only the capabilities its `change_flags` name (0 means all), and
the published snapshot is cumulative: every field holds the last value read
successfully, so a scoped refresh never blanks the fields it skipped and a failed
read never replaces a good value with a zeroed one. `change_flags` accumulates the
reasons published since the last `take_system`, which consumes them.
The system-status snapshot therefore has two readers with different needs, so
the service offers both: `take_system` consumes a published generation (Quick
Settings applies each one exactly once) and `read_system` copies the latest
without consuming it (the top bar polls it from `tick` and diffs the values it
renders). A second consumer must never be added on `take_system`; it would steal
generations from the first. The audio snapshot splits the same way and for the
same reason — `take_audio` for Quick Settings, `read_audio` for the top bar's
volume glyph. Neither snapshot is polled into existence: audio refreshes are
driven by the `audio_volume` port's watcher, which registers an endpoint-volume
notification on the current default render device (and re-registers when that
device changes), fires on a COM thread, and lands in the same
composition-owned atomic-flag → drain → `refresh_*` path the system-controls
watcher uses.
The tray service owns the provider port, cached item snapshot, activation, and native icon
retirement. The top bar consumes that service directly and owns both the inline tray cells and
its private overflow-popup capsule: popup layout, item hit resolution, press/release feedback,
left/right semantic actions, and cancellation. Composition retains topmost-window handling,
surface lifecycle, semantic-action translation, and renderer-cache eviction before retired native
icons are released.
Stage is the window overview: a fullscreen overlay capsule that shrinks open app
windows into a centered grid over a fixed Desktop preview. The Desktop keeps the
same centered, solo-size geometry whether or not apps are present. Its outer edge fits between
the top bar and Dock with 20dp clearance from each; it has no
artificial header or title, and contributes no app-grid layout slot. App sections consume the
Desktop preview's final target rectangle with a fixed 24dp inner inset, falling back to the
bar-reserved bounds only when no Desktop tile exists. Each monitor section first maximizes against
available height, then all sections reduce together only when their combined visual width,
including every tile and monitor-section gap, would overflow. That shared width solve gives
portrait sections 1.3 times the scale priority of landscape sections, capped by the same available
height, so mixed-monitor layouts trade some landscape size for larger portrait windows without
resolution- or DPI-specific cases. Cell width comes from each window's actual aspect ratio rather
than an invisible monitor-shaped box. Landscape sections use a compact grid; portrait-monitor
sections use one vertical column ordered by the windows' original screen coordinates. Visual tiles
use 20dp gaps, monitor sections use 28dp gaps, and the combined fitted content—not proxy boxes—is
centered.
Stage owns tile layout, the open/close animation, hover state, and hit resolution,
gives overlapping app tiles pointer priority over Desktop, and reports only
activate/dismiss actions. It uses no
generic host transition: its fullscreen surface remains fixed to the monitor while
the capsule's elapsed-time progress animates app tiles. Desktop uses a separate track at 35% of
the configured Stage duration, so it settles substantially earlier without depending on a fixed
millisecond delay. Closing an individual window from its tile gives that departing tile a dedicated
opacity track lasting one third of `stage_reflow_seconds`; the surviving tiles keep the full reflow
duration, so this does not change Stage open/close timing or grid rearrangement. It never calls the
thumbnail port — it
publishes a read-only placement list
(`reach_stage_thumbnail_count` / `reach_stage_thumbnail_at`) that composition drives
into `window_thumbnail` each frame. Switcher uses that same generic native-overlay contract and
thumbnail port. Its horizontal strip has one common preview height and per-window widths derived
from cached source aspect ratios. A single capsule-owned card layout supplies drawn chrome and
native placement, packs the visible range to the monitor, and keeps the selected window visible
while cycling the full ring. Minimized or unavailable frame geometry uses an icon placeholder;
native registration failure leaves the titled card usable. Titles and selection chrome stay outside
native content. Visible membership changes reconcile relationships without recreating surviving
sources. Switcher retains its ordinary non-activating Alt-Tab behavior and shared close transition.
Dock hover previews use Context Menu's private window-preview layout and rendering components.
They publish through the same native-overlay contract and reuse cached window tracking and the
icon service. A horizontal row starts at 140dp preview height with source-proportional widths;
Switcher retains 125dp. Both consume the shared pure aspect, common-height fitting, and range
packing helpers in `reach/features/common/window_preview_layout.h`. Context Menu keeps its own
anchoring and complete-card page navigation, exposing every instance in the 16-window semantic
payload independently of the eight-command menu capacity. Headers and close controls stay above
native previews; minimized or unavailable sources use retained icon placeholders.
Window-change notifications refresh titles, geometry and eligibility, prune departed sources,
and close an empty popup. Dock republishes the hovered owner's payload after model changes through
its public route callback, preserving the popup transition for the same owner. Current layout
resolves card placements, hit regions, anchor, DPI and pointer corridor together. An optional
surface-ops pointer-observation hook lets the generic frame path update the event-driven watch
after arrangement. Ordinary command and power menus keep their existing row presentation.
Dock preview width and membership reflow share one capsule-owned 180ms canonical layout epoch.
Target packing remains CPU policy; body/notch placement, proportional equal-height cards and gaps,
header fills, borders, close controls and independent width-text viewports retain that intent.
Survivors capture their screen X at the exact retarget timestamp; the shared same-epoch constraints
keep complete cards in the moving viewport. New sources enter from the existing small offset.
Stable per-window token blocks identify sources independently of visible order. A stable session
envelope holds retained body placement; native destinations and input snapshots sample the same
body/card rectangles. Pointer observation covers the envelope and owner corridor while retention
uses the sampled body. Closing freezes width and reflow, hide/reset retires ownership, and fresh
presentation or changed monitor/DPI constraints cuts stale layout intent. One deadline cleans up
layout; the scalar width manager, periodic width tick/frame demand and sampled render rectangles
are removed. Source extents and header heights remain endpoint layout data. Hovering anywhere
on a card highlights its fixed header pill and square thumbnail border with the shared
`context_menu_hover_background` theme color. The dark theme uses its darker structure shade.
The title has 8dp left padding; the close control renders at 80% of its hit-region dimensions
without moving the title on hover. The theme-scaled outline sits outside native preview bounds
and uses the shared bordered-background renderer so DWM content cannot obscure it.
Each thumbnail relationship declares its destination at creation. Runtime teardown releases only
that surface's IDs, and adapter entries retain stable addresses and support simultaneous targets,
including overlapping Stage and Switcher presentations. Adapter destruction releases all remaining
relationships. Stage's tiles live in
screen space; the render pass converts to surface-local. Native-overlay placements
select either the target surface or a plane behind it. App thumbnails remain DWM
relationships on the Stage target. The Desktop relationship instead targets a
non-activating top-level helper HWND that the Windows adapter keeps fullscreen and
directly behind Stage. That lower HWND paints one fully opaque, theme-colored Stage
background and positions the animated Desktop thumbnail within it. The upper Stage
DirectComposition surface stays transparent except for app plates, headers, titles,
close buttons, and the Desktop's transparent-center outline made from ordinary fills, so no
moving cutout or split backdrop is required. Because each app DWM
thumbnail still composites on top of its own destination surface, app chrome stays
outside the app thumbnail rect. Minimized windows have no DWM content and fall back
to an icon tile. The Desktop source is the Reach-owned top-level Progman
compatibility host, cropped from the virtual-screen thumbnail to the primary
monitor.
Every Stage dismissal uses one close choreography. An app tile publishes ordinary activation
without close-first ordering, so the app-control worker queues the selected window before generic
feature-action handling requests the same Stage close used by the Dock trigger. Stage stores no
closing selection: every live app thumbnail returns to its source frame, while a minimized icon
placeholder keeps its Stage size and exits fully beyond the surface's top-left instead of expanding
toward its restore bounds. Desktop follows its shorter track, and event-driven window-state changes
update that same running close rather than selecting a different choreography. For an app handoff,
composition marks the activated source as the
presentation-only front owner before starting that close. The native-thumbnail adapter promotes
the existing relationship without changing its stable ID or geometry, and Stage emits that tile's
drawn chrome last; the preference remains until native-overlay teardown. This does not become
Stage selection state and does not fork movement, synchronization, fade, or teardown. The feature
never writes foreground tracking optimistically; the
foreground watcher records the OS result. Stage does not wait for native window readiness before
moving. Desktop publishes minimize-all through the same action contract and uses the same close
choreography. Ordinary activation retains its existing
topmost promotion/demotion sequence and native foreground-transfer compatibility path, including
the final attached-input raise. Switcher uses the same ordinary activation path. Its non-activating
overlay presents immediately and starts the shared close transition before publishing the selected
window for activation. Switcher Begin runs the generic transient close sweep before refreshing the
window world and dispatching the event to the capsule.
The app-control worker serializes window requests, coalesces pending activation selections, and
preserves close, minimize, and snap requests. Each request publishes a correlated completion.
Activation or minimize-all completion never gates the initial movement; it only retains the final
opaque aligned frame until composition refreshes the real window state.
The close lifecycle retains every aligned thumbnail until renderer synchronization acknowledges
that frame. A source-frame change during movement rebases interpolation from the currently drawn
rectangle, and a change after alignment runs one short shared retarget. A window that becomes
minimized retargets toward the offscreen placeholder destination; a restored minimized source
retargets toward its live frame and joins the DWM presentation before reveal.
Only after the current aligned frame and any correlated app-control handoff are complete does the
backdrop fade using the shared surface-close duration while the thumbnails stay opaque and
stationary. A transparent-frame acknowledgement precedes native thumbnail and helper disposal;
there is no second activation at disposal. Escape follows the same movement and reveal sequence
without activating another app. A vanished source drops out through the native overlay's close
recovery. The lower helper carries its
own layered opacity and changes bounds, order, visibility, and paint only when needed.
The DWM adapter also caches the last submitted thumbnail properties and skips identical
`DwmUpdateThumbnailProperties` calls; DWM owns live thumbnail resolution and Reach does not request
a capture resolution or refresh rate. Native thumbnail registration and placement are best-effort
while Stage is open or reflowing because a source can disappear between the window snapshot and
the DWM call. Generation changes reconcile relationships by source and plane: unchanged app
relationships and the Desktop helper HWND remain alive, new relationships are added, and departed
relationships are released only after the capsule removes their tiles. Missing relationships are
retried without aborting the host frame. Placement remains strict at Stage's synchronized close
boundaries so a failed final handoff follows the capsule's existing recovery path.
The renderer's synchronization operation waits for DirectComposition commit completion and
then flushes the calling process's queued DWM work; it does not establish that an external
application has finished producing its own content.
A tile's `source_rect` is its opening origin, layout aspect source, and normal live-window close
destination. Windows move while the overview is up — opening Stage forces the top bar shown, and the
bar pushes every trespassing window down with its reveal progress. Composition therefore re-seats
the source rects from live `frame_bounds` immediately before `reach_stage_begin_close`
(`reach_stage_refresh_tile_frames`), so a live tile returns to where its window is now rather than
where it was when the overview opened. Stage then resolves a separate `close_destination_rect`;
minimized placeholders use an equally sized rectangle beyond the surface's top-left, while other
tiles use `source_rect`. The refresh deliberately leaves `target_rect` alone: the grid box is an
aspect fit of `source_rect`, so re-fitting it would resize tiles the instant a close begins.

Context Menu owns row hit resolution, hover state, command selection, dismissal,
and cancellation through `handle_pointer`. Composition executes the reported
command and retains OS calls plus cross-popup and Dock power-button policy.
Interaction hit contracts for every migrated feature remain private to that
feature; public capsule APIs expose semantic actions, queries, and render inputs.
Capsule state is compiler-enforced private: the public `reach_<f>_state_ptr()`
accessors return `const`, mutation goes through semantic ops, and the internal
`reach_<f>_state_mut()` accessors must never appear outside `src/features/`
(checked by `tools/check_architecture.py`), with no exceptions: the launcher
owns its text input end-to-end (its `handle_event` capsule hook drives the edit
model, query, and attached search behind the routed TEXT_CHAR/TEXT_EDIT events,
and composition applies only the reported redraw/relayout).

A capsule that consumes non-pointer UI events declares them as
`reach_feature_definition.routed_events` and receives them through the uniform
`handle_event` hook, which returns the same `reach_capsule_event_result`
vocabulary as pointer input: handled/redraw/relayout/request_update plus one
shared action. Composition broadcasts each routed event to every declaring
capsule and observes the capsule's own `is_open` to drive presentation, so
keyboard behaviour needs no feature branch. A capsule that acts on a target
outside Reach publishes a complete `reach_feature_target`
(`include/reach/features/common/feature_target.h`) — app, path, terminal command,
or explorer location — instead of leaving composition to query it back for the
selection it just acted on. The Launcher owns the whole decision: which result is
selected, whether an empty query means the default location, and whether a query
is a `shell:` location or a filesystem path.

**Accepted coupling (by design — do not “fix”):** the top bar cluster. The top
bar hosts the tray / quick-settings / power buttons, so its private tray overflow UI and the
other popup features may take the top bar layout directly; no anchor indirection is wanted
between them. Split, unified, and simple top-bar styles are chrome and edge-geometry treatments
over that same capsule, layout, control set, and action routing: the style profile changes the
background, border, screen inset, app clearance, and input-region composition without duplicating
controls. Now Playing is not a
separate feature: its private UI subfeature lives inside the top bar and consumes
the shared Now Playing service, leaving room for a future standalone music feature
to consume the same stable service independently. Its bold title is ordinary text inside a retained
render group with a compositor clip and repeating X motion when the line overruns its slot. Windows
Composition and DirectComposition share ordered retained-group ownership; the renderer without
composition draws the title as static clipped text. Hidden groups are detached and released.
Transport controls retain their dedicated actions, and a pressable release on the remaining card
opens the snapshot's source application through the shared `OPEN_TARGET` action.

Finite scalar animation tracks also own canonical `reach_motion` descriptors. The manager samples
an explicit monotonic event time with `reach_animation_manager_sample_at`; it no longer accumulates
frame deltas or implements a separate interpolation curve. Retargeting samples the previous curve
at the event, advances generation and preserves continuity between application frames. Cached value
and activity are CPU presentation state: pending activity survives until a sample delivers the final
endpoint, then stops. `reach_animation_manager_motion` exposes the same intent for retained rendering
without GPU readback. Settings samples all finite scalar families at one update timestamp. Loader,
scroll and native choreography timing remain separate migration responsibilities.
Switcher fitting uses canonical retained motion directly with the `smooth` spring; one endpoint
deadline retires its width descriptor without periodic capsule redraw or scalar-manager sampling.
One selection pill, parented to a body-center anchor that follows the width and clips to the
body's rounded interior, retargets its springs to the selected card's settled slot, keeping value
and velocity: one-slot moves use `bouncy`, wraps and longer jumps `smooth`; its position is never
clamped. The Switcher drives it through the shared selection pill with
`reach_selection_pill_mode_for_distance`.
Card labels ride their card's motion; the Switcher has no fixed durations.

Two shared list-motion helpers live in `features/common` and read only theme motion presets.
`reach/features/common/keyed_rows.h` keeps identity-keyed row motion over a caller-supplied
fixed-capacity pool of `reach_keyed_row` entries (lifecycle, index, `y`, `opacity`); identity is a
caller `same(context, slot, item)` callback and payload stays in a caller-owned per-slot array.
`reach_keyed_rows_sync` takes caller target positions: survivors retarget `y` with `smooth` from
value and velocity, inserts start at opacity 0 and `fade_in`, removals `fade_out` in place, a
returning identity revives its retiring entry, and a full pool makes an insert wait without
eviction; with `animate` off it sets everything without motion. It reports allocated, revived and
released slots so the feature can copy or free payload and icons; `_release_settled`, `_active`
and `_deadline` cover cleanup and presentation deadlines.
`reach/features/common/selection_pill.h` owns one `reach_rect_motion` plus its slot;
`reach_selection_pill_retarget` uses `bouncy` for `STEP`, `smooth` for `JUMP` and sets without
motion for `SET`, retargeting only changed channels from value and velocity, and
`reach_selection_pill_mode_for_distance` encodes the adjacent-step, otherwise-jump rule.

Canonical motion supports continuous piecewise affine mappings, preserving epoch, easing and
generation. `reach_motion_hinge` constructs two slopes meeting at one center; `reach_motion_peak`
uses that shape to brighten and dim a label. Constructors require ordinary input with compatible
limits. `reach_motion_combine` adds same-epoch properties, including active output clamps and
different breakpoints. `reach_motion_min` and `reach_motion_max` enforce moving constraints.
Constants need no common clock; incompatible moving epochs and more than eight slope breakpoints
are rejected without changing the output. The descriptor stores seven additional slope changes;
unused storage does not affect retained intent identity. Output limits and affine transforms retain
the full mapping. `reach_motion_range` includes every interior extremum.

`reach_motion_ratio` derives a finite ratio with a positive denominator from compatible ordinary
inputs; `reach_motion_multiply` derives a quadratic product from compatible ordinary inputs.
Constants need no common epoch. Both reuse existing descriptor storage, preserve the source epoch,
and support output affine mapping, limits, range and crossing queries. Nested or incompatible
moving inputs reject without output mutation. Ratio and quadratic native curves share bounded
adaptive cubic compilation with the existing 32-piece cap and precision. Budget rejection remains
explicit; consumers must retain a finite canonical fallback for unsupported combinations.

`reach_motion_lookup` projects a finite monotone source into up to eight discrete threshold changes
without growing the descriptor or introducing another owner. Source mapping/clamp intersections
resolve into its raw domain; constant native pieces jump through the existing WUC step/DC cubic
execution. At an exact crossing, traversal selects the following state in either direction. Affine
output changes retain crossing times. Range and crossing queries inspect only visited states;
retargeting begins at the current discrete value. Nested lookup, repetition, decreasing source maps
and ambiguous continuous/discrete composition reject before mutation.

The shared compiler splits at mapping knots, clamp crossings and the easing midpoint into at most
32 finite pieces or 68 rotated-cycle pieces. Both native backends consume those same cubic segments.
CPU input/fallback samples use the same descriptor; crossing queries find the first future crossing,
and equal visible endpoints do not suppress intervening periodic peaks. Retarget the source intent
and derive mapped properties again. This extends the existing animation owner rather than adding a
second clock, native graph or application frame loop. Descriptor memory cost and product workload
acceptance are recorded in `gpu-workload-baseline.md`.

Settings Reach-theme and Top Bar style selectors use canonical position intent and shared retained
segmented controls. Cached selection coverage translates; fixed labels use RGBA tint, with a peak
mapping for interior choices. Outer labels use bounded affine weights, preserving clamped positions.
The two scalar managers, sampling functions and application frame predicates are removed. Immediate
Windows system/application theme choices reuse the same primitive with constant intent. Shared
Settings control ids preserve the four-subsource allocation used by toggles and selectors.

Settings Power option colors own five canonical 220ms ease-out descriptors and one earliest visible
cleanup deadline. Selection retains its previous reset-on-selection policy. Shared selection-item
rendering retains opaque outer/inner coverage and separately tinted label ink. Custom backgrounds
reuse bounded canonical piecewise mapping for the existing nested color mixes; custom textbox ink
uses the same selection descriptor without tinting suffixes, placeholders or highlights. Label
source envelopes preserve unclipped overflow and physical pixel alignment. Endpoint cleanup folds
constant sources back into ordinary paint; hidden pages and minimized windows publish no Power
cleanup wake. The scalar Power manager, sampled values and periodic frame predicate are removed.
Unsupported paint geometry/capacity falls back to sampling the same canonical descriptor only while
active. Product appearance and total-cost acceptance remain in the migration review register.

Finite and periodic properties share `reach_motion`. Periodic intent adds forward/backward travel
with independent endpoint holds and an absolute epoch. CPU sampling, affine mapping and bounded
compilation preserve phase; the compiler rotates one complete cycle to the submission time.
Zero travel with positive holds represents discrete endpoint switching through that same intent.
Its compiled pieces stay constant until the boundary; WUC uses final-step easing and DC uses
constant cubics with native repetition. Sampling, mapped ranges and crossing queries include only
visited endpoints, so a skipped intermediate value has no crossing and a mapped interior peak
cannot manufacture activity. A missing hold normalizes to the remaining constant endpoint.
This capability adds no descriptor fields or alternate clock. Eligible textboxes use it for
independent caret opacity; unsupported rendering retains absolute deadline scheduling.
Both compositors repeat the resulting keyframes/cubics without application ticks. Root presentation
remains finite so completion ownership is well-defined. Retiring or replacing retained sources stops
periodic transform, opacity, clip and tint bindings before releasing the layer; font replacement
uses the same cleanup. Now Playing's marquee now uses that canonical periodic X intent rather
than a separate native cycle descriptor. Its capsule owns the absolute epoch and generation;
unchanged submissions, theme repaint and source recreation preserve phase. A changed title or
measured overflow starts a fresh cycle with a readable endpoint hold. Both compositors use the
application's cubic ease-in-out curve, resolving the former WUC Bezier/DC smoothstep difference.
Logical overflow, speed and threshold scale together with DPI; both endpoint holds remain 1.6
seconds. Hide, disable, reset and source retirement stop repetition. Generic surface context
publishes retained-rendering capability, which the registry passes to Top Bar's render context;
unsupported rendering clears marquee intent and retains static text without CPU frame demand.
No application tick, alternate animation owner or adapter-local marquee timing remains. Displayed
appearance and total workload/power acceptance remain migration work.

Settings Wi-Fi, Bluetooth, Windows Update and installed-app loaders publish normalized periodic
width and translation through the shared loader renderer. Fixed caps preserve the bar radius while
the middle stretches; a static rounded viewport preserves container coverage. Retained groups expose
`clip_radius` for rounded body clipping. `reach_render_group_clip_motion` derives canonical origin
and nonnegative size from paired insets; each rounded axis requires compatible epochs or constants.
Windows Composition animates rounded geometry offset, size and independently clamped corner radii;
DirectComposition animates the corresponding clip edges and radii. Rounded root clips remain
unsupported. Removing clipping and retiring repetitions stop the native geometry bindings.
Fallback drawing applies rounded ancestor masks separately from hard rectangular ancestor clips,
so a rectangular bounding approximation cannot cut away rounded antialiasing. Small bounds whose
minimum bar width cannot contain fixed caps publish sampled clipped geometry and set
`requires_content_sampling`; generic frame scheduling respects that flag. Settings passes retained
renderer capability into its render input. Without retained support, the shared loader emits direct
rounded geometry sampled from the same motion, preserving fractional-position fallback pixels
without building stretch patches.

Settings bootstrap owns an auto-reset wake event. Services and local workers publish data before
calling a coalesced notification; the UI drains notification intent before consuming results so
concurrent publications retain a wake. Pending work alone does not request application frames.
Progress wakes respect an already pending frame deadline, and input remains immediate. Minimized
rendering suspends retained sources and clears sampled motion demand; restoring republishes the
current absolute phase. Worker/service teardown precedes closing the bootstrap event.

Retained render groups carry stable identities, local content bounds, clips, and independent X/Y,
opacity, and scale motion. Scale remains uniform by default; an explicit `scale_y` motion overrides
the inherited vertical scale. Both compositors, fallback, root completion, native preview projection,
host hit testing and native input regions use the same axis semantics. Root envelopes account for
the maximum of both axes. Features own gesture policy and animation intent. `reach_motion` samples
finite cubic motion against a monotonic clock for interruption and event-time hit testing; no CPU
tick is needed to advance compositor motion. The adapter owns native visuals, drawing surfaces,
native animation objects, and completion resources. It compares localized commands independently
of presentation properties, preserves painter order by splitting static runs around explicit
groups and individual ungrouped textboxes, and releases the superseded swap chain after constructing
the retained scene. Textbox edits repaint their editor source. An optional `caret_group_id`
references a dedicated opacity descriptor; the adapter resolves a small stroke source from the
same cached DWrite layout and parents it to the editor, preserving its raster limits and ancestor
transforms/clips. Caret boundaries change native opacity without application frames or repaint.
The body is an ungrouped editor or a single-command, tint-free group; the referenced caret owner
cannot paint other content. An optional `text_color_group_id` references an exclusively owned,
root metadata descriptor for textbox ink color. The adapter separates backdrop/selection/placeholder,
glyph coverage and caret; only glyph/caret parts receive the color, while caret opacity remains
independent. Unpainted editor parts allocate no source. The CPU fallback samples the same RGBA
intent. If duplicate painted textbox parts exceed the shared source budget, renderer-owned sampling
repaints only the necessary editor source until its canonical endpoint, preserving the native caret.
Helpers preflight property ownership before append; validation rejects aliases, painted metadata
and ambiguous bodies before renderer dereference.
Explicit groups otherwise retain feature-declared ownership. Surfaces
are bounded to 128 MiB per renderer and 16,384 pixels per dimension. Retired groups are removed
before replacements are allocated. Ordinary text and measurement share 32 text formats and 128
layouts with bounded string lengths per renderer. Font changes and backend destruction invalidate
those entries, and icon retirement invalidates dependent retained commands.

Composition owns one `reach_render_device` per process and passes it to every renderer backend; the
Settings process creates its own for its single surface. The device owns the Direct2D, DirectWrite
and WIC factories, the D3D11, DXGI and D2D devices, the Windows.UI.Composition compositor and its
composition graphics device. It also owns the device-bound caches and contexts: icon bitmaps, the
24-entry, 16 MiB processed cover cache, baked shadow bitmaps, the shadow-bake, shadow-fallback and
cover-bake device contexts, and the bundled UI font collection. Cache keys are content-based (icon
id with cover size, crop, blur and contrast; shadow geometry and color) and never name a surface, so
the caches are process-wide and bounded once for all surfaces. Every shared context restores its
target, transform, antialias mode, primitive blend and unit mode after each use through
`reach_d2d_context_scope`. Icon retirement fans out to every renderer, which clears its own retained
commands while the shared entry is erased idempotently.

Each backend keeps its own window target, visual tree, retained layers and drawing surfaces, main
device context, swap chain, text caches and 128 MiB surface budget. A surface counts as active on
its first frame and inactive on suspension or destruction, once each. Suspension releases only
per-surface state; the device trims Direct2D and DXGI memory only when no surface is active, so
hiding one surface never evicts resources another still draws with, and trimming never drops the
shared caches. `reach_d2d_device_release_device_resources` is the single place that drops every
device-bound cache and context, and device destruction calls it.

Device loss is recovered in process. Every failing Direct2D, DXGI or composition call is classified
(`reach_render_classify_hresult`) and a device-loss failure surfaces as `REACH_DEVICE_LOST` instead
of a generic error; the shared device also registers a device-removed event. The
`reach_render_device_port` in `include/reach/ports/render_device.h` exposes `status`, `recover`, the
event `wait_handle` and `diagnostics`. While the device is lost the Reach and Settings message loops
skip frames, wait on that handle and the retry deadline, and call `recover`, which rebuilds the
D3D11, DXGI and D2D devices, the compositor graphics device and every device-bound cache. Each
renderer backend compares its device generation with the device's before drawing and resets its
per-surface state when it changed, and retained content is invalidated so the next frame redraws
everything. Retry timing is the pure policy
in `include/reach/support/render_recovery.h` and recovery stays on the GPU: the first attempt is
immediate and hardware retries follow every 250 ms for the first 5 s (the driver-reset window), every
2 s until 15 s and every 4 s after that, bounded at 30 s. A display-change, resume or session-unlock
notification received by any platform window reaches the device through
`reach_windows_render_device_hint`, which wakes the loop and makes the next hardware attempt due at
once. The WARP software adapter is used only when hardware is unusable (the device reports
`DXGI_ERROR_UNSUPPORTED` or `DXGI_ERROR_NOT_FOUND`, or the hardware adapter is a software adapter
such as the Microsoft Basic Render Driver) or after the full 30 s hardware window, and the process
exits with code 3 (`REACH_EXIT_DEVICE_LOST`), which the watchdog treats as an unexpected exit, only
when WARP also fails or more than five losses occur within 60 s. While WARP is active, `recover`
rebuilds back on the hardware adapter at the first adapters-changed, display-change, resume or
session-unlock event once the device is healthy; nothing polls. Only the dock window and the
Settings window register for session notifications with `WTSRegisterSessionNotification`, and the
switch reason, timing and `adapter_description` are logged as `device_adapter_switch`.

The adapter caches ordered retained group identities and parent ownership separately from content
and motion. Unchanged submissions, raster edits and property retargets keep the native tree intact;
membership, painter order, parent ownership or font-source replacement rebuild it. A rebuild is
valid only after every attachment succeeds, and suspension clears that ownership. Scene content
matrices use named-field comparison and submit only when changed or recreated.

A retained group may own a shadow-only shape subtree through `has_shadow` and existing Reach shadow
metrics. Its ordinary descendant rectangles, clipped/notched rounded rectangles, triangles and lines
supply alpha. The filter shifts/blurs/colors that alpha and removes the unshifted source; the parent
retains normal output placement, opacity, scale and clipping. Own paint, nested filters, output tint
and root-presentation filters are rejected. Both native adapters retain the caster/effect/output
separately and route children to it. Parameter changes preserve sources; disable/reparent/retirement
use the existing tree ownership. Nominal capture bounds participate in the shared 128 MiB limit.
Unsupported composition uses the same fixed-time source renderer and D2D effects. It shares a
hardware effect context when available, or uses WARP and bounded bitmap transfer for an HWND target.
Those targets also use 96 DPI because command coordinates are already physical pixels. Source joins
must overlap inside their geometry where separate antialiased masks would otherwise leave holes;
filtering preserves the supplied subtree alpha rather than inventing an opaque interior.

Group clips retain a stationary viewport and four independent absolute-time inset descriptors.
Windows Composition animates the cached `InsetClip` properties; DirectComposition animates a cached
rectangle clip, mapping right/bottom insets to native edge coordinates. Removing clipping retires
the native clip. `reach_render_group_clip_at` provides the same event-time rectangle to CPU fallback
and geometry consumers, and active clip motion participates in generic fallback frame demand.
Root-presentation clipping remains unsupported. Rounded groups default to independent radius
clamping on each axis. `clip_uniform_radius` selects circular corners whose shared radius follows
the smaller animated dimension. An optional `clip_radius_motion` replaces the scalar radius,
including an explicit zero, and shares the finite or repeating canonical owner. Its endpoints must
be nonnegative and overshoot below zero clamps; incompatible moving epochs are rejected.
`reach_render_group_clip_radii_motion`
supplies bounded radius curves to both compositors. `reach_render_group_clip_radii_at` samples the
radius and clip dimensions directly for CPU masks and fitted solid paint, preserving scalar
rounding at the event time. Native rectangular clips use hard edges and rounded clips use soft
edges; removing a clip restores inherited edge policy and retires its native bindings. Specialized
fitted image sources reject a radius driver atomically and may retain a separate parent mask.

`reach_render_retain_commands` gives individual paint commands independent cached source groups,
preserving overlapping paint when ancestor opacity changes. It validates the complete batch before
assigning owners and provides pixel-aligned source padding. Single-line text leaves can preserve
DirectWrite's baseline phase with nearest sampling during unscaled vertical movement; phase is
cached with source content, while scaling, horizontal motion, repetition and parent content keep
ordinary linear sampling. Both native backends share this source metadata and update placement or
sampling without adding a frame loop. Fractional bitmap translation still filters shape edges;
endpoint fidelity and intermediate geometry/color/clip fidelity are verified separately.

Groups can declare `parent_id`; source geometry stays in feature coordinates, while each content
transform moves its raster and descendants. A group's clip is stationary within its parent, so
viewports and independently moving contents can nest. CPU geometry helpers compose ancestor
transforms, intersect viewports and sample inherited opacity at one event time. Both native
backends reuse existing container/content visuals; empty parents allocate no raster, and reparenting
changes ownership/origins without repainting unchanged content. Native opacity multiplies retained
surfaces; overlapping per-command paint still requires consumer partitioning. Parents precede
children in declarations, subtrees are contiguous and parent raster content precedes descendants.
Both execution paths validate ownership/order before native mutation. Coverage tint remains a leaf
capability, and root presentation has no parent. Fixed-cap batch preflight uses the same declaration
checks. Complete popup/section integration remains migration work.

`reach_motion_equal` and `reach_render_group_equal` compare named intent fields, including clocks,
generations, limits, geometry, hierarchy, clips, tint and repetition. Both retained backends use
these shared predicates for property submission and root completion ownership. Object padding is
not animation intent; rebuilding an equivalent descriptor does not restart animations or submit
properties. New generations and changed physical timelines still invalidate the appropriate state.

`reach_motion_clamp` bounds a derived motion while retaining its canonical event time, easing,
generation and completion deadline. Affine mappings transform its limits as well as its endpoints.
CPU sampling applies the limits; `reach_motion_compile_at` provides up to four cubic/constant
segments at easing and bound crossings for both native renderers. Native keyframes end at bounded
values, and fixed-cap dimension normalization preserves the same limits. Retargeting samples the
bounded visible value before starting a new ordinary intent. This shared curve capability preserves
delayed/saturated section fades; complete reveal consumer integration remains migration work.

`reach_render_push_stretched_chrome` partitions fixed raster source bounds into up to fifteen retained
groups. Corners retain unit scale, straight edges stretch on one axis and the center stretches on
both. Canonical dimension motion determines patch placement and scaling about a declared anchor;
source crops remain unchanged during resize and reversal. Geometry and source-content validation
reject insufficient corner/shadow caps, compact destinations and text/icons before modifying the
command buffer. Ordinary chrome uses three columns; focal chrome uses five, retaining a rigid
central feature while its straight shoulders stretch independently. Horizontal notches must fit
that focal band or a fixed edge cap, including shadow coverage. Physical source coordinates and
caps are integer pixels. `reach_render_stretch_patch` exposes deterministic geometry and its count
API exposes the nine/fifteen ID allocation. Dimensions validate their full mapped ranges.
`reach_popup_width_motion` shares monitor-constrained body/notch intent with event-time placement.
`reach_popup_push_width_background` selects rigid focal caps when the complete motion range fits;
compact ranges use a retained shape-shadow caster and separate opaque border/background fills.
Rounded body clips and translated/scaled notches share canonical width/anchor intent. The notch
extends one physical pixel into the body while preserving its exterior slopes. Disabled shadow
material creates no caster. `input.bounds.width` supplies a stable source envelope, enlarged to
cover the full motion range; same-envelope retargets reuse source pixels and scene ownership.
The helper reserves sixteen consecutive group IDs, including an optional final scissor wrapper
around the complete output and shadow. Combined parent ownership, IDs and capacity are validated
before an atomic append. DirectComposition rounded clip containers use soft border mode; ordinary
rectangular clips restore inherited mode. Dock/Switcher consume these capabilities; complete
product appearance/cost acceptance remains migration work. See `docs/gpu-resize-reveal-design.md` and
`docs/gpu-compact-shadow-design.md`.

Explicit render groups can tint their rasterized alpha coverage with four normalized RGBA motion
channels. Windows Composition retains a surface mask, color brush and property-set expression;
DirectComposition retains a premultiplied color-matrix effect and uses Visual3 opacity separately.
Content changes redraw coverage; color-only motion changes native properties. Disabling tint or
retiring a group releases its tint objects. The HWND fallback renders aggregate coverage into a
bounded temporary bitmap and applies sampled color once, preserving overlapping shapes.
`reach_render_group_tint_at` and all fallback geometry sample one timestamp per frame. This
capability belongs to explicit groups; root presentation tint is not implemented.

Coverage tint can additionally blend its animated RGB with a fixed RGB target through canonical
`tint_rgb_mix`, preserving the independent alpha coverage. Windows Composition evaluates the blend
in the existing color-brush expression; DirectComposition chains a second color matrix after the
coverage tint. Fallback samples both intents at the same frame timestamp. Normalized blend limits,
semantic equality, native cancellation and repeating-property retirement share the retained contract.
The mix is a leaf capability and is rejected on root presentation.

Clipboard item feedback uses canonical hover motion and item-keyed retained source identities.
Shared bordered feedback consumes `reach_render_bordered_background_shapes`, the same pixel-aligned
opaque outer/inner geometry used by ordinary bordered backgrounds. The RGB blend preserves the
previous nested hover border colors without overlapping translucent rounded layers. Close background
and glyph coverage use separate retained opacity. Hover tracks, sampled render values, tick redraw
and hover-only frame demand are removed; sizing and scrolling remain separate CPU consumers. Stationary
Clipboard and Settings scrollbar drags retain event-driven pointer capture and movement without
requesting periodic frames; actual sizing/scrolling and renderer fallback work retain their own
active scheduling.


Optional process-wide workload diagnostics count application wakes/updates, scalar/loader/scroll
work, layout and command production, renderer executions and actual raster passes, compositor
submissions, retained visual-tree mutations, scene content matrices, and native-thumbnail calls
separately. `REACH_WORKLOAD_REPORT` enables collection at
startup; Reach and Settings write separate aggregate JSON summaries on ordinary exit. No diagnostic
timer or reporting worker is added. Retained-surface bytes are an estimate, not total GPU allocation;
system CPU/GPU, power and presentation latency require independent measurement. See
`docs/gpu-workload-baseline.md`.

Shared pressable feedback publishes absolute-time motion through a single motion pointer. Press
retargets with the theme `press` spring and release with the `release` spring, whose slight
overshoot past rest is clamped by bounded tint and opacity channels; every retarget, including a
context latch, starts from the current value and velocity. Feedback styles carry only the motion
and pressed amount, never timing. Battery retains one fixed
saver-row overlay through `reach_ui_feedback_render`; Quick Settings uses the same primitive for
its current pressed target, including output-row scissoring. Press and release events publish an
opacity descriptor. Each capsule owns capture and original-target activation, resets feedback on
hide, and requests no periodic frames for feedback. The existing generic fallback samples the
same command-buffer motion until its deadline when retained rendering is unavailable.

Settings buttons publish the same pressable motion into shared UI controls. Ordinary buttons
retain a background tint; uninstall buttons retain background and text tint independently. The
model keeps original-target identity and retires settled feedback at the next input. Settings
schedules all command-buffer motion only when retained rendering is unavailable, including one
final endpoint frame. No button-feedback scalar manager, per-tick advancement or compositor frame
demand remains. Target introduction, layout, enabled state and theme changes still redraw content.

Settings' animated binary toggles use `reach_ui_toggle_motion_render`: one stationary viewport
owns separate cached track coverage and knob sources. Shared RGBA tint changes the track color;
an affine X motion translates the knob without scaling it or moving the viewport clip. Sources
use the common pixel-aligned retained-command bounds. Display preferences, top-bar auto-hide,
Wi-Fi/Bluetooth radio state, startup entries and power wait options own canonical `reach_motion`
intent with event-time reversal. The power wait label's alpha shares its toggle timeline. Their
scalar tracks, advancement and frame demand are removed; Settings' existing fallback scheduling
samples the same published descriptors. Immediate stateless toggles retain the static shared
control. Settings row expansion and segmented selectors still use scalar tracks.

Caret phase, duty cycle and generation belong to `reach_blink`. Focus and edit events publish a
new visible interval/generation. `reach_blink_opacity` projects that owner into canonical discrete
repetition; fallback deadline sampling leaves the same epoch/generation unchanged. Power/Account
keep 0.55/0.45-second holds; Wi-Fi and Launcher keep 0.53-second intervals. Shared textbox rendering
publishes a retained stroke part only for the focused visible field. Settings and Launcher suppress
caret updates/deadlines on supported renderers, so blinking performs no application command build,
layout, rasterization or submission. Input and content changes still render normally. The generic
surface context supplies Launcher capability through its existing registry seam.

Page changes and blur stop inactive owners; ordinary Launcher close/reset stops its owner. Offscreen
Settings fields publish no caret part, and minimized rendering/surface suspension retires native
bindings and sources. Font/source recreation preserves the absolute phase and generation. These
visibility paths retain the existing logical timing policy rather than introducing another clock.
Unsupported rendering samples the same phase at visible-field deadlines, skips missed flips and
returns to idle between boundaries; native mode never advances the fallback snapshot for blinking.
The compound drawing path remains necessary for that unsupported renderer. Source/consumer checks
pass; displayed timing and total product workload/power acceptance remain migration work.

`reach_render_push_width_text` associates one ordinary text command with a dedicated rectangular
viewport whose right inset owns layout width. Its text envelope covers the complete width range;
left/top/bottom insets stay zero. Core preflight rejects ambiguous/missing ownership, unsupported
clips and shape-shadow ancestry. The native planner prepares DirectWrite trim states in retained
raster blocks and selects rows through canonical finite lookup. Independent alignment translation
preserves text size. A bounded 32-entry recipe cache reuses the existing font/format/layout caches.
Static fields use one ordinary source; active fitting prepares visible blocks and reuses prior
ones. Owner-based internal namespaces preserve surviving source IDs across command reordering.
Command scissors wrap output before internal alignment/row translation. Retiring a static text
raster restores DirectComposition's inherited interpolation mode before adding descendants.

The planner enforces the existing source-byte, dimension and native-layer limits while staging each
field. Unsupported/over-budget text uses an ordinary source sampled from the same width. Generic
render-port demand includes its required final update. Reach and Settings can sample only that
source when no other work is dirty, without capsule arrangement or command production. Suspension,
source removal and font recreation clear sampled-source ownership; finite endpoints return both
loops to idle. Switcher consumes these sources for centered labels and Dock for leading titles
during fitting. Complete intermediate product verification and total CPU/GPU/power/memory/latency
acceptance remain migration work. See `docs/gpu-text-width-design.md`.

Canonical motion includes a finite normalized exponential-out response used by the shared
scroll retarget policy. CPU sampling and source-turnover crossings use the same absolute descriptor;
the native compiler approximates it with bounded cubic Hermite pieces, retaining existing map/clamp
boundaries and returning no native curve when its fixed segment budget is exhausted. Consumers
must keep that fallback finite and preserve their own input, extents and source-lifetime policy.

Canonical motion also carries a damped spring (`REACH_EASING_SPRING`): response period, damping
ratio 0.5-1 (1 is the exact critically damped case) and initial velocity in absolute units.
`reach_motion_spring_start_at` derives the finite duration from response and damping alone: the
time at which the damped oscillator's decay envelope bounds displacement below 5e-6 of the
amplitude `|from - to| + |velocity| / omega`. Every spring with the same parameters therefore shares
one duration regardless of span or velocity, so same-epoch derivations compare durations exactly;
the deadline snaps exactly to `to`, so completion and `wait_ms` are unchanged.
`reach_motion_spring_to_at` and `reach_animation_manager_spring_to_at` retarget from any finite,
repeating, mapped or clamped curve, preserving value and `reach_motion_velocity_at` at the event
time. `reach_motion_spec` names either a spring or a legacy curve. Range, crossing, mapping,
lookup, combination and clamp helpers treat the spring's raw domain as including its analytic extrema and split time at
turns and raw boundary crossings, so overshoot is supported rather than assumed monotone. Native
compilation uses adaptive cubic Hermite pieces with an analytic fourth-derivative bound, split at
velocity zeros, within the existing 32-piece cap; exhausted budgets return no curve. Repeating
springs and normalized products of zero-span springs are rejected without output mutation.

Reach has one motion vocabulary: `reach_theme.motion_presets`, indexed by `reach_motion_preset`
and read through `reach_theme_motion_preset`. Springs `snappy`, `smooth`, `bouncy`, `press` and
`release` store only response and damping, so their settle time stays derived by the engine; the
`fade_in`, `fade_out` and `menu_fade` ease-out curves store a duration. `reach_motion_spec_scaled`
multiplies response and duration by one time scale, and `reach_theme_stage_motion_scale` derives
that scale from the persisted Stage duration relative to `stage_motion_reference_seconds`. Features
migrate from their legacy per-feature seconds to presets incrementally.

Every size, envelope, clip, raster allocation or budget derived from a motion uses
`reach_motion_range`, never endpoints or the current sample, so spring overshoot stays inside it.
Host presentation frames size the window from the scale range and the union of the declared
translation bounds with the x/y ranges. Fitted solid fills grow their source to the range maximum;
fitted images, width-text viewports, corner radii and popup extents clamp at their source or natural
limits. Endpoints must still fit the declared source; only the excursion beyond them is absorbed.
Bounded channels clamp by construction: `reach_render_unit_motion` limits opacity and tint to [0, 1]
before native submission only when the range leaves it, and CPU sampling clamps the same channels,
so monotone descriptors and settled frames are unchanged. Hit testing samples the event-time value.
Code that rescales `from`/`to` directly must scale `spring_velocity` with them.

Each retained submission samples initial native property values and compiles finite/repeating
curves from one captured timestamp. Renderer-owned source sampling uses the same scoped policy.
Presentation completion observes its deadline at the current time and scopes its settled property
publication separately, so a completed submission cannot retain a stale timestamp.

Native raster planning preserves physical pixel phase for fitted text and isolated vector icons
under fixed unit-scale ancestry. Leaf sources containing only text and vector paint also align
their birth envelope through unit-scale native translation ancestry, using the submission's
captured timestamp. Each layer keeps its raw requested envelope separately from the aligned raster
origin. Matching paint/envelopes reuse that origin through finite retargets, clip/tint changes and
compatible reparenting. A finite-to-constant translation cut realigns once at the legitimate cleanup
submission; static reparenting retains reuse. Attempted synchronization invalidates that layer's
alignment metadata, and only a fully successful synchronization publishes replacement metadata.
Alignment participates in the aggregate source/layer budget. Planning reapplies existing finite
fit fallbacks until the aligned allocation fits; each retry must remove an expansion, and an
irreducible overflow rejects before layer synchronization. Sampled-source ownership is published
after those decisions settle. No application frame or extra animation clock is added.
Explicit solid-clip fill owners
can bake fixed rounded geometry into their source; arbitrary viewport clips retain unchanged source
pixels. CPU fallback draws the same sampled fill directly
and retains ancestor clipping/tint. Moving geometry retains native clips.
This preserves settled corner/control coverage without changing canonical intent or adding frames.
WUC axis-scaled raster pieces select hard quad edges, preserving the alpha already in their source
and avoiding seams between chrome pieces; source retirement restores inherited edge mode.

`reach_render_push_rect_motion` publishes one translation owner and one rounded clipped paint
source. Width/height insets preserve circular corners and permit collapsed endpoints without
scaling borders or content. Its explicit solid-clip command owns one raster with no descendants;
the marker uses existing command padding. Stable source envelopes bound allocation; collision,
parent and range checks reject before append. A stationary outer clip preserves an inherited output scissor.

`reach_render_push_image_motion` shares the fitted-rectangle ownership and stationary scissor path.
Its rounded viewport fits an ordinary cached bitmap through native scale/translation using the
bitmap's actual dimensions; crop-to-fill preserves a centered crop, while ordinary image fitting
uses independent axes. Constant image axes retain bitmap snapping. Fixed endpoints reuse ordinary
raster drawing, and collapsed images draw nothing. Unsupported independent crop clocks, tinted
sources or over-budget expansions use renderer-owned source sampling through the existing render
port until the finite descriptors settle. Image and text expansions share the existing 128MiB and
layer budgets. Source changes invalidate their bitmap, while hide/retirement removes fit ownership
and sampling. No fields are added to the render command/group/buffer for this capability.
Clipboard item width now owns one canonical 160ms ease-out motion and one cleanup deadline.
Endpoint layout stays authoritative; pointer operations sample a separate current-width snapshot.
Normal scrollbar-gutter changes retain fixed-radius item borders, independent hover paint, close
controls, image crops, custom preview line variants and scrollbar placement. Stable item source
identities reserve 128 IDs each. The renderer retains at most five visible items and four bounded
line-capacity variants per item; no width-only capsule frame or layout tick advances the effect.
Monitor/DPI constraints and hidden initialization cut the width. Extremely narrow, horizontally
clipped close glyphs use generic active-only content sampling until the same canonical deadline.
Height and smooth scrolling remain CPU consumers pending their own migration.

Width-text callers own their ellipsis policy. Dock and Switcher explicitly request DWrite ellipsis;
Clipboard preserves its custom line splitting and literal dots. The vertical-overflow fit mode
expands native source bounds using DWrite glyph overhang while retaining horizontal width clipping.
It preserves the ordinary CPU text fallback's unclipped glyph ink instead of applying the metadata
viewport there. Sampled native sources bake horizontal clipping into their raster; active sources
retain bounded vertical ink and animate the width clip. This keeps consumer layout height separate
from the glyph paint extent without adding another animation owner.

`reach_ui_motion_background_render` composes these fills with an optional shape-shadow caster and
opaque outer/inner border semantics. Constant paint axes keep the existing pixel-aligned border
recipe while animated axes retain continuous geometry; the shadow caster keeps its original shape.
Popup width chrome reuses the same primitive for compact
body fills; notches retain their independent rigid geometry. Settled compact chrome and constant
common backgrounds use ordinary cached sources; animated ranges retain the shape graph.

Dock and Top Bar press feedback emits a separate flat retained group, copying item placement
for moving icons. Shared UI feedback owns the group descriptor; the icon helper retains its
original alpha-mask tint, while trigger and overflow glyphs use RGBA tint. Press feedback no longer
owns scalar tracks, sampled render values, capsule frame demand or bar content-frame blockers.
Original-target capture and context-feedback latching still use the shared pressable contract.
Top Bar power hover publishes retained RGBA tint over fixed rounded background coverage, with
its glyph in a separate cached group. Enter/leave retarget the canonical motion; reset and game
mode clear it. Power hover has no scalar track, periodic capsule frame or bar frame blocker.

Stage close hover uses the shared RGBA tint with separate background and glyph coverage for its
current target. Pointer events publish changes directly, including interruption; settled movement
does not resubmit feedback. Source retirement and hide/reset clear the canonical motion. Stage
geometry, backdrop and native close choreography remain separately owned scalar/native consumers.

Dock preview close buttons retain separate background groups keyed by source window identity.
The capsule publishes affine hover opacity and keeps the glyph outside that group. Close hover
owns no scalar track or sampled accessor; hiding/reset clears its motion. Preview header hover now
uses canonical retained opacity, and the square border uses the shared RGBA tint. Both are keyed by
window identity and use shared pixel-aligned source bounds. The border drawing helper makes its
painted alpha opaque, so the visible interpolation is affine. Enter/leave and identity semantics are
preserved; ordinary menu rows keep immediate feedback. The header hover scalar track/accessor and
its frame demand are removed. Width and reflow share canonical retained geometry, as described
above. A contained single rectangle in the tint fallback draws directly with its effective color,
preserving fractional-position
edge antialiasing and avoiding an unnecessary intermediate coverage bitmap.

Top Bar tray and Dock icon groups use stable item identities. Pointer events publish direct
positions, reordered neighbours and released items run compositor motion, and a stationary drag
does not request frames. Dock insertion/removal fitting and Now Playing width changes still own
CPU layout; unaffected item and control groups reuse their rasterized content. Bar reveal and
Stage's native thumbnail synchronization keep their existing CPU geometry paths.

Shared popup transitions, used by Launcher and System HUD as well as the popups, publish a
retained surface presentation and an absolute lifecycle deadline. The generic runtime applies the presentation to
the complete scene, including backdrop, chrome, and shadows, inside a stable native envelope.
Both host hit testing and native input regions sample the same motion on pointer events.
Optional canonical root scale-center motions preserve a moving physical pivot inside that stable
envelope. Capsules publish center coordinates in content space; generic frame stamping transforms
them into renderer and native input coordinates. Host inverse input, Windows nonclient hit testing,
CPU drawing and native preview projection sample the same center. Both compositor drivers bind the
existing scalar compiler to native center properties. The two descriptors live once per command
buffer, rather than in every group; ordinary presentations keep the content-bounds center.

Windows Composition uses scoped-batch completion keyed to the current presentation generation.
Completion waits for canonical root motions, including scale-center movement while visible.
A fully transparent root retires its remaining translation, scale and center motions once opacity
finishes, so an invisible spring tail cannot extend close latency. Suspension stops both center bindings.
Changing any root motion retires old completion
ownership and aggregates the current remaining curves. A hidden or occluded target can defer its
animation callback: after the canonical deadline, the adapter submits final properties once, awaits
`RequestCommitAsync` through a weak-lifetime callback, then synchronizes before close delivery.
Callbacks wake the existing message loop; retarget/reset retire the pending commit and stale
generations cannot finish newer motion. The runtime already waits on the capsule deadline, so
this reconciliation adds no timer or periodic application frames. DirectComposition retains its
one endpoint commit and compositor synchronization before close delivery. The render port reports
pending or complete separately from terminal native errors. Errors are latched for the submitted
motion; retarget or suspension retires them. The runtime suspends failed composition and releases
ordinary capsule close cleanup, and stops querying once the capsule clears its lifecycle deadline.
HUD's fixed lifetime uses its deadline independently of completion, so the hold
requires no animation frames and another action restarts the complete lifetime. Launcher caret
blinking uses native retained opacity when supported and deadlines in fallback. Renderers without
composition sample the same finite group contract
in a duration-limited CPU fallback; repeating marquee motion remains static there.

Launcher owns one canonical pixel body-height motion (theme `smooth`) and one endpoint cleanup
deadline. Every attach, detach and result-count change retargets it from its current value and
velocity; chrome, the results clip, input bounds and published geometry are views of it, and the
published visible bounds cover its range until it settles. Results are clipped to the rounded body
interior (`clip_radius` with a uniform radius, bottom inset riding the height), with a stationary
rectangular viewport below the search field. Result content is not slid: it fades with
`fade_in`/`fade_out`. Rows are keyed by result identity through the shared
`features/common/keyed_rows` pool (sixteen slots, each holding its own result copy so a retiring
row still draws): survivors retarget with `smooth`, inserts fade in at their slot and removals fade
out in place. Rows and the selection live in one content frame offset by the stepped scroll
position. Selection is one shared `features/common/selection_pill`: a one-row step uses `bouncy`,
larger jumps and result-set changes use `smooth`. The selected title crossfades through a retained
tint, a hover tint and the shared press overlay sit in the same content frame, and the search glyph
fades with the query. Event-time input samples the same height motion; unrevealed content cannot
activate. Scroll motion is deliberately absent: scrolling stays stepped until the shared scroll
component exists. The shared sampled fallback consumes the same command descriptors.

Text-dependent layout consumes `reach_text_measure_port`. The DirectWrite renderer supplies the
implementation with the same font family, size, and weight used for drawing; features retain only
a conservative estimate for unavailable-adapter cases. This keeps exact text metrics outside
feature policy while allowing text-dependent surfaces such as the top bar to size before rendering.
Applying a different UI font invalidates both rendering and layout so the next normal frame
remeasures every text-sized surface. Dock preview card widths depend on source geometry, and their
headers ellipsize titles within the remaining space beside the close control.
All Reach-rendered text selects from the shared scale in `reach/core/typography.h`

The window push is the top bar's second private subfeature. The animation manager remains the sole
owner of motion: each frame, the top bar derives the current app-facing edge of its reserved band
from the manager-owned animated Y position and resolved style geometry. Each affected window stays
at its captured origin until that edge reaches it, then its outer top edge is clamped to the moving
edge through the final target; hiding reverses the same relationship. The full frame path and the
`reach_host_move_bar_animation_frame` fast path both provide the animated Y position. The
definition's optional `reach_bar_reveal_ops.position_frame` hook keeps this feature-owned effect
active while the generic path moves the surface without a redraw. Window push does not own a second
animation or derive its position from normalized reveal progress.

It **moves** windows and never resizes them. A resize makes the app relayout its
whole client area on every frame, cross-process, which is the one cost this path
cannot afford — the apps could not keep up with it (10–13 ms per step against the
bar's 5–9 ms). A move costs the app nothing; the window keeps its size and its
bottom simply runs past the screen edge until the bar hides again. Nothing is
clipped permanently and nothing reflows. Moving a maximized window is allowed and
exact: it stays flagged maximized (`WINDOWPLACEMENT.showCmd == 3`), does not snap
back, and lands on the requested position to the pixel on every frame.

Each window animates onto the band's lower edge, which is where its **outer** top
edge lands — a maximized window's rect overhangs the monitor by its invisible
resize border and the app paints that overhang, so aligning anything else puts
that painted border against the bar and eats the gap. The depth of that band
comes from the top bar's own layout — the screen gap above the bar, the bar
height, and the style's app-side clearance below it — not from the bar rect alone. Split and
unified float between the screen edge and the window they push; simple meets both. Only
windows centred on the bar's monitor are pushed; a window on a neighbouring
monitor that merely clips into this one is left alone. It stores each window's
original position at capture and animates back onto it — Windows never moves a
window back on its own.

That stored position is the only record of where a window came from, so a shell
that dies mid-reveal loses it. `reach_top_bar_push_recover` covers that once per
attach: a maximized window always rests at its monitor's origin, so any maximized
window found resting anywhere else is put back before the first push. It is the
only recovery this path needs.

The motion goes through `app_control`'s window ops
(`reach_app_control_window_bounds` / `reach_app_control_window_frame_bounds` to
read, `reach_app_control_move_windows` to write, one `SetWindowPos` with
`SWP_NOSIZE` per covered window per frame in the adapter). The flags are
`SWP_NOSENDCHANGING` so a maximized window cannot clamp the move back, and
`SWP_ASYNCWINDOWPOS` so a slow app cannot stall the frame. `BeginDeferWindowPos`
cannot serve this path at all: `DeferWindowPos` rejects `SWP_NOSENDCHANGING` with
`ERROR_INVALID_PARAMETER`, and without that flag the move is clamped away. These
are the only synchronous window ops on that service — the `schedule_*` ones go to
its worker thread because activate/minimize/close can block on another process,
while a push that misses its frame is worse than useless. They also work in the
outer window rect the OS repositions, never the DWM `frame_bounds` the rest of
the shell measures with; mixing the two drifts by the invisible resize border.
Dynamic mode deliberately leaves the work area alone: changing it costs ~37 ms per call,
which no animation-frame path can afford. Static mode disables reveal and window push, keeps the
bar shown, and publishes work-area reservation intent with its surface geometry. The generic frame
path derives the reservation from the resolved bar bounds, explicit app-side clearance, and current
layout monitor. The monitor adapter caches the
applied geometry, changes the OS work area only when that state changes, and reconciles visible
maximized windows on the affected monitor while preserving their normal restore bounds.

Every popup gets its bounds and its notch from one place —
`reach_popup_place(anchor, width, height, margin)` in `features/popup`. It
centres the popup on the anchoring control, clamps it into the monitor on both
axes, and returns the notch anchor in screen space, so the notch keeps pointing
at the button that opened it after the clamp moves the popup. Quick settings,
the tray popup and the context menu all place through it; a popup that clamps
its own bounds or re-derives its notch at render time has reintroduced the bug
this helper exists to prevent.

`features/common/layout` is the other shared state machine: a pure registry of
participants, layers, conditions and visibility intent with a `resolve` that has no
OS types, no ports and no host include. Composition owns every layer number and the
one pass that applies a resolved plan; the manager itself only answers what the
arrangement should be. See the composition section for the model.

Dock and top bar are both bars: each owns a `reach_bar_visibility_state` driven
by the shared `features/common/bar_visibility` state machine, and composition
reconciles both through one `reach_host_reconcile_bar_visibility` over the
definition `bar_reveal` spec and feature-owned `reach_bar_reveal_ops`. The spec
declaratively supplies dynamic edge-reveal and active-layer policy; the capsule
names its own edge and caches whether a tracked app intersects the protected band
returned by `reach_bar_protected_band`. The symmetric policy band reaches from the
screen edge through the bar's content bounds and its protected clearance. A bar can
declare that clearance in DP when policy should not follow a large rendered shadow;
the top bar declares 18dp, while the Dock retains the theme's resolved per-monitor
shadow extent as its default. `window_tracking` supplies the one outer-bounds trespass
query used by both bars. The resulting hide policy is the same even though reveal
presentation differs: the dynamic top bar rests in the app band and uses
`reach_bar_reveal_ops.position_frame` to push trespassing windows, while the
permanently-topmost Dock reveals over them without a side effect. Static top-bar mode
bypasses the visibility state machine, pointer observation, edge hotspot, and window
push, and normally holds the surface in the active topmost layer. While a foreground
window occupies its whole monitor, static mode keeps the work-area reservation but
yields its layer and native z-order behind that window. Dynamic mode retains its own
hide and reveal behavior. A single
screen-hotspot factory creates every
definition-declared edge reveal. Fixed triggers declare an anchor and DP size;
Stage is a normally enabled 4dp top-left square. Animated bars publish managed
bounds from the shared visibility result. Every trigger is suppressed while a relevant
window manipulation is active. Generic runtime loops own geometry,
callback binding, event dispatch, bounds caching, layout registration and teardown.
The `bar_reveal` capability also owns pointer-exit wake-up. Surface leave remains
a wake-up, but a hideable shown bar additionally publishes its bar-plus-bridge
observation bounds. The Windows input adapter installs a passive low-level mouse
hook only while at least one such region is active and posts only membership
changes; it neither consumes input nor polls. Both this hook and the popup
capture hook run on the shared hook thread in
`src/adapters/windows/mouse_hook_thread_win32.cpp`, never on the UI thread:
Windows dispatches a low-level hook on the thread that installed it and drops
packets that thread does not service within `LowLevelHooksTimeout`, so hooking
from the UI thread would put every mouse event in the session behind the frame
loop. The hook thread only pumps messages, which is also why the input adapter's
region table is guarded — the hook reads it off-thread. This covers the interval after the
thin edge hotspot is hidden, including a top bar that cannot receive a reliable
surface leave because another window owns that part of the screen.

Both answers are cached, so what makes them correct is that every path which
learns the window world changed goes through one function,
`reach_host_refresh_window_world`: it invalidates every capsule's trespass cache
through the generic bar capability, refreshes the manager, and refreshes the
open-window list. The three callers are the `needs_refresh` path in
`reach_host_update`, the `WINDOW_STATE_CHANGED` event, and the completion of reach's own window
operations (`reach_host_apply_window_control_result` — snap, minimize, close).
A path that refreshes without invalidating is the failure this shape exists to
prevent: it leaves the bars deciding from a stale answer, and only an unrelated
foreground change heals it.

Window manipulation is a separate shared-service state, versioned with its own
sequence. Interactive move/resize start publishes the manipulated app and every location
change republishes the active session. On each event, the host reads the pointer once and
makes a tracked app relevant exactly while that pointer is inside the primary monitor;
failed pointer reads retain the previous answer. Entering suppresses both bars,
dismisses open popups, excludes that window from top-bar push, and keeps all edge
reveals disabled; leaving re-enables them under normal policy. Manipulation end publishes
the final window snapshot and clears the state. Reach consumes Win+Arrow and snaps
asynchronously with `ShowWindow` / `SetWindowPos`, so it explicitly brackets that
operation with the same host suppression lifecycle instead of expecting Windows
move/size-loop events.

Two separate inputs decide how another open surface affects the bars, and both
apply to both bars identically. A surface that declares `bar_shown_while_open`
_forces_ them shown for as long as it is *presented*, not merely while its capsule
reports open: `reach_host_surface_presented` is `is_open || presentation_visible`.
The second hook is explicit capsule-owned presentation lifetime, so the force survives
the whole close animation and lifts on the frame the surface actually leaves without
conflating background frame scheduling with native visibility. Stage is the only such
surface, because it is a window overview and wants
the bars in frame. The distinction is load-bearing rather than pedantic: the Stage
capsule reports closed the moment `reach_stage_begin_close` runs, while its tiles
keep animating for the full close duration, and dropping the force there demotes
the top bar out of the topmost band mid-animation — `HWND_NOTOPMOST` lifts it to
the top of the app band, under the still-banded Stage, so the closing overview
covers it until the animation ends. Any open popup instead only
_holds_ them: a bar already shown stays shown, but opening a popup never summons
a hidden bar, and the hold lifts the frame the last popup closes. Everything else
leaves the bars to hide on their own rules — the launcher and the clipboard are
pure transients that own the screen while they are up, and the switcher is the
sole `OVERLAY`. The switcher exclusion is deliberate and differs from
`reach_host_window_list_blocked`, which does count `OVERLAY`: the dock hover menu
is suppressed during alt-tab while the bars still hide. Manipulation suppression
outranks popup hold and pointer sequences, but Stage's force-show remains higher
priority. Game mode retains its separate immediate cut; ordinary bar changes use
the shared reveal animation.

## composition

The host (`reach_host`): wires adapters into ports, constructs services and features,
and runs the app — frame loop, input routing, boundary action dispatch, worker threads,
and surface lifecycle. Concrete feature knowledge has two controlled seams:
`feature_registry.cpp` for definitions and `interfeature_routes.cpp` for the callback
slots that let one feature's action affect another. Those are the only two files in the layer that
include a concrete feature header or call a concrete feature API, and
`tools/check_architecture.py` rejects either anywhere else outright — there is no baseline and no
exception list. `reach_host` holds no feature-named state: surface runtimes are arrays indexed by
`reach_surface_id`, so adding a surface adds no field. `feature_registry.cpp` is the seam for
feature definitions: each registered surface has exactly one immutable
`reach_feature_definition` and one `reach_feature_runtime`. The definition owns its opaque
create/destroy factory, capsule operations, surface operations, layout, and policy. The runtime
contains only the bound surface, capsule, resolved geometry, native-overlay state,
and a pointer to that definition. A registered surface joins every loop by being in that table:
lifecycle in `reach_host_create_with_dependencies`, layout in `reach_host_init_layout`,
presentation completion in `reach_host_finish_surface_presentations`, input ordering in
`reach_host_pointer_order`, and the frame pass in `reach_host_update` each iterate it whole, and
`tools/check_architecture.py` rejects any of them being narrowed to a hand-maintained feature
list. `reach_host_feature_capsule<T>` derives any concrete view from that
capsule; `reach_host` carries no parallel typed capsule pointers. Host construction and cleanup
iterate the factories generically; a shared surface such as the tray declares no factory and
binds to its owning top-bar capsule. Definitions also declare opaque lifecycle operations for
dependency attachment, start, and stop. Composition supplies one service dependency bundle and
iterates every runtime; concrete attachment and start/stop calls remain inside the registry seam.
Definitions may also declare opaque control operations. The shared open/close path applies opening
policy, pointer subscriptions, popup capture, dirtying, and update scheduling around
the capsule's semantic `set_open`; registry adapters are the only code that translates that call to
a concrete capsule. Typed host notifications are broadcast through the same contract for external
state that more than one capsule may present, including system statistics, Now Playing, media
actions, volume, and brightness. Clipboard, Tray, Quick
Settings, Battery, System HUD, Launcher, and Switcher use these operations rather than named host
orchestration files. Closing carries an intent: `REACH_SURFACE_CLOSE_DISMISS` for a dismissal the
user drove, `REACH_SURFACE_CLOSE_SUPERSEDED` when something else took over. A surface that declares
`restores_focus_on_close` has composition remember the foreground window when it opens and
reactivate it once the close presentation has finished; a superseded close drops that window instead,
so a launch or a click on another surface keeps the focus it just took. `close_on_persistent_press`
declares the other half of that rule — a primary press a persistent bar handled supersedes the
surfaces that declare it. Both are surface-spec data, so neither the focus controller
(`host_focus_restore.cpp`) nor the pointer path names a feature. Control operations also declare
`surface_hidden`, called by the generic presentation tail once a surface has finished animating out,
which is where a capsule drops the state that must not survive a close.
Render-resource operations on the control contract expose retired and active opaque
renderer/source identities, so the host drains and releases icons in one runtime loop while each
registry adapter preserves its feature's ownership rules.
Window-event callbacks are bound by that same runtime loop and derive their source role from the
definition. Policy runs as class loops over the runtime table — tick, needs-frame, game
mode, lifecycle resets, pointer-move subscription sync, the popup mouse hook, transient
dismissal, and the “opening a popup closes the other popups” rule. Pointer input
uses one runtime-driven dispatcher for capsule delivery, surface dirtying,
relayout, capture, subscription sync, and update scheduling; capsules receive
the coordinate space declared by `reach_pointer_event.coordinate_space`: popup
capsules receive surface-local coordinates, while other surface classes retain
screen coordinates. Each pointer event kind
runs as a generic loop over the table in `pointer_priority` order (popups →
transients → persistent, first handled result wins), with the definition's
`role` resolving source-gated delivery, and its flags declaring the outside-press policy
(SOURCE_GATED / DOWN_CLOSES_ON_UNHANDLED / DOWN_APPLIES_UNHANDLED). Source-gated rows are delivered to first — press and release go to the row whose
`role` matches the event source (or whose `pointer_sequence_active` hook reports
an in-flight sequence) before the rest of the table sees them — so no surface
needs a hand-written branch to receive its own input. The source row is evaluated
against the same flags as every other row, so a surface that receives its own
press still closes or applies an unhandled result exactly as it would from the
loop. Capsules report outcomes as one shared `reach_feature_action_kind`
(`include/reach/features/common/feature_action.h`); `host_feature_actions.cpp` dispatches
every entry in a single generic switch keyed on the emitting runtime, so no feature has a
composition-side translator. A press that only composition's pointer policy reads is not an action
at all: the capsule publishes an opaque `reach_feature_control` naming which of its own controls was
pressed, and those same slot tokens serve as the anchor slots in `reach_feature_layout_anchor`, so
one vocabulary describes both "this control was pressed" and "this popup hangs off that control".
Controls and layout anchors may also carry a stable owner identity. Keyed owners compare that
identity instead of a mutable list index; unkeyed owners retain index matching. Dock menu requests
copy the application key with their immutable command payload. Generic anchor resolution follows
that key through reorder, and retains the last resolved original anchor if the keyed owner
disappears. It never substitutes the replacement item at the opening index. Hidden surfaces clear
the cached anchor. Continuous following during GPU-only owner motion remains migration work.
An action that opens or changes
another feature is not in the vocabulary at all: the feature exposes a neutral outbound slot
(`reach_dock_routes`, `reach_top_bar_routes`) and `interfeature_routes.cpp` alone decides what it
reaches, so no feature names a peer. Policy that depends on another surface being
open is derived rather than branched: `reach_host_popup_owner_trigger` resolves an
open popup's layout anchor and compares it with the control the source published,
so composition learns only whether the press landed on the control that owns that
popup. The popup capsule decides what that means and answers with neutral flags —
`continue_source_sequence` keeps the owner's press alive so its release can toggle,
`cancel_source_sequence` ends it, `CLOSE_SELF` dismisses — and composition executes
the flags without naming a feature. Capture pre-emption is generic in the same way:
the highest-priority surface whose `pointer_capture_active` predicate is set owns the
sequence, with `EXCLUSIVE_WHILE_OPEN`, `CAPTURE_CONSUMES_RELEASE`, and
`CAPTURE_OWNS_MOVE` declaring the differences between them. Hotkey and
action→port translators for media transport, volume, and brightness live in
`host_system_actions.cpp`, out of the input routing path. Brightness translation submits a relative
command to `reach_system_status`; the host uses only the cached target returned by the service for
immediate presentation. The resulting typed notification is offered to every registered control;
Quick Settings and System HUD consume the kinds they support. Successful media, volume, mute, and
brightness actions present System HUD independently of top-bar visibility and game mode. A confirmed
missing Dock launch target presents a one-line System HUD notification naming the app and stating
that it was removed. Successful level changes carry the exact post-action state; media actions make
the HUD refresh its presentation
snapshot from the Now Playing service. Its media presentation shares the cover-art rendering path
with the top bar: cropped artwork fills and blurs behind the item, then a translucent overlay keeps
static, ellipsized title and artist text legible. The HUD is a persistent, source-gated surface at layer
220, above every other Reach layer. It is fully pointer-transparent. Every presentation uses the normal
centered position above the Dock's shown-position geometry. Media and level presentations use a
1.5-second total lifetime; missing-app notifications use three seconds and cap the displayed app name
at eight characters so the complete one-line sentence remains visible. The closing fade completes
inside the applicable lifetime and a hard deadline makes the HUD fully hidden even after a delayed
frame. The whole render command buffer is faded by the shared animation manager.
A deferred launch is keyed on the surface that requested it, so composition waits for that
surface's own close presentation before running the launch rather than testing one named feature.
`app_control` publishes correlated asynchronous launch completions. Composition retains the generic
pin id and display name until completion and broadcasts a typed failure notification only when the
adapter classifies the target as missing; cancellation, access denial, and unknown failures do not
masquerade as an uninstalled app. Composition removes that pin 500 milliseconds after presenting the
notification, while the HUD keeps its copied display text for the rest of its independent lifetime.
Per-frame layout resolves in dependency order in `reach_host_update`, and every capsule owns its
own geometry: the Dock and the Launcher each compute their layout inside `arrange` during the frame
pass. Popup capsules, Launcher,
Clipboard, Switcher and System HUD embed the shared `features/common/presentation` transition and
choose only a style: popup scales 0.94 to 1 with a 6dp offset toward its anchor, and menu (Context
Menu and Switcher, anchored at the Switcher's center) scales 0.98 to 1 without offset. Scale and offset use the `snappy` spring and opacity the
style's fade; close retargets to a smaller rest (0.97 and 4dp for popups) with an ease-out fade.
Open and close retarget every channel from its current value and velocity, and completion and
visibility end at the opacity deadline rather than the spring tail. A capsule supplies its anchor
in surface coordinates with `reach_feature_transition_set_anchor`, or its trigger x through
`reach_feature_transition_set_trigger`; otherwise the anchor is the center of the edge facing its
direction. The anchor is published as the canonical root scale center. System HUD keeps its own
lifetime and starts the exit fade so that it ends on the lifetime deadline. Geometry publishes only
generic presentation values;
composition applies managed opacity to the complete rendered command buffer and, for scaled
presentation, maintains a stable maximum envelope plus matching render and pointer transforms.
Capsules whose close presentation outlives `is_open` expose that lifetime through
`presentation_visible`; `needs_frame` remains scheduler-only. Registry entries do not carry
animation direction or scale policy, and composition owns no surface animation tracks. The
per-surface frame steps
(`host_surface_frames.cpp`, layout refresh → presentation → window state →
corners → show/render) run as one loop over the table in definition `layout.priority`
order against a shared `reach_host_frame_context`. Presentation completion is runtime-driven:
composition observes capsule-owned presentation before delivering `surface_hidden` and restoring
focus. Every definition
exposes uniform `surface_ops` for
arrangement and render-command production. `reach_host_frame_registered_surface`
then resolves the declared layout anchor, applies window geometry and visibility,
and executes rendering without naming the feature. System HUD uses this path and
declares Dock as its anchor. The Dock's shown-position bounds are stored on
its feature runtime rather than in a HUD-specific host cache. Switcher also uses the
path: its capsule owns width animation, presentation, arranged bounds, and geometry publication.
Generic native-overlay placement resolves screen geometry into the same client content space as
drawn commands, including shadow and retained envelope offsets. It samples retained translation,
centered scale, and opacity from the existing motion clock. Runtime native-motion demand delivers
active projection samples and one terminal sample independently of content-frame demand. Switcher
and Context Menu no longer request content frames solely for preview-linked popup presentation;
Switcher width, fitted cards and native destinations share one canonical epoch within a stable
session envelope. Empty body parents translate retained rounded fills, fixed-thickness outlines,
proportional fallback icons and independently fitted label viewports. Card source tokens survive
visible-source reorder; monitor/DPI changes cut stale layout intent. Input bounds and native
destinations sample the body at event time. Width retargeting preserves continuity, continues during
the existing close fade and retires once at its endpoint or on hide. Context Menu publishes its
shared width/reflow epoch through the same body/input/native destination contract. Settled DWM content
updates independently. Stage keeps its capsule-owned exact placement and synchronized close semantics.

Each surface runtime caches its last arrangement inputs, canonical geometry and native projection
context. Unchanged presented surfaces skip capsule arrangement, command production and renderer
submission while another surface or native motion keeps the host awake. Native overlay generation
changes reconcile relationships without requiring a content frame. Environment, anchor, canonical
intent, window identity or renderer-capability changes invalidate the cache; ordinary dirty flags and
CPU/fallback frame demand retain their existing behavior. Each surface remembers whether its own
commands require feature sampling, independently of the shared command buffer and renderer-only
source sampling. Mixed fallback work rebuilds feature commands until their finite requirement ends;
renderer-only sampling reuses arrangement and command sources. Hide/reset clears this ownership.
HWND bounds changes may require arrangement
without forcing content redraw by themselves. Hide clears the cache, relationships and native-motion
demand. Synchronized presentation continues through the existing full path. This separates native
sampling from retained content work without feature-specific composition branches; broader layout,
input and consumer migrations remain pending.
Bar position-only frames also publish the terminal native position after the capsule's position
manager becomes inactive. The shared host compares the last published bounds with that endpoint,
updates the same native window and position callback once, and advances its cached layout bounds.
This avoids a completion-only arrange, command rebuild and renderer execution. Existing dirty and
content-motion guards still require the ordinary full frame when necessary.

Native overlay items may also publish a canonical destination rectangle. The runtime samples its
four properties at the same timestamp as root presentation, then applies the existing client-space
projection. Destination motion participates in active-only native demand and the terminal sample.

Dock width and reflow share canonical progress and derive constrained card rectangles by stable
window identity. Retained parents move the independent sources; thumbnails preserve aspect and
equal heights, while title widths, close widths and proportional glyph/icon sizes follow fitting.
Native rectangles and event-time item/close hit tests sample the same geometry. The fixed-geometry
preview-border path reuses the existing pixel-aligned bordered-shape recipe; active geometry uses
the published clips. Complete-card paging and source-loss handling retain the native overlay seam.

Retained render groups also publish four animated clip insets. Both Windows backends retain their
clip objects and submit changed edges without regenerating unchanged content; unsupported targets
sample the same descriptor. Battery, Quick Settings and tray popup press feedback publish
absolute-time motion into retained opacity layers. Those feedback effects no longer own scalar
tracks or capsule frame demand. The shared pressable scalar bridge has been removed.
Quick Settings's output-device/app-volume reveals, following-row movement and title/chevron
crossfades now publish retained motion. Popup height, both sections' pixel spans and expansions and
the scroll limit are quadratic views of one theme `smooth` spring (`reach_motion_multiply`/`reach_motion_combine`):
every retarget starts each channel at its current value and velocity and ends it at its settled
target, which alone carries the saturation limit. The spring's start rate is the candidate whose
channels stay in bounds and best match independent springs. Revealed content is not slid: each
section's bottom clip inset follows that spring, the clip carries the section's uniform corner
radius so its bottom corners stay rounded at every height, and its parent opacity fades with
`fade_in`/`fade_out` while individual paint leaves preserve overlapping alpha. Device and app rows
use the shared keyed rows, keyed by device or session id in a pool capped at the list maximum: survivors retarget
their y with `smooth`, inserts fade in at their slot once a pool entry is free, and removals fade
out with `fade_out` and are released with their held icon. When every section motion has settled,
the capsule tick restarts both spans as settled motions, so the section frame, which is sized from
the span range while it moves, returns to its settled size. Network and Bluetooth
tiles retain background, icon-background and foreground coverage tint between their off and on
colors on `fade_in`/`fade_out`. Source layout is stable; pointer events sample current geometry
before hit testing. One deadline retires closing section content, and force-close clears motion.
The capsule owns shared fixed-cap chrome, a moving body, a clipped viewport and a scroll
parent inside a stable session envelope. Wheel intent updates the scroll clamp; section source
extent changes reconcile it even when the final collapsed height is unchanged. Environment changes
cut old geometry and reopen resets scroll intent. The height manager, sampled render values and
height frame blocker are removed. Optional capsule input bounds use `reach_rect_motion`; generic
composition projects them into native client space and classifies events, and Win32 samples them
for hit testing after the root inverse transform. Pure retained height motion requests no periodic
application frames. Compact static chrome preserves the existing drawing path; animated geometry
below fixed-cap limits remains part of the migration's pending capability work.
Capsules with several moving input rectangles can publish `input_regions_at`. Generic host input
and the native window presentation carry this const provider and its opaque capsule ownership.
Both sample the provider at the same timestamp as their root inverse projection; native input
maps the returned surface-local rectangles into client coordinates. Gaps remain transparent as
the rectangles move, without periodic input-region publication. Hidden surfaces clear the native
provider, and window teardown precedes capsule destruction. Single animated bounds and ordinary
static input regions remain supported. The Top Bar width consumer is still being migrated.
Broader resize, color feedback and native-layout migrations remain pending in
the [GPU migration checklist](gpu-composition-migration-checklist.md).
Layout definitions can also reserve a top or bottom edge. A consumer that opts into reserved
bounds receives the remaining rectangle through the generic surface context; Stage uses this
contract after Dock and top bar arrangement rather than reading either feature directly.
Separately, a surface can publish `manage_monitor_work_area` and
`reserve_monitor_work_area` in its resolved geometry. Composition maps the definition's
reservation edge, resolved bounds, and published `work_area_clearance` to the monitor port's generic
`set_work_area` operation. The
Windows monitor adapter uses `SPI_SETWORKAREA` with change broadcasting, caches identical writes,
reconciles visible maximized windows on the affected monitor without changing their normal restore
bounds, and restores full monitor bounds when the surface is hidden, returns to dynamic mode, or the
adapter is destroyed. Game-mode suppression changes Reach surface visibility without changing the
applied monitor work area, so a static reservation remains stable across game focus transitions.
`reachctl` uses the same port when installation repairs monitor work areas. Monitor selection is not
configuration here: the reservation follows the monitor in the current layout context.
Clipboard declares Launcher as its anchor and likewise owns relayout, presentation, geometry,
and command production. There is no named frame fallback: every registered surface runs the
same frame function, and the architecture checker requires one runtime binding and one
`surface_ops` contract for every registered id.
Surfaces that take OS activation declare `BEHAVIOR_ACTIVATES`; the class rules then
own show-on-activate, close-on-focus-loss, and the staleness check that discards a
focus-loss signal the surface has already recovered from (reach's own foreground
handover emits one). An activating surface shows once through the
apply pass (idempotent via `surface->activated`) and never through a raw `show()` —
a fullscreen surface re-asserting `HWND_TOPMOST` every frame makes the desktop
unusable.

Z-order and visibility for the windows Reach owns are decided in one place:
the `features/common/layout` manager plus the apply pass in `host_layout.cpp`. Each
participant registers a base layer and, optionally, per-condition layer overrides,
and visibility overrides; `reach_layout_resolve` is a pure function of registrations,
the active condition set, and per-participant intent. Layer 0 is the app band — an
exit, not a slot, and Windows owns ordering inside it. Layers above 0 are the
topmost band, ordered among ourselves: the pass walks the plan in descending layer,
seeds the chain with `set_topmost(1)`, and chains each following participant behind
the previous with `place_behind`. It emits `set_topmost(0)` only on the transition
out of the band, never while a participant rests there — `HWND_NOTOPMOST` lifts a
window to the top of the app band, so a redundant demote would pop a resting bar
above whatever covers it. The pass is the sole owner of `show()` / `hide()` for
these windows; frame steps compute intent and render, and never touch visibility or
z. It emits nothing when the resolved plan equals the last applied one unless native
pointer interaction, a foreground change, or a completed window-control operation invalidated
the topmost chain. A pointer-down on a visible banded
surface schedules an order-only reconciliation because Windows may reorder clicked
HWNDs inside the topmost band; visibility remains cached and is not replayed. Each
semantic plan change emits ops only for the participants whose layer or visibility
actually changed.

Conditions are bits, not triggers: the arrangement is recomputed from the whole
active set, so setting an already-set condition is a no-op and a missed one heals on
the next resolve. `reach_host_sync_bar_layout_conditions` publishes the process-wide
`BARS_FORCED` and `BARS_HELD` values once before the surface frame loop; individual bar
reconciliation only owns that bar's visibility result and layer intent. `GAME_MODE` resolves every
participant hidden except a definition
that declares `BEHAVIOR_GAME_MODE_VISIBLE`. System HUD uses it because hardware media and level
keys remain active, and Switcher uses it so an Alt-Tab session can present while the game remains
foreground. `host_game_mode.cpp` owns the state and the main gate in
`reach_host_update`; the game-mode path runs only behavior-flagged capsules and
their frames, leaving all ordinary composition work below the gate dormant.
Game-mode Alt-Tab uses the ordinary Switcher transaction. Begin, navigation and cancellation do
not change the game window. Committing a different target runs ordinary shared activation; Reach
Service first transfers foreground to the registered shell desktop, minimizes the game and clears
the shared cursor clip, then activates the committed target. Committing the foreground game does
not minimize it. The Reach-owned Progman host permits the explicit desktop activation while
continuing to reject pointer activation.
The top bar is the only participant whose layer moves: dynamic mode rests at 0 and rises to
130 while its reveal transition is live, while a `bar_shown_while_open` surface is open, or while a
popup holds the bars; static mode holds layer 130 at rest unless its fullscreen-only auto-hide
policy is active. Starting a Y animation and
reporting its transition are the same act, performed by `reach_bar_update_visibility` alone. A mode
change may snap `REACH_TOP_BAR_ANIM_Y` to its shown position while disabling that visibility policy.
The layer intent is published generically from resolved capsule geometry and the visibility result.
Surface geometry selects an always, fullscreen-only, or disabled reveal policy. Reach Service
classifies fullscreen and fullscreen-game state on every event-driven window snapshot by matching
the window's extent to its hosting monitor while its center remains on that monitor. Position is
deliberately independent because the top-bar reveal translates windows without resizing them.
Window tracking resolves whether a visible, unminimized,
non-game fullscreen window occupies the bar's monitor without consulting foreground state. A
configured static top bar activates the existing reveal, animation, and window-push path only while
that fact remains true; its configuration and work-area reservation remain unchanged. Losing focus
therefore does not restore the bar over the video, while leaving fullscreen returns it to ordinary
static behavior. Definition-declared edge reveals are participants too,
attached to their owning surface runtime but independently visible; the underlying
screen-hotspot port carries `set_topmost` / `native_id` / `place_behind` so they
chain and seed like any other participant.
`system_stats` samples CPU, memory, and network rates; it no longer owns power state.
`system_status` owns the canonical cumulative power snapshot, reads it synchronously at creation,
and refreshes it on power-change requests independently of its slower system worker. The Windows
system-controls adapter subscribes to saver, battery-percentage, and power-source notifications,
including an initial notification, and unregisters them before teardown. Each process shares its
service snapshot between its consumers. Top Bar and Battery read that snapshot; Battery publishes
saver pending reconciliation through a neutral route bound to Top Bar in `interfeature_routes.cpp`.
Power-change requests and service-ready wakeups remain active in game mode while CPU/network
sampling is disabled. A failed power read retains the last successful value, and a delayed system
worker publication cannot overwrite a newer power snapshot.
Battery's two-second pending timeout and Top Bar's two-second Bluetooth absence grace are semantic
absolute deadlines. Capsules publish their next wake through generic surface geometry; Battery
merges its timeout with popup completion. Awaiting policy outcomes causes no periodic frame or
update request. Status acknowledgement/recovery and reset cancel stale deadlines, and expiry
reconciles the visible state once. The Bluetooth grace starts at the first observed absence and
repeated absent snapshots do not extend it. Neither timer advances from frame deltas.

The pure `reach_frame_rate_resolve` policy uses the main display's refresh rate when adaptive
animations are enabled, with a 60 Hz override while battery saver is on. Adaptive off or an invalid
display rate also selects 60 Hz, without changing the persisted adaptive preference. Reach and
Settings use the same policy and `reach_frame_clock` deadlines. Their native event loops use the
shared Windows frame-wait adapter's one-shot high-resolution timer, with ordinary timer/timeout
fallback, message and process-event wakeups, and no recurring frame timer while idle. Deadlines
preserve fractional periods and skip missed frames; animation progress uses elapsed time.
The wallpaper service is not a participant: its presentation surface re-pins itself to
`HWND_BOTTOM` and is
deliberately not hidden in game mode.

Band membership has exactly one author. The Win32 window adapter no longer keys
topmost off `reach_surface_role`, caches no topmost bit, and never changes z in
`show()` or `raise()` — a second table there would have to be kept in sync with the
layer registry by hand, and a cached bit goes stale the moment `place_behind` puts a
window in the band. Windows are created non-topmost and hidden; the first apply pass
shows and chains them in the same frame. The one visibility owner rule holds across
the lifecycle too: `reach_host_stop` hides through `reach_host_hide_all_surfaces` so
the applied plan stays truthful, rather than hiding windows behind the pass's back.

Surfaces that cast a shadow are drawn into a window larger than themselves, and the
rule that keeps that from leaking is absolute: **`reach_rect_f32 bounds` means content
bounds everywhere.** A definition declares which theme shadow a surface takes
(`reach_surface_shadow`); `reach_host_surface_shadow_pad` is the single producer of the
resulting margin, and it has exactly three consumers — the rect handed to `set_bounds`,
the command buffer's `content_rect`, and the input-region offset. `last_bounds` keeps the
content rect, so hit testing, popup placement, trespass, dock-local conversion and the top
bar's window-push band all keep working untouched. A fourth consumer would be the failure
this shape exists to prevent.

Native-only bar movement keeps the capsule's shown content geometry stable. Generic input
projection translates the actual published content origin back to that capsule geometry;
pointer dispatch and popup-owner dismissal guards share the same projection. Retained root
translation/scale is inverted there as well. Shadow margins remain outside this content contract.

The margin is not empty space the renderer may ignore. The WUC host-backdrop visual is
sized to the whole window, so it must be inset-clipped to `content_rect` or the acrylic
fills the margin with a hard-edged rectangle. Bars need the same care at the other end:
they never hide their window, they slide to `reach_bar_hidden_position`, so that position
takes the shadow's extent as clearance — without it a retracted bar leaves its shadow
smeared along the screen edge. Both paths that derive the hidden position must pass the
same clearance or reveal progress drifts between them.

Shadow rasterisation is the adapter's, and it is cached per backend on the shape rather
than the size: the ring around a rounded rect is invariant along its straight runs, so it
is baked once and drawn as slices, stretched along whichever axes can stretch. A notched
shape cannot stretch along the notch's axis and a shape too small to hold two corner caps
cannot stretch at all — both are facts about geometry, never about whether something is
animating, which is why an animating surface is a cache hit rather than a special case.
Entries whose key is size-independent are permanent; the few that must carry a size are
bounded, because a surface that animates a dimension too small to slice would otherwise
mint one entry per frame.

Every open path calls one entry point —
`reach_host_surface_opening(host, id, origin)` — and never hand-picks close rules. It
derives the sweep from declarative properties: opening anything closes every open
`BEHAVIOR_EXCLUSIVE` surface, and opening anything exclusive, transient, or popup
closes every open transient and popup. The matrix is therefore total. `origin` is the
surface that spawned this one and is never closed, so a menu opened from a panel does
not dismiss the panel underneath it; pass `REACH_SURFACE_ORIGIN_NONE` when nothing
spawned it. Launcher, switcher, and stage are the exclusive surfaces, so at most one
is ever open, and a fourth needs only the flag. Popups and transients are sibling
classes, not subtypes — every rule but popup-mutual-exclusion treats them together,
so a rule keyed on one silently misses the other. The global outside-press rule is likewise one loop
over the transient and popup rows, closing any surface the press missed. Foreground identity
comes from the in-process foreground watcher port, including Reach-owned and untracked windows.
`window_tracking` keeps current native foreground separate from the remembered tracked app used
for focus history. Dock's minimize toggle checks current foreground through the window-manager
port, including owned dialogs, rather than treating the remembered app as currently focused.
Nothing reads focus state out of the Reach Service snapshot.
Genuinely per-feature policies stay as named exceptions (e.g. dock-cluster pairwise
button rules); a growing exception list signals a missing class rule. May include
everything.

### Adding a feature

Everything a new interactive surface needs is authored in its own directory plus one
registry definition and runtime binding; no other feature's code changes. A feature-specific
composition branch outside the two controlled seams requires explicit approval and a scoped
entry in `docs/repo-analysis.md`.

1. **Capsule** (`src/features/<name>/`, header in `include/reach/features/`):
   implement `reach_feature_capsule_ops` (null-skip the hooks you don't need;
   `handle_pointer` gets the complete pointer stream in its declared coordinate
   space; popup capsules use surface-local coordinates), keep state compiler-private
   (`const` `state_ptr`, internal
   `state_mut`, semantic ops for writes), and report outcomes as
   `reach_feature_action_kind` values — never name a peer feature.
2. **Services**: attach any you consume at wiring
   (`reach_<name>_attach_...`, lifecycle attach/detach pair). Apply an external effect
   through the attached service rather than reporting an action for composition to
   translate; composition keeps only the effects it owns, such as surface open/close,
   app-launch scheduling, window control, and pin mutation.
3. **Registry definition** (`src/composition/feature_registry.cpp`): id, opaque factory,
   class, surface runtime binding, opaque lifecycle attachment/start/stop operations,
   host-level `force_close`, optional opaque control operations for semantic open/close,
   notifications, fast-frame blockers, and render-resource ownership, capsule ops,
   pointer flags, `role`, `pointer_priority`, `dismiss` if outside-press close differs
   from `force_close`, `layout.priority`, uniform `surface_ops`, and declarative
   `toggle_events`/`routed_events` for activation. Immutable policy belongs only in the
   definition; mutable instance state belongs only in `reach_feature_runtime`.
   If the feature must affect another feature, add one neutral outbound slot to its own
   header and bind it in `interfeature_routes.cpp` — the only composition file allowed to
   know which feature an action reaches.
4. **Surface operations**: provide uniform arrange, geometry, and render-command
   operations consumed by `reach_host_frame_registered_surface`. Do not add another
   named `reach_host_frame_<feature>` function.
5. **Tests**: no UI tests. Add logic tests only where ownership, reconciliation, asynchronous
   publication, or another fragile rule is likely to regress; do not mirror straightforward
   presentation wiring in tests.
6. Run build + ctest + `tools/check_architecture.py`, then the live-run
   protocol with a visual pass.

If the feature needs cross-feature policy the class rules don't cover, add a
named, commented exception in composition — and treat a growing exception
list as a missing class rule.

### Wi-Fi and Bluetooth (Settings app)

The Settings app owns two radio pages, each backed by a service over a port and a Windows adapter.
Both follow the "ongoing OS state belongs behind a service" rule rather than the app's older
worker-per-page pattern, so `settings_app.cpp` gained no fourth and fifth bespoke worker.

`reach_wifi_service` and `reach_bluetooth_service` (`src/services/`) each own one worker thread, a
serialized command queue, a published snapshot with a generation, and the drain of their port's
off-thread change callback. Commands are typed (`refresh`, `scan`, `connect`, `disconnect`,
`forget`, `set_radio` / `refresh`, `set_scan`, `pair`, `respond_pairing`, `unpair`); a refresh
raised by an OS change never displaces a queued user action. The app consumes them with
`take` on the UI tick and folds their `*_pending` into `needs_frame`. Port ops may block, so the
port carries an optional `thread_attach`/`thread_detach` pair that the adapter uses for its
per-thread WinRT apartment — that keeps `winrt` out of `reach_services`.

`wifi_wlanapi.cpp` drives WLAN API: `WlanRegisterNotification` for scan/connection/profile
changes, `WlanGetAvailableNetworkList` unioned with `WlanGetProfileList` so saved-but-out-of-range
networks still appear, and a profile XML built by the pure `wifi_profile.cpp` helper. Radio power
goes through WinRT `Radio` with `RadioKind::WiFi`, matching the Bluetooth precedent already in
`system_controls_win32.cpp`. `bluetooth_winrt.cpp` runs two `DeviceWatcher`s — classic and LE
association endpoints — and drives `DeviceInformationCustomPairing`, holding the deferral for a
`ConfirmPinMatch` request until the page answers. Device icons come from the `System.Devices.Icon`
property, environment-expanded, and resolve through the existing `icon_provider` resource-ref path.
The standalone Settings app consumes those references through `reach_icon_service`, so shell icon
resolution stays off its UI thread and completed icons replace fallback glyphs incrementally.

List policy is pure and lives in core: `reach_wifi_network_list_normalize` merges one SSID
advertised by several access points, keeps the strongest signal and the union of the
connected/saved/in-range facts, and orders connected → in range → saved → signal.
`reach_bluetooth_device_list_normalize` does the same across the two watchers, dropping unnamed
endpoints. Bluetooth radio power reuses `system_controls`; there is no second path. The app reaches
it through its own `reach_system_status` instance rather than calling the port from the UI thread,
so the radio row is fed by scoped refreshes and published snapshots exactly as Quick Settings is —
`reach_system_controls_get_bluetooth_state` is a blocking cross-thread round trip and belongs on a
service worker. The device list says nothing about the radio, so a device snapshot does not
re-read it. The retained `system_controls_port` exists only to create the service, start the change
watcher, and destroy the adapter.

Both pages render as an accordion: one row expands in place, driven by one animation track per
row, and every action lives inside the expanded row, so neither page opens a popup. The Wi-Fi
"Known networks" view is a sub-view of the same page, not an eighth nav row.

### Installed applications (Settings and Launcher)

Launchable applications registered with Windows come from one Windows adapter over the
`shell:AppsFolder` namespace. The adapter preserves each entry's AppUserModelID and maps packaged
entries to the current user's PackageManager records, including package family, full package name,
publisher, and version. Reach never reads or executes files from the protected `WindowsApps`
directory; packaged launches use `IApplicationActivationManager`, with the AppsFolder parsing path
retained as a shell fallback for desktop entries that publish an AppUserModelID but are not MSIX.

The Launcher search catalog consumes the complete adapter projection and carries the
AppUserModelID through search results and feature targets. The standalone Settings app consumes it
through `reach_installed_apps_service`, whose worker retains only packaged entries for which the
Shell publishes an uninstall verb, thereby excluding desktop launchers and non-removable Windows
components while preserving user-manageable packages. The worker serializes refresh, open,
uninstall, and management commands and publishes snapshots to the UI tick. The Applications page
commits its page state immediately on the activating release while preserving the selector's
animated travel, shows the shared indeterminate loader while refresh is pending, renders the
returned rows without waiting for icons, and resolves those icons through `reach_icon_service`. It
currently exposes only uninstall; its Open and Options backend paths remain wired as WIP for later
restoration. Current-user packaged uninstall uses the OS PackageManager API. Repair and reset are
not reimplemented because Settings-equivalent direct repair belongs to the Windows App SDK
PackageDeploymentManager and Reach does not deploy that runtime.

## tools

The executables — reach shell, Reach Service, watchdog, reachctl, update helper,
elevation helper, probes. Each includes what it needs.

### Watchdog and crash-loop policy

`reachWatchdog.exe` starts `reach.exe --launch` and waits on it. Exit code 0 is intentional and is
never restarted (an exit-0 loser of the single-instance mutex re-attaches to the surviving Reach).
Any other exit, including a failed start, is an unexpected exit and goes through one policy in
`include/reach/support/crash_loop.h` regardless of uptime: the first is restarted immediately, the
second after one second, and a third within a three-minute window triggers the permanent fallback.
The fallback launches a detached notice child, sets the HKCU Winlogon shell to `explorer.exe`,
terminates Reach and starts Explorer. The notice child waits for Explorer's taskbar, shows a
tray balloon that Reach failed and was replaced by Windows Explorer, and records
`diagnostics/notice.pending`. The fallback is permanent: a crash loop means the system is not
compatible with the installed build. `reach.exe --launch` run directly while Reach is not
installed as the shell (`reach_windows_reach_is_installed`) shows a dialog asking the user to
install Reach first and exits 0, which is not a crash.

### Diagnostics

Every Reach process writes one-line UTF-8 events to `%LOCALAPPDATA%/Reach/diagnostics/events.log`
through the sink installed by `reach_diagnostics_log_install`; `reach_log_*` routes to it. Appends
are serialized by a named mutex, the log rotates at 512 KB and keeps two generations
(`reach_log_rotation_steps`), and Reach, Settings and the watchdog all write to the same file.
Device loss, recovery, adapter switches, watchdog exits and fallback decisions are logged with the
fields needed to correlate them.

`reach_crash_dump_install` runs in `reach.exe` and `reachSetting.exe`. It sets the process-wide
error mode once at startup to `SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX`, so no thread shows
a critical-error or error-reporting box; `ShellExecuteEx` launches do not inherit it. It also
suppresses the Windows error-reporting dialog with `WerSetFlags(WER_FAULT_REPORTING_NO_UI)`, sets
`_set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT)` so `abort` shows no message box, and
in Debug builds routes CRT assert and error reports to the debugger. It installs the
unhandled-exception filter, the CRT invalid-parameter and pure-call handlers, `std::set_terminate`
and a `SIGABRT` handler.
Each handler signals a dump thread created at startup, so a stack overflow is covered because the
dump is written on the thread's own stack. That thread calls `MiniDumpWriteDump`, logs a `crash`
line and ends the process with the exception code, which the watchdog treats as an unexpected exit.
Dumps are `diagnostics/dumps/<process>-YYYYMMDD-HHMMSS-<pid>.dmp`. The watchdog prunes them when it
starts (`reach_crash_dump_prune_select`): the newest five, within 50 MB in total. Fail-fast
terminations (`0xC0000409` raised directly) and heap corruption bypass these handlers and leave only
the watchdog's exit line. No registry state is involved.

The Update page of the Settings app shows a quiet Diagnostics row: the event count and first date,
the last device recovery, and an action that opens the diagnostics folder. The log is read once when
the page is entered (`reach_diagnostics_read_log`, bounded to the log's size limit and summarized by
`reach_diagnostics_summarize`). If `notice.pending` exists, the page also shows that Reach fell back
to Windows Explorer and deletes the file after the first frame that displayed it.

## apps

Self-contained leaf executables outside the shell process — e.g. the standalone
Settings app (`reachSetting.exe`). May include `features` … `core`. Nothing depends on
an app — apps are leaves, like `tools`.
