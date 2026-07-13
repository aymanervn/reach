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
OS controls, filesystem, clipboard, media, icons, the Reach Service client. The media
port separates fast core-state reads from generation-checked cover reads so image I/O
cannot block transport state. Interfaces only. Includes `core`, `protocol`.

## adapters

The Windows implementations of `ports`; the **only** layer that touches the OS.
Includes `ports`, `protocol`, `core`, and platform SDKs.

## services

Shared in-process capabilities with state/cache/policy — config, icons, search,
system status, Now Playing, … Includes `ports`, `protocol`, `core`. Now Playing
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
`on_game_mode`, `needs_frame`, `wants_pointer_move`, `handle_pointer`);
composition orchestrates through these, so adding a feature costs no
feature-specific composition code. `handle_pointer` carries the complete
down/up/move/wheel/leave/cancel/context/middle stream so cleanup semantics do
not fall back to feature-specific host branches.
The Dock is fully migrated to this contract: it owns press/release, hosted-button
feedback, drag/reorder, middle/context actions, cancellation, and its private Now
Playing input. Composition translates only its semantic actions and cross-feature
popup policy. Raw hit types and row/index results are feature-private; the Dock
exposes only a semantic pointer-region query for the global popup mouse hook.
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
launcher→search precedent): refresh requests, snapshot take/apply, and the
bluetooth-pending grace timers run inside the capsule
(`reach_quick_settings_process_changes`); its pending service work folds into
`needs_frame`. The system-controls watcher fires on a port thread, so
composition keeps the atomic change-flag accumulator and passes the drained
flags in — capsule state is never written off-thread. GPU lifetime stays in
composition: audio applies retire the replaced session/device render icons
and the host drains and releases them.
Tray owns popup item hit resolution, press/release feedback, left/right activation
semantics, and cancellation. Composition retains provider activation, topmost
window handling, and popup lifecycle.
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

**Accepted coupling (by design — do not “fix”):** the dock cluster. The dock
hosts the tray / quick-settings / power buttons, so those popup features may
take the dock layout directly (e.g. `reach_tray_layout_popup(…, dock_layout,
…)`); no anchor indirection is wanted between them. Now Playing is not a
separate feature: its private UI subfeature lives inside dock and consumes the
shared Now Playing service, leaving room for a future standalone music feature
to consume the same stable service independently.

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
(SOURCE_GATED / DOWN_CLOSES_ON_UNHANDLED / DOWN_APPLIES_UNHANDLED). Dock-cluster
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
Genuinely per-feature policies stay as named exceptions (e.g. the launcher closes
on a foreground-window change); a growing exception list signals a missing class
rule. May include everything.

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
probes. Each includes what it needs.

## apps

Self-contained leaf executables outside the shell process — e.g. the standalone
Settings app (`reachSetting.exe`). May include `features` … `core`. Nothing depends on
an app — apps are leaves, like `tools`.
