# Reach Architecture

Reach uses a modular ports-and-adapters architecture. The core application
policy is kept away from platform APIs, while Windows-specific code stays at
the edge behind ports and adapter factories.

## Layers

- `app`: the executable entry point and composition root.
- `shell`: the thin orchestrator that coordinates core state, feature logic,
  ports, and support code.
- `features`: user-facing behavior modules built from core models and ports.
- `core`: pure C application state, layout, events, and render command logic.
- `ports`: platform-neutral interfaces consumed by shell and features.
- `support`: shared non-platform helper code.
- `adapters/windows`: Windows implementations of ports and adapter factories.

## Allowed Dependencies

- `app -> shell`
- `app -> adapters/windows` only through adapter factories and composition root
- `shell -> features`
- `shell -> core`
- `shell -> ports`
- `shell -> support`
- `features -> core`
- `features -> ports`
- `features -> support`
- `adapters/windows -> ports`
- `adapters/windows -> support`
- `core -> support` only

## Forbidden Dependencies

- `core -> shell`
- `core -> features`
- `core -> adapters/windows`
- `features -> shell`
- `features -> adapters/windows`
- `shell -> adapters/windows` internals
- `ports -> adapters/windows`

## Windows Boundary

Only files under `src/adapters/windows` should directly use Win32 APIs or
Win32 types. The app composition root may call adapter factory functions to wire
ports together.

Temporary transition exceptions:

- `src/app/main.cpp`: executable entry point, command-line shell registration, COM,
  and Windows message loop setup.
- `src/shell/shell.cpp`: public shell creation glue still directly uses Windows
  adapter factories while the compatibility entry point remains in shell.
- `src/shell/shell_input.cpp`: current shell input orchestration still uses
  Win32 types for launcher activation and transitional popup/context-menu
  handling.
- `src/shell/shell_render.cpp`: current shell render orchestration still uses
  DirectWrite constants and Win32 path helpers while text alignment and
  switcher label derivation remain shell-owned.
- `src/shell/shell_update.cpp`: current shell update orchestration still uses
  Win32 path comparison, cursor helpers, and wallpaper reload orchestration.
- `src/support/util.cpp`: logging currently uses `OutputDebugStringA`.
- `src/app/config_path.cpp`: default config path lookup currently uses Win32 path
  APIs.
- `src/app/pin_config.cpp`: pin matching currently uses Win32 path helpers.
- `src/adapters/windows/monitor_win32.cpp`: monitor enumeration currently uses
  Win32 display APIs and is still compiled into `reach_shell` until monitor
  access moves behind a port.
- `src/adapters/windows/hotkeys_win32.cpp`: global hotkey registration
  currently uses Win32 APIs and is still compiled into `reach_shell` until
  hotkey access moves behind a port.
- `src/tools/*.cpp`: developer and support tools are Windows-specific today.

These exceptions should shrink as behavior moves behind ports or into Windows
adapters. New Win32 usage outside `src/adapters/windows` should not be added
without updating this section.

## Internal CMake Targets

- `reach_core`: `src/core/*.c`; pure core logic and theme defaults; no linked
  support or platform libraries.
- `reach_features`: feature modules such as dock item identity and ordering;
  links `reach_core`.
- `reach_support`: shared helper code under `src/support`.
- `reach_windows_adapters`: `src/adapters/windows/*.cpp`; Windows port
  implementations and adapter factories; links `reach_support`. Transitional
  hotkey and monitor implementations live under `src/adapters/windows` but are
  still compiled into `reach_shell` until port boundaries are added.
- `reach_shell`: current shell implementation; links `reach_features`,
  `reach_core`, and `reach_support`.
- `reach`: executable and app composition root; links `reach_shell`,
  `reach_core`, `reach_support`, and `reach_windows_adapters`.

`reach_support` currently owns Windows-dependent support code in
`src/support/util.cpp`, so pure core code must not link it.

The target graph is intended to converge on the allowed dependency graph above.
Where source files still violate it, the transition exceptions list is the
contract for follow-up refactors.

## Architecture Checks

The `check_architecture` CMake target runs `tools/check_architecture.py`.
It flags forbidden includes across core, features, shell, and ports, and it
flags direct Win32 API/type tokens outside the documented transition allowlist.
The allowlist is intentionally temporary and should shrink with the transition
exceptions above.

## Remaining Transition Debt

- `src/shell/shell.cpp` still contains public shell creation glue that directly
  calls Windows adapter factories. New composition should continue to prefer
  `src/app/composition_root.cpp`.
- `src/shell/shell_input.cpp` still contains native input and context-menu
  transition code, including Win32 window handles and native menu hook helpers.
- `src/shell/shell_render.cpp` still owns DirectWrite text constants and
  Win32 path-label derivation before calling feature render builders.
- `src/shell/shell_update.cpp` still owns Win32 path comparison, cursor helpers,
  and wallpaper reload orchestration. Wallpaper seed/apply logic has moved to
  the wallpaper feature.
- `src/adapters/windows/hotkeys_win32.cpp` and
  `src/adapters/windows/monitor_win32.cpp` are in the Windows adapter folder but
  still compile into `reach_shell` until hotkey and monitor access move behind
  ports.
- `src/app/pin_config.cpp` is still compiled into `reach_shell` while pinned-app loading/matching remains shell-owned; this should move behind a cleaner app/config boundary later.
