# Reach Architecture

Reach follows Clean Architecture with ports and adapters. Dependencies must point inward. Inner layers define policy and stable data. Outer layers perform composition, platform work, IO, persistence, and process integration.

This document is the architecture contract for new code.

## Dependency Rule

- Inner layers must not depend on outer layers.
- Product policy must not depend on implementation details.
- Windows APIs, COM, Direct2D, DirectComposition, registry access, shell APIs, process APIs, file formats, and external services are details.
- Data crossing inward must be neutral project data, not native Windows objects.
- Features decide behavior; shell orchestrates; adapters perform platform work;
  app composition wires concrete dependencies.

## Layers

### `include/reach/support` and `src/support`

Shared low-level utilities.

Rules:

- May not depend on app, shell, features, ports, platform, or adapters.
- Must stay platform-neutral.
- Must not include Windows headers or expose Windows types.

### `include/reach/core` and `src/core`

Stable product/domain types and pure state models.

Rules:

- May depend only on support, or on nothing.
- Must not depend on ports, features, shell, app, platform, or adapters.
- Must not include Windows headers or expose Windows types.
- Shared domain models belong here when they are used across layers.

### `include/reach/ports`

Platform-neutral capability interfaces.

Rules:

- May depend only on core and support.
- Must not depend on app, shell, features, platform, or adapters.
- Must not include Windows headers or expose native Windows types.
- Must describe capabilities, not Windows mechanisms.

### `include/reach/features` and `src/features`

Feature policy, feature models, hit testing, rendering inputs, and feature actions.

Rules:

- May depend only on core, ports, and support.
- Must not depend on app, shell, platform, or adapters.
- Must not include Windows headers or call Windows APIs.
- Must not know registry, config-file formats, shell replacement mechanics, Direct2D, DirectComposition, process APIs, or AppUserModel implementation details.
- Should operate on data and ports passed in.
- Should return actions/intents when shell or adapters must perform effects.

### `include/reach/shell` and `src/shell`

Shell orchestration.

Rules:

- May depend on core, ports, features, and support.
- Must not depend on app, platform headers, or adapter implementations.
- Must not include Windows headers or call Windows APIs.
- Owns lifecycle orchestration, input routing, feature coordination, reloads, dirty flags, surface state, and port calls.
- Must not absorb feature policy that belongs in `features`.

### `include/reach/app` and `src/app`

Executable startup and composition.

Rules:

- May depend on shell, ports, features, core, support, platform factory headers,
  and adapters through composition.
- Owns application startup, composition, and outer process lifecycle.
- Owns executable-specific runtimes, including the Settings process.
- May contain deliberate executable-bound platform integration.
- Must not become a second shell implementation.
- Must not move feature policy or adapter implementation details into app code.

### `include/reach/platform`

Outer platform-facing declarations used by composition and tools.

Rules:

- May describe platform factories, platform messages, and platform registration surfaces.
- Must not be included by core, ports, features, or shell.
- Must not be used as a back door around ports.

### `src/adapters/windows`

Windows implementations of ports and Windows-specific integration.

Rules:

- May depend inward on ports, core, and support.
- May use Win32, COM, WIC, Direct2D, DirectComposition, registry, shell APIs, process APIs, AppUserModel APIs, and native Windows handles.
- Must not leak Windows types or platform assumptions into core, ports, features, or shell.
- Owns Windows metadata extraction and platform resource lifetime.

### `src/tools`

CLI and diagnostic tools.

Rules:

- May use app composition surfaces, feature policy, ports, support, platform
  declarations, and Windows adapters as needed.
- Must not become another shell implementation.
- Diagnostic behavior must be labelled as diagnostic behavior.

### `tests`

Tests for neutral behavior and isolated platform behavior.

Rules:

- Prefer tests for core, features, ports-facing policy, and support without
  requiring platform state.
- Platform-specific tests must remain isolated.
- New architectural rules should be covered by the architecture check.

## Allowed Dependencies

- support -> nothing
- core -> support
- ports -> core, support
- features -> core, ports, support
- shell -> core, ports, features, support
- app -> shell, ports, features, core, support, platform, adapters
- adapters/windows -> ports, core, support
- tools -> app, platform, adapters, features, ports, core, support
- tests -> the layer under test and its allowed inward dependencies

No other dependency direction is allowed.

## Forbidden Dependencies

- core -> ports, features, shell, app, platform, adapters
- ports -> features, shell, app, platform, adapters
- features -> shell, app, platform, adapters
- shell -> app, platform, adapters
- support -> core, ports, features, shell, app, platform, adapters
- adapters/windows -> shell or app policy
- any inner layer -> Windows headers or native Windows types

## Identity Rules

- `path` means launch identity.
- `arguments` means launch arguments.
- `icon_ref` means icon lookup identity only.
- `app_user_model_id` means match/group identity.
- Dock matching must use AppUserModelID first, then exact launch path.
- Dock matching must not use filename, folder ancestry, launch-time
  correlation, or icon reference as identity.
- Pinning from a running window must ask a platform-neutral port for pin metadata.
- Platform relaunch metadata belongs in Windows adapters.
- Relaunch metadata probing must be on demand, not part of hot window snapshot refresh.

## Runtime Rules

- Window procedures must translate native messages into queued project events.
- Heavy work must happen in shell update/orchestration, not inside native window callbacks.
- Runtime debug logging must not remain in hot paths.
- Native resources must have clear owners and release paths.
- Caches must have explicit refresh or eviction rules.
- Config changes from tools must notify the running shell.
- The running shell must reload relevant config live.

## Adding Code

1. Put stable product data in core.
2. Put feature behavior in features.
3. Put capability interfaces in ports.
4. Put orchestration in shell.
5. Put composition and executable startup in app.
6. Put Windows implementation details in adapters/windows.
7. Put CLI behavior in tools.
8. Add or update tests at the lowest layer that can verify the behavior.
9. Extend the architecture check when adding a new boundary rule.

## Required Verification

Before finishing architecture-affecting work, run:

```powershell
cmake -S . -B build
cmake --build build
ctest --test-dir build --output-on-failure
```

The architecture check is part of the contract. Do not weaken it or add allowlist entries without explicit approval.
