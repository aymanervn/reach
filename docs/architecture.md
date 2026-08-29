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
Includes `ports`, `protocol`, `core`, and platform SDKs.

## services

Shared in-process capabilities with state/cache/policy — config, icons, search,
system status, Now Playing, … Includes `ports`, `protocol`, `core`. The config service owns
the live configuration snapshot, publishes typed mutations before persistence, rebases pending
mutations on the latest stored snapshot, coalesces background writes by generation, and
reconciles external-change reloads. Features consume snapshot copies and never access the
config-store port directly. Window tracking owns the one naming policy for a running app
(`reach_window_tracking_app_display_name`: executable stem, window title as
fallback); every surface that labels an app — the top bar's current-app pill, the
switcher — reads it from there. Pinned apps do not persist a title; the Dock derives
its icon fallback from the executable path and never treats config as a live label. Now Playing
publishes atomic core media generations immediately, enriches them with the latest
generation's cover asynchronously, owns transport serialization and cover lifetime,
and masks every transport control while a command is settling. A new core generation
temporarily retains the previous cover; a missing or failed current cover replaces it
with the UI placeholder. A media-to-no-media transition retains the last snapshot for
four seconds, then refetches and publishes disappearance only if absence is confirmed.
Cover acquisition waits for a 300-millisecond quiet period on the latest media
generation, coalescing provider thumbnail bursts without delaying core state.

## features

Self-contained UI capsules — dock, launcher, switcher, quick settings, clipboard,
settings, context menu, wallpaper. Each owns its state, layout, animation,
hit-testing, render composition, and interaction behind
create/update/handle_event/append_render_commands entry points, and returns semantic
actions instead of calling ports. Includes `services`, `ports`, `protocol`, `core` —
never another feature's internals.

Every capsule also implements the uniform hooks in
`reach/features/feature_capsule.h` (`reset`, `tick`, `is_open`, `force_close`,
`on_game_mode`, `needs_frame`, `wants_pointer_move`, `handle_pointer`,
`pointer_sequence_active`);
composition orchestrates through these, so adding a feature costs no
feature-specific composition code. `handle_pointer` carries the complete
down/up/move/wheel/leave/cancel/context/middle stream so cleanup semantics do
not fall back to feature-specific host branches.
The Dock is fully migrated to this contract: it owns press/release, item
feedback, drag/reorder, middle/context actions, and cancellation. The Top Bar
owns the same for its hosted buttons — power, tray icons, tray overflow, quick
settings, language — plus its private Now Playing input. Composition translates
only their semantic actions and cross-feature popup policy. Raw hit types and
row/index results are feature-private; both bars expose only a semantic
pointer-region query for the global popup mouse hook.
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
Quick Settings also attaches the system-status service directly (the
launcher→search precedent): snapshot take/apply runs inside the capsule
(`reach_quick_settings_process_changes`); its pending service work folds into
`needs_frame`. A bluetooth toggle is resolved by the port, not by a timer: the
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
Stage is the window overview: a fullscreen overlay capsule that shrinks every open
window into a centered grid. It owns tile layout, the open/close animation, hover
state, and hit resolution, and reports only activate/dismiss actions. It never calls
the thumbnail port — it publishes a read-only placement list
(`reach_stage_thumbnail_count` / `reach_stage_thumbnail_at`) that composition drives
into `window_thumbnail` each frame, the dock-layout precedent. Its tiles live in
screen space; the render pass converts to surface-local. Because DWM composites
thumbnails _on top of_ the host surface, stage chrome (labels, selection) must stay
outside the tile rects — drawing over a tile is not possible from the same surface.
Minimized windows have no DWM content and fall back to an icon tile.
The Desktop tile uses the Reach-owned top-level Progman compatibility host as its
DWM source and crops the virtual-screen thumbnail to the primary monitor.
Activating a tile suppresses every other tile's thumbnail for the close animation, so
the chosen window animates alone instead of being covered by a maximized neighbour.
A tile's `source_rect` is the screen rect the close animation lands on, and windows
move while the overview is up — opening Stage forces the top bar shown, and the bar
pushes every trespassing window down with its reveal progress. Composition therefore
re-seats the landing rects from live `frame_bounds` immediately before
`reach_stage_begin_close` (`reach_stage_refresh_tile_frames`), so a tile returns to
where its window is now rather than where it was when the overview opened. That
refresh deliberately leaves `target_rect` alone: the grid box is an aspect fit of
`source_rect`, so re-fitting it would resize tiles the instant a close begins.

Context Menu owns row hit resolution, hover state, command selection, dismissal,
and cancellation through `handle_pointer`. Composition executes the reported
command and retains OS calls plus cross-popup and Dock power-button policy.
Interaction hit contracts for every migrated feature remain private to that
feature; public capsule APIs expose semantic actions, queries, and render inputs.
Capsule state is compiler-enforced private: the public `reach_<f>_state_ptr()`
accessors return `const`, mutation goes through semantic ops, and the internal
`reach_<f>_state_mut()` accessors must never appear outside `src/features/`
(checked by `tools/check_architecture.py`), with no exceptions: the launcher
owns its text input end-to-end (`reach_launcher_handle_text_event` drives the
edit model, query, and attached search; composition only routes the raw
TEXT_CHAR/TEXT_EDIT events and applies the reported redraw/relayout).

**Accepted coupling (by design — do not “fix”):** the top bar cluster. The top
bar hosts the tray / quick-settings / power buttons, so its private tray overflow UI and the
other popup features may take the top bar layout directly; no anchor indirection is wanted
between them. Now Playing is not a
separate feature: its private UI subfeature lives inside the top bar and consumes
the shared Now Playing service, leaving room for a future standalone music feature
to consume the same stable service independently. It renders one bold line and
scrolls it with the shared `features/common/marquee` clock when the text
overruns its slot; the scroll is gated on the bar being shown, so a hidden or
game-mode bar never asks for a frame.

Text-dependent layout consumes `reach_text_measure_port`. The DirectWrite renderer supplies the
implementation with the same font family, size, and weight used for drawing; features retain only
a conservative estimate for unavailable-adapter cases. This keeps exact text metrics outside
feature policy while allowing the top bar and Dock window-list popup to size before rendering.
Applying a different UI font invalidates both rendering and layout so the next normal frame
remeasures every text-sized surface. The Dock window-list popup fits its widest measured title
between one-letter-plus-close-control chrome and a monitor-bounded maximum, ellipsizing beyond it.
All Reach-rendered text selects from the shared scale in `reach/core/typography.h`

The window push is the top bar's second private subfeature: while the bar can
hide and is sliding in, it moves the windows the bar would cover down by the
bar's own reveal progress, so they track the bar edge on one clock. That
progress is `reach_bar_visibility_result.reveal_progress` — the fraction of the
bar's full travel between its hidden and shown positions, produced where the
hidden position is known. Deriving it instead from how far the bar's bottom edge
has crossed the screen edge leaves the windows parked for the first ~80% of the
reveal and then racing to catch up. Both the full frame path and the
`reach_host_move_bar_animation_frame` fast path feed it: that fast path slides a
bar without a redraw and so never reaches `reconcile_bar_visibility`, which is
exactly the state a hide settles into. The definition's optional
`reach_bar_reveal_ops.position_frame` hook keeps feature-owned effects such as
the top-bar push on that same clock while the generic path moves the surface.

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
height, and the same gap again below it — not from the bar rect alone, so a
revealed bar floats between the screen edge and the window it pushed. Only
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
The work area is deliberately left alone: changing it costs ~37 ms per call,
which no per-frame path can afford.

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
screen edge through the bar's content bounds and its rendered shadow extent. The
shadow clearance comes from the theme's resolved per-monitor DPI geometry rather
than an independent policy constant. `window_tracking` supplies the one outer-bounds
trespass query used by both bars. The resulting hide policy is the
same even though reveal presentation differs: the top bar rests in the app band
and uses `reach_bar_reveal_ops.position_frame` to push trespassing windows, while
the permanently-topmost Dock reveals over them without a side effect. A single
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
reports open: `reach_host_surface_presented` is `is_open || needs_frame`, the same
predicate the frame pass uses to decide a surface is still on screen, so the force
survives the whole close animation and lifts on the frame the surface actually
leaves. Stage is the only such surface, because it is a window overview and wants
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
slots that let one feature's action affect another. `feature_registry.cpp` is the seam for
feature definitions: each registered surface has exactly one immutable
`reach_feature_definition` and one `reach_feature_runtime`. The definition owns its opaque
create/destroy factory, capsule operations, surface operations, layout, and policy. The runtime
contains only the bound surface, transition, capsule, resolved geometry, native-overlay state,
and a pointer to that definition. A registered surface joins every loop by being in that table:
lifecycle in `reach_host_create_with_dependencies`, layout in `reach_host_init_layout`,
transition completion in `reach_host_finish_surface_transitions`, input ordering in
`reach_host_pointer_order`, and the frame pass in `reach_host_update` each iterate it whole, and
`tools/check_architecture.py` rejects any of them being narrowed to a hand-maintained feature
list. `reach_host_feature_capsule<T>` derives any concrete view from that
capsule; `reach_host` carries no parallel typed capsule pointers. Host construction and cleanup iterate the factories
generically; a shared surface such as the tray declares no factory and binds to its owning
top-bar capsule. Policy runs as class loops over the runtime table — tick, needs-frame, game
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
`host_system_actions.cpp`, out of the input routing path.
The system HUD consumes the final top-bar visibility result cached by that same
bar reconciliation, so keyboard media, volume, and brightness actions never
reconstruct the hiding predicate. Successful level changes hand the capsule the
exact post-action state; media actions refresh their presentation snapshot from
the Now Playing service. The HUD is a persistent, source-gated surface at layer
220, above every other Reach layer. Its visual card is one blocking input region:
presses are consumed without actions or capture, while pointer enter pauses its
dismissal dwell and pointer leave releases it. It is centered above the Dock's
shown-position geometry even when the Dock itself is hidden, and its whole render
command buffer is faded by the shared animation manager.
Per-frame layout resolves in dependency order in `reach_host_update` (monitor →
dock cluster → launcher → clipboard → switcher); the per-surface frame steps
(`host_surface_frames.cpp`, layout refresh → transition → window state →
corners → show/render) run as one loop over the table in definition `layout.priority`
order against a shared `reach_host_frame_context`. Transition completion is runtime-driven:
every non-null runtime transition is finalized by the generic animation tick. Every definition
exposes uniform `surface_ops` for
arrangement and render-command production. `reach_host_frame_registered_surface`
then resolves the declared layout anchor, applies window geometry and visibility,
and executes rendering without naming the feature. System HUD uses this path and
declares Dock as its anchor; the Dock's shown-position bounds are stored on its
feature runtime rather than in a HUD-specific host cache. Switcher also uses the
path: its capsule owns width animation, arranged bounds, and geometry publication,
while its registry adapter supplies the transition-adjusted render bounds. Clipboard
declares Launcher as its anchor and likewise owns relayout, animation state, geometry,
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
participant registers a base layer and, optionally, per-condition layer and
visibility overrides; `reach_layout_resolve` is a pure function of registrations,
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
pointer interaction invalidated the topmost chain. A pointer-down on a visible banded
surface schedules an order-only reconciliation because Windows may reorder clicked
HWNDs inside the topmost band; visibility remains cached and is not replayed. Each
semantic plan change emits ops only for the participants whose layer or visibility
actually changed.

Conditions are bits, not triggers: the arrangement is recomputed from the whole
active set, so setting an already-set condition is a no-op and a missed one heals on
the next resolve. `GAME_MODE` resolves every participant hidden except a definition
that declares `BEHAVIOR_GAME_MODE_VISIBLE`. The system HUD is the only such
participant because hardware media and level keys remain active while the top bar
is suppressed. `host_game_mode.cpp` owns the state and the main gate in
`reach_host_update`; the game-mode path runs only behavior-flagged capsules and
their frames, leaving all ordinary composition work below the gate dormant.
Game-mode Alt-Tab is one Reach Service window transition: it transfers foreground
to the registered shell desktop, requires that handoff to succeed, minimizes the
game, and then clears the shared cursor clip once. The Reach-owned Progman host
permits that explicit activation while continuing to reject pointer activation.
The top bar is the only participant whose layer moves: it rests at 0 and rises to
130 while its reveal
transition is live, while a `bar_shown_while_open` surface is open, or while a popup
holds the bars. Starting that Y animation and reporting the transition are the same
act, performed by `reach_bar_update_visibility` alone — nothing else may write
`REACH_TOP_BAR_ANIM_Y`, and nothing may set the bar's layer except the resolve
reading `reach_bar_visibility_result.reveal_transition_active` into that
participant's layer intent. Definition-declared edge reveals are participants too,
attached to their owning surface runtime but independently visible; the underlying
screen-hotspot port carries `set_topmost` / `native_id` / `place_behind` so they
chain and seed like any other participant.
The wallpaper is not a participant: it re-pins itself to `HWND_BOTTOM` and is
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
has a single producer — the in-process foreground watcher port feeds
`window_tracking`, and nothing reads focus state out of the Reach Service snapshot.
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
   class, surface runtime binding, transition, host-level `force_close`, capsule ops,
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
5. **Tests**: logic-only, against the capsule ops — no UI or service tests.
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

## tools

The executables — reach shell, Reach Service, watchdog, reachctl, update helper,
elevation helper, probes. Each includes what it needs.

## apps

Self-contained leaf executables outside the shell process — e.g. the standalone
Settings app (`reachSetting.exe`). May include `features` … `core`. Nothing depends on
an app — apps are leaves, like `tools`.
