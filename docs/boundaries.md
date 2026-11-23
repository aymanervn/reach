# Boundaries

## Features

- Every feature is a capsule: it owns its state, internal layout, animation, hit testing, input sequences and render commands, behind the uniform hooks in `include/reach/features/feature_capsule.h`. If a feature cannot be a capsule, stop and get the user's permission.
- Features never include or name another feature, except shared `features/common` contracts.
- Interfeature behavior uses typed callbacks or slots that a feature exposes. Composition assigns them. Never create a port only for in-process routing.
- Alternative visual styles of one feature reuse its capsule, layout, controls and action routing. A style owns only its chrome, edge insets and app-side clearance (`reach_top_bar_apply_config`, `reach_top_bar_background_bounds`).

## Composition

- Composition is a generic runtime. Concrete feature knowledge lives only in two seams:
  - `feature_registry.cpp`: one `reach_feature_definition` per feature (factory, capsule ops, surface spec, layout spec), using public contracts only.
  - `interfeature_routes.cpp`: assigning callbacks and slots between features.
- Everything else in composition iterates definitions and runtimes generically: lifecycle, layout, presentation completion, input order and the frame pass. Never add a hand-maintained feature list, per-feature frame function or feature-named branch.
- A new feature may change only its header, `src/features/<feature>/`, one registry entry, one route if needed, tests, build and resources. Any other composition change is an exception: stop, name the missing generic contract and ask.
- The layout manager stays pure: it owns cross-surface placement, layer, visibility and dependency order, and emits resolved bounds. Extend its generic declarations instead of adding feature branches.
- Definitions do not receive `reach_host` to regain arbitrary composition access.
- Feature definitions may declare `open_events` and `close_events` beside `toggle_events`. Composition's shell-wide fallbacks run only for events no feature consumed (`reach_host_dispatch_registered_events`).
- Surfaces are identified only by `reach_surface_key_for<T>()`, written only in the two seams. Slots come from definition order and are runtime detail: never add a numeric surface ID, index runtimes by slot or seed a slot from a constant.
- A definition is one self-contained block. Generic code reads behavior from definition fields and callbacks (flags, events, layout, redraw, lifecycle hooks); a new behavior gets a generic field, not a branch on which feature is running.
- Routes only assign callbacks and slots. They do not reach into another feature's runtime, renderer or dirty state.
- Removing a feature is the reverse: delete its files, registry entry and route. Nothing generic, no host dependency, adapter or checker entry changes.

## Shared contracts

- Anything one feature's value can break in another is a declared contract, checked by the registry or `tools/check_architecture.py`, or listed in `pending-work.md` until a check exists. Never leave it a silent convention.
- Ordering values (surface layer, edge-reveal layer, pointer priority, layout priority) are named constants in one ordering table in `feature_registry.cpp`, so the full stack and hit order read top to bottom. Definitions never use literals. Registration rejects duplicate values in the same ordering.
- Each routed or toggle event has one owning definition. An event that several definitions handle (Escape) must be in an explicit shared set; registration rejects any other duplicate.
- Every callback across a seam states its calling thread at its declaration. Work finished off the UI thread returns through a posted event; UI-thread entry points `REACH_ASSERT` the thread.
- A new shared value or callback adds its declaration and check in the same commit.

## Ports, adapters, services

- A port expresses a generic Reach contract, not a thin copy of an OS API. OS code stays in adapters.
- Ports and host dependencies describe capabilities, never feature identities: window behavior is `reach_platform_window_flag` bits, native surfaces come from `create_surface_ports`. No per-feature role, field, factory or list.
- Ongoing OS-derived state lives in a service over its port; the service owns refresh, snapshots, caching, threading, generations and policy. A direct read is fine only when it is simple and stateless.
- `reach_config_service` is the only owner of live config in a process. Features read snapshot copies and submit typed mutations. Only composition and bootstrap code hold `reach_config_store_port`.
- A surface that owns work-area policy publishes management and reservation intent with its geometry and far-edge clearance. Composition derives the band without knowing feature styles; the monitor port applies it (`manage_monitor_work_area`, `reserve_monitor_work_area`, `set_work_area`). `reachctl` uses the same port for repair.

## Confirmed workarounds

- The Quick Settings Project tile injects Win+P (`reach_system_controls_open_project_menu`); Windows has no API for that flyout.
- DWM draws host backdrop brushes black until the shell publishes the session WNF state `WNF_IMSN_TRANSPARENCYPOLICY`, which only Explorer's immersive shell does; the compat host publishes it from the transparency, high-contrast and battery-saver state (`reach_explorer_desktop_compat_publish_transparency_policy`).
- Stage's live thumbnails come from the undocumented `dwmapi` ordinal 147 (`DwmpCreateSharedThumbnailVisual`) as DirectComposition visuals the compositor animates; its properties are fixed at creation. Without it Stage keeps classic DWM thumbnails moved per frame (`render_d2d_thumbnail.cpp`).
- Reach's supervision rests on the undocumented `SetShellWindow` and on Windows restarting the shell when its first registrant exits (measured 2026-11-23; `shell_session_win32.cpp`).
- Focusing Reach surfaces and app windows attaches to the target and foreground input threads (`AttachThreadInput` in `window_win32.cpp`, `window_manager_win32.cpp`, `reach_service/window_actions.cpp`) to bypass the foreground lock. A hung foreground app can stall the caller; revisit only if that is seen.

## Polling

- Polling is prohibited unless the user approves a narrow exception.
- `system_stats` (`src/services/system_stats.cpp`) is the only approved exception and is not a precedent.
