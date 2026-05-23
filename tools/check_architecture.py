#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]

SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp"}

WIN32_TOKENS = [
    "#include <windows.h>",
    "HWND",
    "HICON",
    "HMODULE",
    "SetCapture",
    "ReleaseCapture",
    "GetCapture",
    "SetWindowsHookEx",
    "UnhookWindowsHookEx",
    "WindowFromPoint",
    "IDWrite",
]

# Temporary transition debt documented in docs/architecture.md. Keep this list
# narrow so new Win32 usage outside adapters/windows is caught by default.
WIN32_ALLOWLIST = {
    "src/main.cpp",
    "src/shell/shell_input.cpp",
    "src/shell/shell_render.cpp",
    "src/shell/shell_update.cpp",
    "src/util.cpp",
    "src/config_path.cpp",
    "src/pin_config.cpp",
    "src/monitor.cpp",
    "src/hotkeys.cpp",
}

WIN32_ALLOWED_PREFIXES = (
    "src/adapters/windows/",
    "src/app/",
    "src/tools/",
)

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.MULTILINE)


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def is_source(path: Path) -> bool:
    return path.suffix.lower() in SOURCE_EXTENSIONS


def iter_sources() -> list[Path]:
    paths: list[Path] = []
    for base in ("include", "src"):
        root = ROOT / base
        if root.exists():
            paths.extend(path for path in root.rglob("*") if path.is_file() and is_source(path))
    return paths


def has_allowed_win32_path(relative: str) -> bool:
    return relative in WIN32_ALLOWLIST or any(relative.startswith(prefix) for prefix in WIN32_ALLOWED_PREFIXES)


def include_violations(relative: str, text: str) -> list[str]:
    violations: list[str] = []
    includes = INCLUDE_RE.findall(text)

    if relative.startswith("src/core/") or relative.startswith("include/reach/core/"):
        forbidden = ("reach/shell", "reach/features", "reach/platform/", "adapters/windows")
        for include in includes:
            if include.startswith(forbidden):
                violations.append(f"{relative}: core must not include {include}")

    if relative.startswith("src/features/") or relative.startswith("include/reach/features/"):
        forbidden = ("reach/shell", "reach/platform/", "adapters/windows")
        for include in includes:
            if include.startswith(forbidden):
                violations.append(f"{relative}: features must not include {include}")

    if relative.startswith("src/shell/") or relative.startswith("include/reach/shell"):
        for include in includes:
            if "adapters/windows" in include:
                violations.append(f"{relative}: shell must not include adapter internals {include}")

    if relative.startswith("include/reach/ports/"):
        for include in includes:
            if "adapters/windows" in include or include.startswith("reach/platform/"):
                violations.append(f"{relative}: ports must not include {include}")

    return violations


def win32_violations(relative: str, text: str) -> list[str]:
    if has_allowed_win32_path(relative):
        return []

    violations: list[str] = []
    for token in WIN32_TOKENS:
        if token in text:
            violations.append(f"{relative}: Win32 token outside allowed paths: {token}")
    return violations


def main() -> int:
    violations: list[str] = []
    for path in iter_sources():
        relative = rel(path)
        text = path.read_text(encoding="utf-8", errors="ignore")
        violations.extend(include_violations(relative, text))
        violations.extend(win32_violations(relative, text))

    if violations:
        print("Architecture check failed:")
        for violation in violations:
            print(f"  {violation}")
        return 1

    print("Architecture check passed.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
