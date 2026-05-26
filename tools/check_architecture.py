#!/usr/bin/env python3
from __future__ import annotations

import re
import sys
from dataclasses import dataclass
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
ARCHITECTURE_DOC = ROOT / "docs" / "architecture.md"

SOURCE_EXTENSIONS = {".c", ".cc", ".cpp", ".h", ".hpp"}

RED = "\033[31m"
GREEN = "\033[32m"
YELLOW = "\033[33m"
RESET = "\033[0m"

INCLUDE_RE = re.compile(r'^\s*#\s*include\s+([<"])([^>"]+)[>"]', re.MULTILINE)

# Very small, conservative CMake parser.
# It is not a full CMake interpreter. It catches ordinary target_link_libraries(...)
# declarations and should be treated as an additional guardrail, not the source of truth.
TARGET_LINK_LIBRARIES_RE = re.compile(
    r"target_link_libraries\s*\(\s*([A-Za-z0-9_:\-\.]+)\s+(.+?)\)",
    re.IGNORECASE | re.DOTALL,
)

CMAKE_SCOPE_KEYWORDS = {
    "PUBLIC",
    "PRIVATE",
    "INTERFACE",
    "debug",
    "optimized",
    "general",
}

ALLOWED_SRC_LAYER_DIRS = {
    "adapters",
    "app",
    "core",
    "features",
    "shell",
    "support",
    "tools",
}

ALLOWED_INCLUDE_LAYER_DIRS = {
    "app",
    "core",
    "features",
    "platform",
    "ports",
    "shell",
    "support",
}

ALLOWED_LAYER_DEPENDENCIES: dict[str, set[str]] = {
    "support": set(),
    "core": {"support"},
    "ports": {"core", "support"},
    "features": {"core", "ports", "support"},
    "shell": {"core", "ports", "features", "support"},
    "app": {"shell", "ports", "features", "core", "support", "platform", "adapters"},
    "platform": {"ports", "core", "support"},
    "adapters": {"ports", "core", "support"},
    "tools": {"app", "platform", "adapters", "features", "ports", "core", "support"},
    "tests": {
        "app",
        "shell",
        "features",
        "ports",
        "core",
        "support",
        "platform",
        "adapters",
    },
}

INNER_LAYERS = {"support", "core", "ports", "features", "shell"}
OUTER_LAYERS = {"app", "platform", "adapters", "tools", "tests"}
WINDOWS_ALLOWED_LAYERS = OUTER_LAYERS

# Map CMake targets to architectural layers. Prefer exact project targets over
# naming heuristics so the checker does not classify Reach targets incorrectly.
EXACT_TARGET_LAYERS = {
    "reach_support": "support",
    "reach_core": "core",
    "reach_features": "features",
    "reach_shell": "shell",
    "reach_windows_adapters": "adapters",
    "reach": "app",
    "reachctl": "tools",
    "reach_tray_probe": "tools",
}

TARGET_LAYER_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"(^|::|_|-)(support)(_|-|$)", re.IGNORECASE), "support"),
    (
        re.compile(r"(^|::|_|-)(core|domain|entity|entities)(_|-|$)", re.IGNORECASE),
        "core",
    ),
    (
        re.compile(r"(^|::|_|-)(port|ports|boundary|boundaries)(_|-|$)", re.IGNORECASE),
        "ports",
    ),
    (
        re.compile(
            r"(^|::|_|-)(feature|features|usecase|usecases|interactor|interactors)(_|-|$)",
            re.IGNORECASE,
        ),
        "features",
    ),
    (re.compile(r"(^|::|_|-)(shell)(_|-|$)", re.IGNORECASE), "shell"),
    (re.compile(r"(^|::|_|-)(app|application)(_|-|$)", re.IGNORECASE), "app"),
    (
        re.compile(
            r"(^|::|_|-)(adapter|adapters|gateway|gateways|presenter|presenters|controller|controllers)(_|-|$)",
            re.IGNORECASE,
        ),
        "adapters",
    ),
    (
        re.compile(
            r"(^|::|_|-)(platform|win32|windows|linux|macos|os)(_|-|$)", re.IGNORECASE
        ),
        "platform",
    ),
    (re.compile(r"(^|::|_|-)(tool|tools|cli)(_|-|$)", re.IGNORECASE), "tools"),
    (re.compile(r"(^|::|_|-)(test|tests|testing)(_|-|$)", re.IGNORECASE), "tests"),
]

WINDOWS_HEADER_NAMES = {
    "windows.h",
    "dwrite.h",
    "d2d1.h",
    "d3d11.h",
    "dcomp.h",
    "dxgi.h",
    "dwmapi.h",
    "shlwapi.h",
    "shlobj.h",
    "shobjidl.h",
    "shellapi.h",
    "wincodec.h",
    "wrl.h",
}

WINDOWS_TOKEN_PATTERNS = [
    r"\bHWND\b",
    r"\bHICON\b",
    r"\bHBITMAP\b",
    r"\bHDC\b",
    r"\bHMODULE\b",
    r"\bHHOOK\b",
    r"\bHRESULT\b",
    r"\bIUnknown\b",
    r"\bIDWrite\w*\b",
    r"\bID2D1\w*\b",
    r"\bIDComposition\w*\b",
    r"\bDWRITE_[A-Z0-9_]+\b",
    r"\bSetCapture\b",
    r"\bReleaseCapture\b",
    r"\bGetCapture\b",
    r"\bSetWindowsHookEx[A-Z]?\b",
    r"\bUnhookWindowsHookEx\b",
    r"\bWindowFromPoint\b",
    r"\bPathFindFileNameW\b",
]

# These are deliberately heuristic. They catch common semantic leakage that an include
# dependency check will miss. Keep them strict for public inner-layer headers, but expect
# to tune the list if the project has legitimate domain words that collide.
PUBLIC_INNER_FORBIDDEN_TOKEN_PATTERNS: dict[str, list[str]] = {
    "support": [
        r"\bController\b",
        r"\bPresenter\b",
        r"\bView(Model)?\b",
        r"\bRepository\b",
        r"\bSql\w*\b",
        r"\bHttp\w*\b",
        r"\bJson\w*\b",
        r"\bXml\w*\b",
        r"\bWindow\w*\b",
        r"\bWidget\w*\b",
    ],
    "core": [
        r"\bController\b",
        r"\bPresenter\b",
        r"\bView(Model)?\b",
        r"\bRepository(Impl|Implementation)?\b",
        r"\bGateway(Impl|Implementation)?\b",
        r"\bSql\w*\b",
        r"\bDatabase\b",
        r"\bDb\b",
        r"\bHttp\w*\b",
        r"\bJson\w*\b",
        r"\bXml\w*\b",
        r"\bWindow\w*\b",
        r"\bWidget\w*\b",
        r"\bScreen\b",
        r"\bButton\b",
        r"\bMouse\b",
        r"\bKeyboard\b",
    ],
    "ports": [
        r"\bController\b",
        r"\bPresenter\b",
        r"\bView(Model)?\b",
        r"\bRepositoryImpl\b",
        r"\bGatewayImpl\b",
        r"\bSql\w*\b",
        r"\bDatabase\b",
        r"\bDbConnection\b",
        r"\bHttpClient\b",
        r"\bWin32\b",
        r"\bWindows\b",
        r"\bNative(Window|Handle|View|Widget)\b",
        r"\bWindow\w*\b",
        r"\bWidget\w*\b",
    ],
    "features": [
        r"\bController\b",
        r"\bPresenterImpl\b",
        r"\bView(Model)?Impl\b",
        r"\bRepositoryImpl\b",
        r"\bGatewayImpl\b",
        r"\bSql\w*\b",
        r"\bDatabase\b",
        r"\bDbConnection\b",
        r"\bHttpClient\b",
        r"\bWin32\b",
        r"\bWindows\b",
        r"\bWindow\w*\b",
        r"\bWidget\w*\b",
    ],
    "shell": [
        r"\bRepositoryImpl\b",
        r"\bGatewayImpl\b",
        r"\bSql\w*\b",
        r"\bDatabase\b",
        r"\bDbConnection\b",
        r"\bHttpClient\b",
        r"\bWin32\b",
        r"\bWindows\b",
        r"\bWindow\w*\b",
        r"\bWidget\w*\b",
    ],
}

# Inner-layer public headers should not expose implementation/storage/framework headers.

# These patterns are suspicious in public ports because they often mean an untyped
# platform handle is crossing the boundary. They are warnings, not failures, because
# C/C++ ports often use opaque handles deliberately to keep OS headers out of the
# inner API. Prefer named project-owned handle typedefs where possible.
PUBLIC_INNER_WARNING_TOKEN_PATTERNS: dict[str, list[str]] = {
    "ports": [
        r"\buintptr_t\b",
        r"\bintptr_t\b",
        r"\bnative_handle\b",
        r"\bsurface_handle\b",
    ],
}

PUBLIC_INNER_FORBIDDEN_INCLUDE_PATTERNS = [
    r"(^|/)(windows|win32|d2d|dwrite|dxgi|dcomp)(/|\.|$)",
    r"(^|/)(sqlite|mysql|postgres|pqxx|odbc)(/|\.|$)",
    r"(^|/)(curl|boost/asio|asio|httplib)(/|\.|$)",
    r"(^|/)(nlohmann|rapidjson|jsoncpp)(/|\.|$)",
    r"(^|/)(qt|gtk|wx|imgui|sdl)(/|\.|$)",
]


@dataclass(frozen=True)
class Include:
    delimiter: str
    value: str


def rel(path: Path) -> str:
    return path.relative_to(ROOT).as_posix()


def read(path: Path) -> str:
    return path.read_text(encoding="utf-8", errors="ignore") if path.exists() else ""


def is_source(path: Path) -> bool:
    return path.suffix.lower() in SOURCE_EXTENSIONS


def iter_source_files() -> list[Path]:
    paths: list[Path] = []
    for base in ("include", "src", "tests"):
        root = ROOT / base
        if root.exists():
            paths.extend(
                path for path in root.rglob("*") if path.is_file() and is_source(path)
            )
    return paths


def iter_cmake_files() -> list[Path]:
    return [p for p in ROOT.rglob("CMakeLists.txt") if ".git" not in p.parts]


def layer_for_path(path: Path) -> str | None:
    relative = rel(path)

    if relative.startswith("include/reach/"):
        parts = relative.split("/")
        if len(parts) >= 3:
            layer = parts[2]
            if layer in ALLOWED_INCLUDE_LAYER_DIRS:
                return layer

    if relative.startswith("src/"):
        parts = relative.split("/")
        if len(parts) >= 2:
            layer = parts[1]
            if layer in ALLOWED_SRC_LAYER_DIRS:
                return layer

    if relative.startswith("tests/"):
        return "tests"

    return None


def layer_for_reach_include(include: str) -> str | None:
    if not include.startswith("reach/"):
        return None

    parts = include.split("/")
    if len(parts) < 2:
        return None

    layer = parts[1]
    if layer in ALLOWED_INCLUDE_LAYER_DIRS:
        return layer
    return None


def resolve_local_include(source: Path, include: str) -> Path | None:
    candidates = [
        source.parent / include,
        ROOT / include,
        ROOT / "include" / include,
        ROOT / "src" / include,
    ]

    for candidate in candidates:
        if candidate.exists() and candidate.is_file():
            return candidate.resolve()

    return None


def layer_for_include(source: Path, include: Include) -> str | None:
    reach_layer = layer_for_reach_include(include.value)
    if reach_layer is not None:
        return reach_layer

    # Resolve quoted and angle-bracket includes. System includes will simply not resolve.
    resolved = resolve_local_include(source, include.value)
    if resolved is not None:
        return layer_for_path(resolved)

    return None


def includes_from(text: str) -> list[Include]:
    return [Include(delimiter, value) for delimiter, value in INCLUDE_RE.findall(text)]


def strip_comments(text: str) -> str:
    text = re.sub(r"/\*.*?\*/", "", text, flags=re.DOTALL)
    return re.sub(r"//.*", "", text)


def is_public_header(path: Path) -> bool:
    return rel(path).startswith("include/reach/") and path.suffix.lower() in {
        ".h",
        ".hpp",
    }


def target_layer(target: str) -> str | None:
    simplified = target.split("/")[-1]
    if simplified in EXACT_TARGET_LAYERS:
        return EXACT_TARGET_LAYERS[simplified]
    for pattern, layer in TARGET_LAYER_PATTERNS:
        if pattern.search(simplified):
            return layer
    return None


def cmake_tokens(body: str) -> list[str]:
    body = re.sub(r"#.*", "", body)
    raw = re.split(r"[\s\n\r\t]+", body.strip())
    return [
        token.strip('"')
        for token in raw
        if token and token not in CMAKE_SCOPE_KEYWORDS and not token.startswith("$<")
    ]


def validate_layer_directories() -> list[str]:
    violations: list[str] = []

    src_root = ROOT / "src"
    if src_root.exists():
        for path in src_root.iterdir():
            if path.is_dir() and path.name not in ALLOWED_SRC_LAYER_DIRS:
                violations.append(f"{rel(path)}: unexpected source layer directory")
            elif path.is_file() and is_source(path):
                violations.append(
                    f"{rel(path)}: source file must live inside a layer folder"
                )

    include_root = ROOT / "include" / "reach"
    if include_root.exists():
        for path in include_root.iterdir():
            if path.is_dir() and path.name not in ALLOWED_INCLUDE_LAYER_DIRS:
                violations.append(
                    f"{rel(path)}: unexpected public include layer directory"
                )
            elif path.is_file() and is_source(path):
                violations.append(
                    f"{rel(path)}: header must live inside a layer folder"
                )

    return violations


def validate_document_contract() -> list[str]:
    text = read(ARCHITECTURE_DOC)
    if not text:
        return ["docs/architecture.md: missing or unreadable"]

    required_terms = (
        "Dependency Rule",
        "Allowed Dependencies",
        "Forbidden Dependencies",
        "Required Verification",
        "Do not weaken",
    )

    return [
        f"docs/architecture.md: missing architecture contract term: {term}"
        for term in required_terms
        if term not in text
    ]


def validate_imports(path: Path, text: str) -> list[str]:
    violations: list[str] = []
    source_layer = layer_for_path(path)
    relative = rel(path)

    if source_layer is None:
        return violations

    allowed = ALLOWED_LAYER_DEPENDENCIES[source_layer]

    for include in includes_from(text):
        imported_layer = layer_for_include(path, include)
        if imported_layer is None:
            continue
        if imported_layer == source_layer:
            continue
        if imported_layer not in allowed:
            violations.append(
                f"{relative}: {source_layer} must not include {imported_layer} "
                f"dependency {include.value}"
            )

    return violations


def validate_windows_boundary(path: Path, text: str) -> list[str]:
    violations: list[str] = []
    source_layer = layer_for_path(path)
    relative = rel(path)

    if source_layer is None or source_layer in WINDOWS_ALLOWED_LAYERS:
        return violations

    for include in includes_from(text):
        include_name = include.value.replace("\\", "/").split("/")[-1].lower()
        if include_name in WINDOWS_HEADER_NAMES:
            violations.append(
                f"{relative}: {source_layer} must not include Windows header {include.value}"
            )

    scan_text = strip_comments(text)
    for pattern in WINDOWS_TOKEN_PATTERNS:
        if re.search(pattern, scan_text):
            violations.append(
                f"{relative}: {source_layer} must not use Windows/native token {pattern}"
            )

    return violations


def validate_public_inner_api(path: Path, text: str) -> list[str]:
    violations: list[str] = []
    source_layer = layer_for_path(path)
    relative = rel(path)

    if source_layer not in INNER_LAYERS or not is_public_header(path):
        return violations

    scan_text = strip_comments(text)

    for include in includes_from(scan_text):
        normalized = include.value.replace("\\", "/").lower()
        for pattern in PUBLIC_INNER_FORBIDDEN_INCLUDE_PATTERNS:
            if re.search(pattern, normalized, re.IGNORECASE):
                violations.append(
                    f"{relative}: public {source_layer} header exposes forbidden include {include.value}"
                )

    for pattern in PUBLIC_INNER_FORBIDDEN_TOKEN_PATTERNS.get(source_layer, []):
        if re.search(pattern, scan_text):
            violations.append(
                f"{relative}: public {source_layer} header exposes suspicious outer-layer token {pattern}"
            )

    return violations


def validate_public_inner_api_warnings(path: Path, text: str) -> list[str]:
    warnings: list[str] = []
    source_layer = layer_for_path(path)
    relative = rel(path)

    if source_layer not in INNER_LAYERS or not is_public_header(path):
        return warnings

    scan_text = strip_comments(text)

    for pattern in PUBLIC_INNER_WARNING_TOKEN_PATTERNS.get(source_layer, []):
        if re.search(pattern, scan_text):
            warnings.append(
                f"{relative}: public {source_layer} header uses opaque handle token {pattern}; "
                "prefer a project-owned named handle typedef if practical"
            )

    return warnings


def validate_cmake_dependencies() -> list[str]:
    violations: list[str] = []

    for cmake_file in iter_cmake_files():
        text = read(cmake_file)
        relative = rel(cmake_file)

        for match in TARGET_LINK_LIBRARIES_RE.finditer(text):
            target = match.group(1)
            deps = cmake_tokens(match.group(2))
            target_arch_layer = target_layer(target)

            if target_arch_layer is None:
                continue

            allowed = ALLOWED_LAYER_DEPENDENCIES[target_arch_layer]

            for dep in deps:
                dep_layer = target_layer(dep)
                if dep_layer is None or dep_layer == target_arch_layer:
                    continue
                if dep_layer not in allowed:
                    violations.append(
                        f"{relative}: CMake target {target} ({target_arch_layer}) "
                        f"must not link {dep} ({dep_layer})"
                    )

    return violations


def main() -> int:
    violations: list[str] = []
    warnings: list[str] = []

    violations.extend(validate_layer_directories())
    violations.extend(validate_document_contract())
    violations.extend(validate_cmake_dependencies())

    for path in iter_source_files():
        text = read(path)
        violations.extend(validate_imports(path, text))
        violations.extend(validate_windows_boundary(path, text))
        violations.extend(validate_public_inner_api(path, text))
        warnings.extend(validate_public_inner_api_warnings(path, text))

    if violations:
        print(f"{RED}Architecture check failed:{RESET}")
        for violation in violations:
            print(f"{RED}  {violation}{RESET}")
        if warnings:
            print(f"{YELLOW}Architecture warnings:{RESET}")
            for warning in warnings:
                print(f"{YELLOW}  {warning}{RESET}")
        return 1

    if warnings:
        print(f"{YELLOW}Architecture warnings:{RESET}")
        for warning in warnings:
            print(f"{YELLOW}  {warning}{RESET}")

    print(f"{GREEN}Architecture check passed.{RESET}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
