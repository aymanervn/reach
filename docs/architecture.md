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
the standard library.

## protocol

Cross-process contracts — Reach Service messages, shared-memory layout, kernel object
names, version constants. Includes `core`.

## ports

Abstract interfaces for every external boundary — renderer, surface, input, monitor,
OS controls, filesystem, clipboard, media, icons, the Reach Service client. The
`window_thumbnail` port abstracts live window previews (DWM thumbnails on Windows);
`screen_hotspot` abstracts a screen-edge/corner trigger region — the dock's reveal
edge and the stage's hot corner are two instances of the one port, and its Win32
adapter registers its window class idempotently so further instances cost nothing.
The media
port separates fast core-state reads from generation-checked cover reads so image I/O
cannot block transport state. Interfaces only. Includes `core`, `protocol`.

## adapters

The Windows implementations of `ports`; the **only** layer that touches the OS.
Includes `ports`, `protocol`, `core`, and platform SDKs.

## services

Shared in-process capabilities with state/cache/policy — config, icons, search,
system status, Now Playing, … Includes `ports`, `protocol`, `core`. Window
tracking owns the one naming policy for a running app
(`reach_window_tracking_app_display_name`: executable stem, window title as
fallback); every surface that labels an app — the top bar's current-app pill, the
switcher — reads it from there. A pinned app's stored title is a config record,
not a live label, so nothing renders it as one. Now Playing
publishes atomic core media generations immediately, enriches them with the latest
generation's cover asynchronously, owns transport serialization and cover lifetime,
and masks every transport control while a command is settling. A new core generation
temporarily retains the previous cover; a missing or failed current cover replaces it
with the UI placeholder. A media-to-no-media transition retains the last snapshot for
four seconds, then refetches and publishes disappearance only if absence is confirmed.
Cover acquisition waits for a 300-millisecond quiet period on the latest media
generation, coalescing provider thumbnail bursts without delaying core state.

## features

Self-contained UI capsules — dock, launcher, switcher, tray, quick settings, clipboard,
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
The Dock also owns its geometry (`reach_dock_local_point` /
`reach_dock_rect_to_screen` / `reach_dock_layout_to_screen`), converts the
screen-space pointer stream to dock-local coordinates itself, performs the
animated item rebuild (snapshot/build/rebind) as one op, and assembles the
context-menu command list for its items from its pin state and window service;
the command vocabulary lives in `reach/core/menu_commands.h` so the dock and
the context_menu display capsule share it without a feature→feature edge.
The Launcher is also fully migrated: it owns result and pinned-app presses,
scrolling and scrollbar capture, cancellation, and context-hit semantics through
`handle_pointer`. Composition translates launch/open/reveal actions and retains
focus restoration plus transient-surface policy.
The Clipboard is fully migrated as well: it owns item, close, clear, hover,
scroll, scrollbar-capture, leave, and cancellation behavior. Composition handles
only restore/provider calls, external resource release, and transient-surface
policy for the semantic actions it reports.
Quick Settings owns tile, slider, output-device, expansion, drag/capture, release,
and cancellation behavior through the same hook. Composition translates its
semantic actions into audio and system-control calls and retains popup policy.
Quick Settings also attaches the system-status service directly (the
launcher→search precedent): snapshot take/apply and the bluetooth-pending grace
timers run inside the capsule (`reach_quick_settings_process_changes`); its
pending service work folds into `needs_frame`. The system-controls watcher fires
on a port thread, so composition keeps the atomic change-flag accumulator and —
because the top bar's network readout needs fresh state whether or not the panel
is open — composition, not the capsule, turns the drained flags into the
`reach_system_status_refresh_system` request. Capsule state is never written
off-thread. GPU lifetime stays in
composition: audio applies retire the replaced session/device render icons
and the host drains and releases them.
The system-status snapshot therefore has two readers with different needs, so
the service offers both: `take_system` consumes a published generation (Quick
Settings applies each one exactly once) and `read_system` copies the latest
without consuming it (the top bar polls it from `tick` and diffs the values it
renders). A second consumer must never be added on `take_system`; it would steal
generations from the first.
Tray owns popup item hit resolution, press/release feedback, left/right activation
semantics, and cancellation. Composition retains provider activation, topmost
window handling, and popup lifecycle.
Stage is the window overview: a fullscreen overlay capsule that shrinks every open
window into a centered grid. It owns tile layout, the open/close animation, hover
state, and hit resolution, and reports only activate/dismiss actions. It never calls
the thumbnail port — it publishes a read-only placement list
(`reach_stage_thumbnail_count` / `reach_stage_thumbnail_at`) that composition drives
into `window_thumbnail` each frame, the dock-layout precedent. Its tiles live in
screen space; the render pass converts to surface-local. Because DWM composites
thumbnails *on top of* the host surface, stage chrome (labels, selection) must stay
outside the tile rects — drawing over a tile is not possible from the same surface.
Minimized windows have no DWM content and fall back to an icon tile.
Activating a tile suppresses every other tile's thumbnail for the close animation, so
the chosen window animates alone instead of being covered by a maximized neighbour.

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
bar hosts the tray / quick-settings / power buttons, so those popup features may
take the top bar layout directly (e.g. `reach_tray_layout_popup(…, top_bar_layout,
…)`); no anchor indirection is wanted between them. Now Playing is not a
separate feature: its private UI subfeature lives inside the top bar and consumes
the shared Now Playing service, leaving room for a future standalone music feature
to consume the same stable service independently. It renders one bold line and
scrolls it with the shared `features/common/marquee` clock when the text
overruns its slot; the scroll is gated on the bar being shown, so a hidden or
game-mode bar never asks for a frame.

The window push is the top bar's second private subfeature: while the bar can
hide and is sliding in, it moves the windows the bar would cover down by the
bar's own animated overlap, so they track the bar edge on one clock. It stores
each window's original outer rect at capture and animates back onto it — Windows
never grows a window back on its own — and drives the motion through
`app_control`'s generic window geometry ops (`reach_app_control_window_bounds` /
`reach_app_control_set_window_bounds`, one `BeginDeferWindowPos` batch per frame
in the adapter). Those ops work in the outer window rect the OS repositions,
never the DWM `frame_bounds` the rest of the shell measures with; mixing the two
drifts by the invisible resize border. The work area is deliberately left alone:
changing it costs ~37 ms per call, which no per-frame path can afford.

Every popup gets its bounds and its notch from one place —
`reach_popup_place(anchor, width, height, margin)` in `features/popup`. It
centres the popup on the anchoring control, clamps it into the monitor on both
axes, and returns the notch anchor in screen space, so the notch keeps pointing
at the button that opened it after the clamp moves the popup. Quick settings,
the tray popup and the context menu all place through it; a popup that clamps
its own bounds or re-derives its notch at render time has reintroduced the bug
this helper exists to prevent.

Dock and top bar are both bars: each owns a `reach_bar_visibility_state` driven
by the shared `features/common/bar_visibility` state machine, and composition
reconciles both through one `reach_host_reconcile_bar_visibility` over the
descriptor's `update_visibility` / `bar_edge` / reveal-edge fields.

Two separate inputs decide how another open surface affects the bars, and both
apply to both bars identically. A surface that declares `bar_shown_while_open`
*forces* them shown for as long as it is open; stage is the only one, because it
is a window overview and wants the bars in frame. Any open popup instead only
*holds* them: a bar already shown stays shown, but opening a popup never summons
a hidden bar, and the hold lifts the frame the last popup closes. Everything else
leaves the bars to hide on their own rules — the launcher and the clipboard are
pure transients that own the screen while they are up, and the switcher is the
sole `OVERLAY`. The switcher exclusion is deliberate and differs from
`reach_host_window_list_blocked`, which does count `OVERLAY`: the dock hover menu
is suppressed during alt-tab while the bars still hide.

## composition

The host (`reach_host`): wires adapters into ports, constructs services and features,
and runs the app — frame loop, input routing, action→port translators, worker threads,
and surface lifecycle. Surfaces register a descriptor with a class
(persistent | transient | popup | overlay) plus the feature capsule and its uniform
hooks; policy runs as class loops over that table — tick, needs-frame, game mode,
lifecycle resets, pointer-move subscription sync, the popup mouse hook, transient
dismissal, and the “opening a popup closes the other popups” rule. Pointer input
uses one descriptor-driven dispatcher for capsule delivery, surface dirtying,
relayout, capture, subscription sync, and update scheduling; capsules receive
screen-space coordinates and convert locally themselves. Each pointer event kind
runs as a generic loop over the table in `pointer_priority` order (popups →
transients → persistent, first handled result wins), with the descriptor's
`role` resolving source-gated delivery, its `apply_pointer_action` translating
handled results, and its flags declaring the outside-press policy
(SOURCE_GATED / DOWN_CLOSES_ON_UNHANDLED / DOWN_APPLIES_UNHANDLED). Source-gated rows are delivered to first — press and release go to the row whose
`role` matches the event source (or whose `pointer_sequence_active` hook reports
an in-flight sequence) before the rest of the table sees them — so no surface
needs a hand-written branch to receive its own input. Top-bar-cluster
pairwise policy (QS-button pass-through, power-press dismissal, tray/launcher
close rules) and true capture pre-emption (dock drag, QS slider, launcher
scrollbar) stay as named, commented exceptions ahead of the loops. Hotkey and
action→port translators for media transport, volume, and brightness live in
`host_system_actions.cpp`, out of the input routing path.
Per-frame layout resolves in dependency order in `reach_host_update` (monitor →
dock cluster → launcher → clipboard → switcher); the per-surface frame steps
(`host_surface_frames.cpp`, layout refresh → transition → window state →
corners → show/render) run as one loop over the table in `frame_priority`
order against a shared `reach_host_frame_context`.
Surfaces that take OS activation declare `BEHAVIOR_ACTIVATES`; the class rules then
own show-on-activate, close-on-focus-loss, and the staleness check that discards a
focus-loss signal the surface has already recovered from (reach's own foreground
handover emits one). An activating surface must show through
`reach_host_apply_surface_activation` (idempotent via `surface->activated`), never
through the per-frame `show()` in `reach_host_apply_transient_frame` — a fullscreen
surface re-asserting `HWND_TOPMOST` every frame makes the desktop unusable.
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

Everything a new interactive surface needs is authored in its own directory
plus one descriptor row; no other feature's code changes.

1. **Capsule** (`src/features/<name>/`, header in `include/reach/features/`):
   implement `reach_feature_capsule_ops` (null-skip the hooks you don't need;
   `handle_pointer` gets the complete screen-space stream and converts
   locally), keep state compiler-private (`const` `state_ptr`, internal
   `state_mut`, semantic ops for writes), and return semantic actions —
   never call ports.
2. **Services**: attach any you consume at wiring
   (`reach_<name>_attach_...`, lifecycle attach/detach pair) — read +
   request only; mutations stay composition's.
3. **Descriptor row** (`reach_host_init_surface_descriptors`): id, class,
   surface runtime, transition, host-level `force_close`, capsule + ops,
   pointer flags, `role`, `pointer_priority`, `apply_pointer_action`
   (your action→port translator), `dismiss` if outside-press close differs
   from `force_close`, `frame` + `frame_priority`, and declarative
   `toggle_events`/`routed_events` for activation.
4. **Frame step** (`host_surface_frames.cpp`): one function over
   `reach_host_apply_transient_frame` for the common case.
5. **Tests**: logic-only, against the capsule ops — no UI or service tests.
6. Run build + ctest + `tools/check_architecture.py`, then the live-run
   protocol with a visual pass.

If the feature needs cross-feature policy the class rules don't cover, add a
named, commented exception in composition — and treat a growing exception
list as a missing class rule.

## tools

The executables — reach shell, Reach Service, watchdog, reachctl, update helper,
elevation helper, probes. Each includes what it needs.

## apps

Self-contained leaf executables outside the shell process — e.g. the standalone
Settings app (`reachSetting.exe`). May include `features` … `core`. Nothing depends on
an app — apps are leaves, like `tools`.
