#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CMAKE_FILE = ROOT / "CMakeLists.txt"
ARCHITECTURE_DOC = ROOT / "docs" / "architecture.md"

SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp"}

WINDOWS_LIBS = {
    "advapi32",
    "comctl32",
    "d2d1",
    "d3d11",
    "dcomp",
    "dwrite",
    "dwmapi",
    "dxgi",
    "ole32",
    "runtimeobject",
    "shell32",
    "shcore",
    "shlwapi",
    "user32",
    "uuid",
    "windowscodecs",
}

WIN32_TOKEN_PATTERNS = [
    r"#\s*include\s*<\s*windows\.h\s*>",
    r"#\s*include\s*<\s*dwrite\.h\s*>",
    r"#\s*include\s*<\s*shlwapi\.h\s*>",
    r"\bHWND\b",
    r"\bHICON\b",
    r"\bHMODULE\b",
    r"\bHHOOK\b",
    r"\bIDWrite\w*\b",
    r"\bDWRITE_[A-Z0-9_]+\b",
    r"\bSetCapture\b",
    r"\bReleaseCapture\b",
    r"\bGetCapture\b",
    r"\bSetWindowsHookEx[A-Z]?\b",
    r"\bUnhookWindowsHookEx\b",
    r"\bWindowFromPoint\b",
    r"\bPathFindFileNameW\b",
    r"reach/platform/windows_adapters\.h",
]

# Temporary transition debt. Every entry here must be documented in
# docs/architecture.md. Keep this list narrow.
WIN32_ALLOWLIST = {
    "src/app/main.cpp",
    "src/app/config_path.cpp",
    "src/app/pin_config.cpp",
    "src/shell/shell.cpp",
    "src/shell/shell_input.cpp",
    "src/shell/shell_render.cpp",
    "src/shell/shell_update.cpp",
    "src/support/util.cpp",
    "src/adapters/windows/monitor_win32.cpp",
    "src/adapters/windows/hotkeys_win32.cpp",
}

WIN32_ALLOWED_PREFIXES = (
    "src/adapters/windows/",
    "src/tools/",
)

# This include is allowed by the architecture: app composition wires concrete
# Windows adapters into platform-neutral ports.
WINDOWS_ADAPTER_FACTORY_INCLUDE_ALLOWLIST = {
    "src/app/composition_root.cpp",
}

# Root-level files should not exist after the layer-folder cleanup.
# Keep these empty unless you intentionally add documented compatibility wrappers.
ROOT_SRC_ALLOWLIST: set[str] = set()
ROOT_INCLUDE_ALLOWLIST: set[str] = set()

# Current documented transition debt in CMake target membership.
SHELL_ADAPTER_SOURCE_DEBT = {
    "src/adapters/windows/hotkeys_win32.cpp",
    "src/adapters/windows/monitor_win32.cpp",
}

SHELL_APP_SOURCE_DEBT = {
    "src/app/pin_config.cpp",
}

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+[<"]([^>"]+)[>"]', re.MULTILINE)


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def is_source(path: Path) -> bool:
    return path.suffix.lower() in SOURCE_EXTENSIONS


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore") if path.exists() else ""


def iter_sources() -> list[Path]:
    paths: list[Path] = []
    for base in ("include", "src"):
        root = ROOT / base
        if root.exists():
            paths.extend(
                path for path in root.rglob("*") if path.is_file() and is_source(path)
            )
    return paths


def strip_comments_for_token_scan(text: str) -> str:
    # Good enough for architecture checks. Prevents most false positives from comments.
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    text = re.sub(r"//.*", "", text)
    return text


def has_allowed_win32_path(relative: str) -> bool:
    return relative in WIN32_ALLOWLIST or any(
        relative.startswith(prefix) for prefix in WIN32_ALLOWED_PREFIXES
    )


def include_violations(relative: str, text: str) -> list[str]:
    violations: list[str] = []
    includes = INCLUDE_RE.findall(text)

    if relative.startswith("src/core/") or relative.startswith("include/reach/core/"):
        forbidden = (
            "reach/shell",
            "reach/features",
            "reach/platform/",
            "adapters/windows",
        )
        for include in includes:
            if include.startswith(forbidden):
                violations.append(f"{relative}: core must not include {include}")

    if relative.startswith("src/features/") or relative.startswith(
        "include/reach/features/"
    ):
        forbidden = (
            "reach/shell",
            "reach/platform/",
            "adapters/windows",
            "reach/platform/windows_adapters.h",
        )
        for include in includes:
            if include.startswith(forbidden):
                violations.append(f"{relative}: features must not include {include}")

    if relative.startswith("src/shell/") or relative.startswith("include/reach/shell/"):
        for include in includes:
            if "adapters/windows" in include:
                violations.append(
                    f"{relative}: shell must not include adapter internals {include}"
                )

    if relative.startswith("include/reach/ports/"):
        for include in includes:
            if (
                "adapters/windows" in include
                or include.startswith("reach/platform/")
                or include.lower() in {"windows.h", "dwrite.h", "shlwapi.h"}
            ):
                violations.append(f"{relative}: ports must not include {include}")

    return violations


def win32_violations(relative: str, text: str) -> list[str]:
    if has_allowed_win32_path(relative):
        return []

    scan_text = strip_comments_for_token_scan(text)
    violations: list[str] = []

    for pattern in WIN32_TOKEN_PATTERNS:
        if (
            pattern == r"reach/platform/windows_adapters\.h"
            and relative in WINDOWS_ADAPTER_FACTORY_INCLUDE_ALLOWLIST
        ):
            continue

        if re.search(pattern, scan_text, flags=re.IGNORECASE):
            violations.append(
                f"{relative}: Win32/platform token outside allowed paths: {pattern}"
            )

    return violations


def root_folder_violations() -> list[str]:
    violations: list[str] = []

    src_root = ROOT / "src"
    if src_root.exists():
        for path in src_root.iterdir():
            if path.is_file() and is_source(path):
                relative = rel(path)
                if relative not in ROOT_SRC_ALLOWLIST:
                    violations.append(
                        f"{relative}: source file must not live directly under src/"
                    )

    include_root = ROOT / "include" / "reach"
    if include_root.exists():
        for path in include_root.iterdir():
            if path.is_file() and is_source(path):
                relative = rel(path)
                if relative not in ROOT_INCLUDE_ALLOWLIST:
                    violations.append(
                        f"{relative}: header must not live directly under include/reach/"
                    )

    return violations


def parse_cmake_target_blocks(cmake_text: str, command: str, target: str) -> list[str]:
    pattern = re.compile(
        rf"{re.escape(command)}\s*\(\s*{re.escape(target)}\b", re.IGNORECASE
    )
    bodies: list[str] = []

    for match in pattern.finditer(cmake_text):
        start = match.end()
        depth = 1
        i = start
        while i < len(cmake_text) and depth > 0:
            if cmake_text[i] == "(":
                depth += 1
            elif cmake_text[i] == ")":
                depth -= 1
            i += 1

        if depth == 0:
            bodies.append(cmake_text[start : i - 1])

    return bodies


def cmake_tokens_from_bodies(bodies: list[str]) -> set[str]:
    tokens: set[str] = set()

    for body in bodies:
        body = re.sub(r"#.*", "", body)
        for token in re.split(r"[\s\r\n\t]+", body):
            token = token.strip()
            if token:
                tokens.add(token)

    return tokens


def cmake_target_sources(cmake_text: str, target: str) -> set[str]:
    sources: set[str] = set()

    for command in ("add_library", "add_executable", "target_sources"):
        bodies = parse_cmake_target_blocks(cmake_text, command, target)
        for token in cmake_tokens_from_bodies(bodies):
            if token.startswith("src/") or token.startswith("include/"):
                sources.add(token)

    return sources


def cmake_target_links(cmake_text: str, target: str) -> set[str]:
    bodies = parse_cmake_target_blocks(cmake_text, "target_link_libraries", target)
    return cmake_tokens_from_bodies(bodies)


def cmake_violations() -> list[str]:
    violations: list[str] = []
    cmake_text = read(CMAKE_FILE)

    if not cmake_text:
        violations.append("CMakeLists.txt: missing or unreadable")
        return violations

    core_links = {
        token.lower() for token in cmake_target_links(cmake_text, "reach_core")
    }
    feature_links = {
        token.lower() for token in cmake_target_links(cmake_text, "reach_features")
    }
    shell_links = {
        token.lower() for token in cmake_target_links(cmake_text, "reach_shell")
    }

    for lib in sorted(WINDOWS_LIBS):
        if lib in core_links:
            violations.append(
                f"CMakeLists.txt: reach_core must not link Windows/platform library {lib}"
            )
        if lib in feature_links:
            violations.append(
                f"CMakeLists.txt: reach_features must not link Windows/platform library {lib}"
            )

    if "reach_support" in core_links:
        violations.append(
            "CMakeLists.txt: reach_core must not link reach_support while support has platform debt"
        )

    if "reach_windows_adapters" in feature_links:
        violations.append(
            "CMakeLists.txt: reach_features must not link reach_windows_adapters"
        )

    if "reach_windows_adapters" in shell_links:
        violations.append(
            "CMakeLists.txt: reach_shell must not link reach_windows_adapters"
        )

    core_sources = cmake_target_sources(cmake_text, "reach_core")
    feature_sources = cmake_target_sources(cmake_text, "reach_features")
    shell_sources = cmake_target_sources(cmake_text, "reach_shell")
    adapter_sources = cmake_target_sources(cmake_text, "reach_windows_adapters")

    for source in sorted(core_sources):
        if source.startswith(
            ("src/adapters/windows/", "src/shell/", "src/features/", "src/app/")
        ):
            violations.append(f"CMakeLists.txt: reach_core must not compile {source}")

    for source in sorted(feature_sources):
        if source.startswith(("src/adapters/windows/", "src/shell/", "src/app/")):
            violations.append(
                f"CMakeLists.txt: reach_features must not compile {source}"
            )

    for source in sorted(shell_sources):
        if (
            source.startswith("src/adapters/windows/")
            and source not in SHELL_ADAPTER_SOURCE_DEBT
        ):
            violations.append(
                f"CMakeLists.txt: reach_shell must not compile adapter source {source}"
            )
        if source.startswith("src/app/") and source not in SHELL_APP_SOURCE_DEBT:
            violations.append(
                f"CMakeLists.txt: reach_shell should not compile app-layer source {source}"
            )

    architecture_doc = read(ARCHITECTURE_DOC)

    for source in sorted(SHELL_ADAPTER_SOURCE_DEBT):
        if source in shell_sources and source not in architecture_doc:
            violations.append(
                f"CMakeLists.txt/docs: {source} is compiled into reach_shell but is not documented as transition debt"
            )

    for source in sorted(SHELL_APP_SOURCE_DEBT):
        if source in shell_sources and source not in architecture_doc:
            violations.append(
                f"CMakeLists.txt/docs: {source} is compiled into reach_shell but is not documented as transition debt"
            )

    for source in sorted(adapter_sources):
        if not source.startswith("src/adapters/windows/"):
            violations.append(
                f"CMakeLists.txt: reach_windows_adapters should not compile non-adapter source {source}"
            )

    return violations


def docs_allowlist_violations() -> list[str]:
    violations: list[str] = []
    doc_text = read(ARCHITECTURE_DOC)

    if not doc_text:
        violations.append("docs/architecture.md: missing or unreadable")
        return violations

    documented_allowlist_entries = set(WIN32_ALLOWLIST)
    documented_allowlist_entries.update(ROOT_SRC_ALLOWLIST)
    documented_allowlist_entries.update(ROOT_INCLUDE_ALLOWLIST)
    documented_allowlist_entries.update(SHELL_ADAPTER_SOURCE_DEBT)
    documented_allowlist_entries.update(SHELL_APP_SOURCE_DEBT)

    for relative in sorted(documented_allowlist_entries):
        if relative not in doc_text:
            violations.append(
                f"docs/architecture.md: architecture allowlist/debt entry is undocumented: {relative}"
            )

    return violations


def main() -> int:
    violations: list[str] = []

    violations.extend(root_folder_violations())
    violations.extend(cmake_violations())
    violations.extend(docs_allowlist_violations())

    for path in iter_sources():
        relative = rel(path)
        text = read(path)
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
