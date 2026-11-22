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
- Feature definitions may declare open-only and close-only events beside toggle events. A close event that closes an open surface is consumed.

## Ports, adapters, services

- A port expresses a generic Reach contract, not a thin copy of an OS API. OS code stays in adapters.
- Ongoing OS-derived state lives in a service over its port; the service owns refresh, snapshots, caching, threading, generations and policy. A direct read is fine only when it is simple and stateless.
- `reach_config_service` is the only owner of live config in a process. Features read snapshot copies and submit typed mutations. Only composition and bootstrap code hold `reach_config_store_port`.
- A surface that owns work-area policy publishes management and reservation intent with its geometry and far-edge clearance. Composition derives the band without knowing feature styles; the monitor port applies it (`manage_monitor_work_area`, `reserve_monitor_work_area`, `set_work_area`). `reachctl` uses the same port for repair.

## Confirmed workarounds

- The Quick Settings Project tile injects Win+P (`reach_system_controls_open_project_menu`); Windows has no API for that flyout.
- Focusing Reach surfaces and app windows attaches to the target and foreground input threads (`AttachThreadInput` in `window_win32.cpp`, `window_manager_win32.cpp`, `reach_service/window_actions.cpp`) to bypass the foreground lock. A hung foreground app can stall the caller; revisit only if that is seen.

## Polling

- Polling is prohibited unless the user approves a narrow exception.
- `system_stats` (`src/services/system_stats.cpp`) is the only approved exception and is not a precedent.
