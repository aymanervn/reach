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

- `src/main.cpp`: executable entry point, command-line shell registration, COM,
  and Windows message loop setup.
- `src/shell/shell.cpp`: current shell implementation still directly uses Win32
  types and Windows adapter factories.
- `src/util.cpp`: logging currently uses `OutputDebugStringA`.
- `src/config_path.cpp`: default config path lookup currently uses Win32 path
  APIs.
- `src/pin_config.cpp`: pin matching currently uses Win32 path helpers.
- `src/monitor.cpp`: monitor enumeration currently uses Win32 display APIs.
- `src/hotkeys.cpp`: global hotkey registration currently uses Win32 APIs.
- `src/tools/*.cpp`: developer and support tools are Windows-specific today.

These exceptions should shrink as behavior moves behind ports or into Windows
adapters. New Win32 usage outside `src/adapters/windows` should not be added
without updating this section.

## Internal CMake Targets

- `reach_core`: `src/core/*.c`; pure core logic; no linked support or platform
  libraries.
- `reach_features`: feature modules such as dock item identity and ordering;
  links `reach_core`.
- `reach_support`: shared helper code used across layers.
- `reach_windows_adapters`: `src/adapters/windows/*.cpp`; Windows port
  implementations and adapter factories; links `reach_support`.
- `reach_shell`: current shell implementation; links `reach_features`,
  `reach_core`, and `reach_support`.
- `reach`: executable and app composition root; links `reach_shell`,
  `reach_core`, `reach_support`, and `reach_windows_adapters`.

`reach_support` currently owns Windows-dependent support files such as
`src/util.cpp` and `src/config_path.cpp`, so pure core code must not link it.

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

- `src/shell/shell.cpp` still contains popup/capture Win32 orchestration while
  the shell is being thinned. The exact popup/capture APIs still present are
  `SetCapture`, `ReleaseCapture`, `GetCapture`, `SetWindowsHookExW`,
  `UnhookWindowsHookEx`, and `WindowFromPoint`.
- `src/shell/shell.cpp` still contains native context-menu hook and window
  styling helpers. These remain shell transition debt until popup capture and
  native context-menu behavior move behind ports or Windows adapters.
- `src/shell/shell.cpp` still contains switcher rendering/model helpers and
  wallpaper orchestration. These were not moved in the current pass because the
  launcher and context-menu extractions were the safer mechanical boundary.
