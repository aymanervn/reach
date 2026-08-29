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

    "core",
    "protocol",
    "ports",
    "services",
    "features",
    "adapters",
    "composition",
    "tools",
    "apps",

    "app",
    "shell",
    "support",
}

ALLOWED_INCLUDE_LAYER_DIRS = {

    "core",
    "protocol",
    "ports",
    "services",
    "features",
    "adapters",
    "composition",
    "tools",
    "apps",

    "app",
    "platform",
    "shell",
    "support",
}

ALLOWED_LAYER_DEPENDENCIES: dict[str, set[str]] = {

    "core": {"support"},
    "protocol": {"core", "support"},
    "ports": {"core", "protocol", "support"},
    "services": {"ports", "protocol", "core", "support"},
    "features": {"services", "ports", "protocol", "core", "support"},
    "adapters": {"ports", "protocol", "core", "support"},
    "composition": {
        "features",
        "services",
        "adapters",
        "ports",
        "protocol",
        "core",

        "shell",
        "app",
        "platform",
        "support",
    },
    "tools": {
        "composition",
        "adapters",
        "features",
        "services",
        "ports",
        "protocol",
        "core",

        "app",
        "shell",
        "platform",
        "support",
    },

    "apps": {
        "features",
        "services",
        "adapters",
        "ports",
        "protocol",
        "core",
        "support",
        "platform",
    },

    "support": set(),
    "shell": {"features", "services", "ports", "protocol", "core", "support"},
    "app": {
        "composition",
        "shell",
        "features",
        "services",
        "ports",
        "protocol",
        "core",
        "support",
        "platform",
        "adapters",
    },
    "platform": {"ports", "protocol", "core", "support"},
    "tests": {
        "composition",
        "app",
        "apps",
        "shell",
        "features",
        "services",
        "ports",
        "protocol",
        "core",
        "support",
        "platform",
        "adapters",
    },
}

INNER_LAYERS = {"support", "core", "protocol", "ports", "services", "features"}
OUTER_LAYERS = {
    "app",
    "apps",
    "platform",
    "shell",
    "adapters",
    "composition",
    "tools",
    "tests",
}
WINDOWS_ALLOWED_LAYERS = OUTER_LAYERS

EXACT_TARGET_LAYERS = {
    "reach_support": "support",
    "reach_core": "core",
    "reach_features": "features",
    "reach_shell": "shell",
    "reach_windows_adapters": "adapters",
    "reach_composition": "composition",
    "reach": "tools",
    "reach_settings": "apps",
    "reach_settings_feature": "apps",
    "reach_settings_feature_tests": "tests",
    "reachctl": "tools",
    "reach_elevation_helper": "tools",
    "reach_tray_probe": "tools",
}

TARGET_LAYER_PATTERNS: list[tuple[re.Pattern[str], str]] = [
    (re.compile(r"(^|::|_|-)(support)(_|-|$)", re.IGNORECASE), "support"),
    (
        re.compile(r"(^|::|_|-)(core|domain|entity|entities)(_|-|$)", re.IGNORECASE),
        "core",
    ),
    (
        re.compile(r"(^|::|_|-)(protocol|protocols|ipc)(_|-|$)", re.IGNORECASE),
        "protocol",
    ),
    (
        re.compile(r"(^|::|_|-)(port|ports|boundary|boundaries)(_|-|$)", re.IGNORECASE),
        "ports",
    ),
    (
        re.compile(r"(^|::|_|-)(service|services)(_|-|$)", re.IGNORECASE),
        "services",
    ),
    (
        re.compile(r"(^|::|_|-)(composition|compositionroot)(_|-|$)", re.IGNORECASE),
        "composition",
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

REGISTERED_FEATURE_LIFECYCLE_RE = re.compile(
    r"\breach_(dock|top_bar|launcher|clipboard_feature|quick_settings|battery|"
    r"system_hud|context_menu|switcher|stage)_(create|destroy)\s*\("
)

INTERFEATURE_ROUTES_SEAM = "src/composition/interfeature_routes.cpp"

INTERFEATURE_ROUTE_TARGETS: dict[str, str] = {
    "reach_host_show_dock_app_context_menu": (
        "src/composition/host_context_menu_orchestration.cpp"
    ),
    "reach_host_show_power_context_menu": "src/composition/host_context_menu_orchestration.cpp",
    "reach_host_dock_item_hovered": "src/composition/host_window_list_orchestration.cpp",
    "reach_host_toggle_stage": "src/composition/host_stage_orchestration.cpp",
}

FEATURE_ACTION_ENUM_RE = re.compile(r"REACH_[A-Z_]+_POINTER_ACTION_[A-Z_]+")

FEATURE_ACTION_POLICY_ALLOWED: set[str] = set()

GENERIC_SURFACE_LOOPS = (
    ("src/composition/host_lifecycle.cpp", "reach_host_create_with_dependencies", "lifecycle"),
    ("src/composition/host_surfaces.cpp", "reach_host_init_layout", "layout"),
    ("src/composition/host_update.cpp", "reach_host_finish_surface_transitions", "transition"),
    ("src/composition/host_input.cpp", "reach_host_pointer_order", "input"),
    ("src/composition/host_update.cpp", "reach_host_update", "frame"),
)

SURFACE_TABLE_LOOP_RE = re.compile(
    r"for\s*\([^;]*;[^;]*<\s*REACH_HOST_SURFACE_COUNT\s*;[^)]*\)"
)

MIGRATED_SURFACE_FRAME_RE = re.compile(
    r"\breach_host_(frame|render)_(dock|top_bar|launcher|context_menu|stage|"
    r"system_hud|switcher|clipboard)(?:_surface)?\b"
)

FEATURE_REGISTRY_SEAM = "src/composition/feature_registry.cpp"

COMPOSITION_FEATURE_SEAMS = {
    FEATURE_REGISTRY_SEAM,
    INTERFEATURE_ROUTES_SEAM,
}

CONCRETE_FEATURE_HEADER_BASELINE: dict[tuple[str, str], int] = {
    ("src/composition/host_internal.h", "reach/features/battery.h"): 1,
    ("src/composition/host_internal.h", "reach/features/clipboard.h"): 1,
    ("src/composition/host_internal.h", "reach/features/context_menu.h"): 1,
    ("src/composition/host_internal.h", "reach/features/dock.h"): 1,
    ("src/composition/host_internal.h", "reach/features/quick_settings.h"): 1,
    ("src/composition/host_internal.h", "reach/features/stage.h"): 1,
    ("src/composition/host_internal.h", "reach/features/switcher.h"): 1,
    ("src/composition/host_internal.h", "reach/features/system_hud.h"): 1,
    ("src/composition/host_internal.h", "reach/features/top_bar.h"): 1,
}

CONCRETE_FEATURE_SYMBOL_BASELINE: dict[tuple[str, str], int] = {
    ("src/composition/host_config.cpp", "reach_dock_mark_items_changed"): 1,
    ("src/composition/host_config.cpp", "reach_dock_order_count"): 1,
    ("src/composition/host_config.cpp", "reach_dock_order_key_at"): 1,
    ("src/composition/host_config.cpp", "reach_dock_restore_order"): 1,
    ("src/composition/host_config.cpp", "reach_stage_set_animation_seconds"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_context_menu_is_open"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_context_menu_open_for_item"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_context_menu_open_power"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_context_menu_reset"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_context_menu_state_ptr"): 3,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_dock_build_item_context_commands",
    ): 2,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_dock_collect_item_windows"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_dock_item_at"): 3,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_dock_item_count"): 2,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_dock_rect_to_screen"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_top_bar_rect_to_screen"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_top_bar_state_ptr"): 1,
    ("src/composition/host_dock_orchestration.cpp", "reach_dock_item_at"): 5,
    ("src/composition/host_dock_orchestration.cpp", "reach_dock_item_count"): 2,
    ("src/composition/host_feature_actions.cpp", "reach_dock_rebuild_items"): 1,
    ("src/composition/host_feedback.cpp", "reach_dock_clear_context_feedback"): 1,
    ("src/composition/host_game_mode.cpp", "reach_dock_clear_item_x_animations"): 1,
    ("src/composition/host_game_mode.cpp", "reach_switcher_force_close"): 1,
    ("src/composition/host_icons.cpp", "reach_dock_touch_icons"): 1,
    ("src/composition/host_popup.cpp", "reach_context_menu_state_ptr"): 1,
    ("src/composition/host_popup.cpp", "reach_dock_local_point"): 1,
    ("src/composition/host_popup.cpp", "reach_dock_pointer_region_at"): 1,
    ("src/composition/host_popup.cpp", "reach_top_bar_local_point"): 1,
    ("src/composition/host_popup.cpp", "reach_top_bar_pointer_region_at"): 1,
    ("src/composition/host_popup.cpp", "reach_top_bar_state_ptr"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_stage_begin_close"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_stage_is_open"): 4,
    ("src/composition/host_stage_orchestration.cpp", "reach_stage_open"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_stage_refresh_tile_frames"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_stage_update_windows"): 1,
    ("src/composition/host_update.cpp", "reach_dock_build_layout"): 1,
    ("src/composition/host_update.cpp", "reach_dock_rebuild_items"): 1,
    ("src/composition/host_update.cpp", "reach_dock_slots_animating"): 1,
    ("src/composition/host_update.cpp", "reach_dock_take_items_changed"): 1,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_context_menu_hover_region_contains",
    ): 1,
    ("src/composition/host_window_list_orchestration.cpp", "reach_context_menu_is_open"): 1,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_context_menu_open_window_list",
    ): 1,
    ("src/composition/host_window_list_orchestration.cpp", "reach_context_menu_state_ptr"): 1,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_context_menu_window_list_is_open",
    ): 4,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_context_menu_window_list_remove",
    ): 1,
    ("src/composition/host_window_list_orchestration.cpp", "reach_dock_collect_item_windows"): 1,
    ("src/composition/host_window_list_orchestration.cpp", "reach_dock_item_count"): 3,
    ("src/composition/host_window_list_orchestration.cpp", "reach_dock_rect_to_screen"): 2,
    ("src/composition/host_window_tracking.cpp", "reach_dock_mark_items_changed"): 1,
}

COMPOSITION_FEATURE_HELPER_BASELINE: dict[tuple[str, str], int] = {
    ("src/composition/host_bar_layout.cpp", "reach_host_dock_build_context"): 1,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_host_close_context_menu"): 7,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_context_menu_monitor",
    ): 3,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_dock_item_command_allowed",
    ): 2,
    ("src/composition/host_context_menu_orchestration.cpp", "reach_host_dock_item_path"): 1,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_launch_context_menu_item",
    ): 3,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_open_context_menu_transition",
    ): 3,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_show_dock_app_context_menu",
    ): 1,
    (
        "src/composition/host_context_menu_orchestration.cpp",
        "reach_host_show_power_context_menu",
    ): 1,
    ("src/composition/host_dock_orchestration.cpp", "reach_host_dock_item_path"): 2,
    ("src/composition/host_dock_orchestration.cpp", "reach_host_launch_dock_item"): 1,
    ("src/composition/host_feature_actions.cpp", "reach_host_dock_build_context"): 1,
    ("src/composition/host_feature_actions.cpp", "reach_host_launch_dock_item"): 1,
    ("src/composition/host_feedback.cpp", "reach_host_clear_sticky_dock_feedback"): 1,
    ("src/composition/host_input.cpp", "reach_host_clear_sticky_dock_feedback"): 1,
    ("src/composition/host_input.cpp", "reach_host_close_stage"): 1,
    ("src/composition/host_input.cpp", "reach_host_sync_stage_window_states"): 1,
    ("src/composition/host_internal.h", "reach_host_clear_sticky_dock_feedback"): 1,
    ("src/composition/host_internal.h", "reach_host_close_context_menu"): 1,
    ("src/composition/host_internal.h", "reach_host_close_stage"): 1,
    ("src/composition/host_internal.h", "reach_host_dock_build_context"): 1,
    ("src/composition/host_internal.h", "reach_host_dock_item_hovered"): 1,
    ("src/composition/host_internal.h", "reach_host_dock_item_path"): 1,
    ("src/composition/host_internal.h", "reach_host_launch_dock_item"): 1,
    ("src/composition/host_internal.h", "reach_host_on_stage_edge_reveal"): 1,
    ("src/composition/host_internal.h", "reach_host_open_context_menu_transition"): 1,
    ("src/composition/host_internal.h", "reach_host_open_stage"): 1,
    ("src/composition/host_internal.h", "reach_host_show_dock_app_context_menu"): 1,
    ("src/composition/host_internal.h", "reach_host_show_dock_window_list"): 1,
    ("src/composition/host_internal.h", "reach_host_show_power_context_menu"): 1,
    ("src/composition/host_internal.h", "reach_host_sync_stage_window_states"): 1,
    ("src/composition/host_internal.h", "reach_host_toggle_stage"): 1,
    ("src/composition/host_popup.cpp", "reach_host_clear_sticky_dock_feedback"): 2,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_close_stage"): 2,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_collect_stage_windows"): 4,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_on_stage_edge_reveal"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_open_stage"): 3,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_stage_monitor_for"): 3,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_stage_monitor_is_portrait"): 3,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_sync_stage_window_states"): 1,
    ("src/composition/host_stage_orchestration.cpp", "reach_host_toggle_stage"): 1,
    ("src/composition/host_surfaces.cpp", "reach_host_clear_sticky_dock_feedback"): 1,
    ("src/composition/host_update.cpp", "reach_host_dock_build_context"): 1,
    ("src/composition/host_update.cpp", "reach_host_sync_stage_window_states"): 1,
    ("src/composition/host_window_list_orchestration.cpp", "reach_host_close_context_menu"): 3,
    ("src/composition/host_window_list_orchestration.cpp", "reach_host_dock_item_hovered"): 1,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_host_open_context_menu_transition",
    ): 1,
    (
        "src/composition/host_window_list_orchestration.cpp",
        "reach_host_show_dock_window_list",
    ): 3,
}

REGISTERED_SURFACE_IDS = (
    "REACH_SURFACE_ID_DOCK",
    "REACH_SURFACE_ID_TOP_BAR",
    "REACH_SURFACE_ID_LAUNCHER",
    "REACH_SURFACE_ID_CLIPBOARD",
    "REACH_SURFACE_ID_TRAY",
    "REACH_SURFACE_ID_QUICK_SETTINGS",
    "REACH_SURFACE_ID_BATTERY",
    "REACH_SURFACE_ID_SYSTEM_HUD",
    "REACH_SURFACE_ID_CONTEXT_MENU",
    "REACH_SURFACE_ID_SWITCHER",
    "REACH_SURFACE_ID_STAGE",
)

REGISTERED_CAPSULE_OPS_RE = re.compile(
    r"\breach_(dock|top_bar|launcher|clipboard_feature|quick_settings|battery|"
    r"system_hud|context_menu|switcher|stage)_(?:tray_)?capsule_ops\s*\("
)

LEGACY_FEATURE_RUNTIME_RE = re.compile(
    r"\b(reach_surface_desc|surface_descs|frame_priority)\b"
)

FEATURE_RUNTIME_STRUCT_RE = re.compile(
    r"typedef\s+struct\s+reach_feature_runtime\s*\{(?P<body>.*?)"
    r"\}\s*reach_feature_runtime\s*;",
    re.DOTALL,
)

TYPED_CAPSULE_ALIAS_RE = re.compile(
    r"\b(dock|top_bar|launcher|clipboard|quick_settings|battery|system_hud|"
    r"context_menu|switcher|stage)_capsule\s*;"
)

IMMUTABLE_RUNTIME_FIELDS = (
    "id",
    "capsule_ops",
    "surface_ops",
    "force_close",
    "pointer_flags",
    "shadow",
    "behavior_flags",
    "layer",
    "role",
    "pointer_priority",
    "dismiss",
    "layout_anchor",
    "frame_priority",
    "toggle_events",
    "routed_events",
    "frame",
)

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

    resolved = resolve_local_include(source, include.value)
    if resolved is not None:
        return layer_for_path(resolved)

    return None

def includes_from(text: str) -> list[Include]:
    return [Include(delimiter, value) for delimiter, value in INCLUDE_RE.findall(text)]


def concrete_feature_headers() -> dict[str, str]:
    source_root = ROOT / "src" / "features"
    include_root = ROOT / "include" / "reach" / "features"
    if not source_root.exists() or not include_root.exists():
        return {}

    headers: dict[str, str] = {}
    for feature_dir in source_root.iterdir():
        if not feature_dir.is_dir() or feature_dir.name == "common":
            continue
        header = include_root / f"{feature_dir.name}.h"
        if header.exists():
            headers[f"reach/features/{header.name}"] = feature_dir.name
    return headers


def concrete_feature_symbols(headers: dict[str, str]) -> dict[str, str]:
    symbols: dict[str, str] = {}
    for include, feature in headers.items():
        header = ROOT / "include" / include
        for symbol in re.findall(r"\b(reach_[a-z0-9_]+)\s*\(", strip_comments(read(header))):
            symbols[symbol] = feature
    return symbols

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
        "## core",
        "## protocol",
        "## ports",
        "## services",
        "## features",
        "## adapters",
        "## composition",
        "## tools",
        "inward",
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

def validate_capsule_state_encapsulation(path: Path, text: str) -> list[str]:
    """Feature capsule state is compiler-enforced const outside the feature:
    the internal mutable accessors (reach_<f>_state_mut) must never appear
    outside src/features/. A hit means someone re-opened the hole the
    class-driven capsule interface closed."""
    violations: list[str] = []
    relative = rel(path)
    if relative.replace("\\", "/").startswith("src/features/"):
        return violations
    scan_text = strip_comments(text)
    if re.search(r"\b\w+_state_mut\s*\(", scan_text):
        violations.append(
            f"{relative}: capsule state must stay const outside src/features/ "
            f"(found a *_state_mut( accessor use)"
        )
    return violations

def feature_owner_for_path(path: Path, headers: dict[str, str]) -> str | None:
    relative = rel(path).replace("\\", "/")
    if relative.startswith("src/features/"):
        parts = relative.split("/")
        if len(parts) >= 3 and parts[2] != "common":
            return parts[2]
    if relative.startswith("include/reach/features/"):
        include = relative.removeprefix("include/")
        return headers.get(include)
    return None


def validate_feature_peer_dependencies(
    path: Path, text: str, headers: dict[str, str], symbols: dict[str, str]
) -> list[str]:
    owner = feature_owner_for_path(path, headers)
    if owner is None:
        return []

    relative = rel(path).replace("\\", "/")
    violations: list[str] = []
    for include in includes_from(text):
        peer = headers.get(include.value.replace("\\", "/"))
        if peer is not None and peer != owner:
            violations.append(
                f"{relative}: feature {owner} must not include peer feature {peer} "
                f"through {include.value}; use a features/common contract or a routed slot"
            )

    scan_text = strip_comments(text)
    for symbol, peer in symbols.items():
        if peer != owner and re.search(rf"\b{re.escape(symbol)}\s*\(", scan_text):
            violations.append(
                f"{relative}: feature {owner} must not call peer feature {peer} symbol {symbol}"
            )
    return violations


def validate_composition_feature_boundary(
    path: Path, text: str, headers: dict[str, str], symbols: dict[str, str]
) -> list[str]:
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/composition/") or relative in COMPOSITION_FEATURE_SEAMS:
        return []

    violations: list[str] = []
    include_counts: dict[str, int] = {}
    for include in includes_from(text):
        normalized = include.value.replace("\\", "/")
        if normalized in headers:
            include_counts[normalized] = include_counts.get(normalized, 0) + 1
    for include, count in include_counts.items():
        baseline = CONCRETE_FEATURE_HEADER_BASELINE.get((relative, include), 0)
        if count > baseline:
            violations.append(
                f"{relative}: concrete feature header {include} is allowed only in the "
                f"composition seams (found {count}, baseline {baseline})"
            )
    for (baseline_file, include), baseline in CONCRETE_FEATURE_HEADER_BASELINE.items():
        if baseline_file == relative and include_counts.get(include, 0) < baseline:
            violations.append(
                f"{relative}: concrete feature header baseline for {include} is stale; "
                f"shrink it from {baseline} to {include_counts.get(include, 0)}"
            )

    scan_text = strip_comments(text)
    symbol_counts: dict[str, int] = {}
    for symbol in symbols:
        count = len(re.findall(rf"\b{re.escape(symbol)}\s*\(", scan_text))
        if count == 0:
            continue
        symbol_counts[symbol] = count
        baseline = CONCRETE_FEATURE_SYMBOL_BASELINE.get((relative, symbol), 0)
        if count > baseline:
            violations.append(
                f"{relative}: concrete feature symbol {symbol} is allowed only in the "
                f"composition seams (found {count}, baseline {baseline})"
            )
    for (baseline_file, symbol), baseline in CONCRETE_FEATURE_SYMBOL_BASELINE.items():
        if baseline_file == relative and symbol_counts.get(symbol, 0) < baseline:
            violations.append(
                f"{relative}: concrete feature symbol baseline for {symbol} is stale; "
                f"shrink it from {baseline} to {symbol_counts.get(symbol, 0)}"
            )
    return violations


def composition_feature_helper_pattern(headers: dict[str, str]) -> re.Pattern[str] | None:
    features = sorted(set(headers.values()), key=len, reverse=True)
    if not features:
        return None
    return re.compile(
        r"\breach_host_[a-z0-9_]*(?:" + "|".join(features) + r")[a-z0-9_]*\b"
    )


def validate_composition_feature_helpers(
    path: Path, text: str, pattern: re.Pattern[str] | None
) -> list[str]:
    """Composition v0 wants generic composition code. A reach_host helper named after a
    concrete feature is per-feature orchestration wearing a host prefix, so the remaining
    ones are an explicit baseline that may only shrink."""
    relative = rel(path).replace("\\", "/")
    if (
        pattern is None
        or not relative.startswith("src/composition/")
        or relative in COMPOSITION_FEATURE_SEAMS
    ):
        return []

    violations: list[str] = []
    counts: dict[str, int] = {}
    for symbol in pattern.findall(strip_comments(text)):
        counts[symbol] = counts.get(symbol, 0) + 1
    for symbol, count in counts.items():
        baseline = COMPOSITION_FEATURE_HELPER_BASELINE.get((relative, symbol), 0)
        if count > baseline:
            violations.append(
                f"{relative}: {symbol} is feature-specific composition; move it behind a "
                f"generic contract (found {count}, baseline {baseline})"
            )
    for (baseline_file, symbol), baseline in COMPOSITION_FEATURE_HELPER_BASELINE.items():
        if baseline_file == relative and counts.get(symbol, 0) < baseline:
            violations.append(
                f"{relative}: feature-specific composition baseline for {symbol} is stale; "
                f"shrink it from {baseline} to {counts.get(symbol, 0)}"
            )
    return violations


def validate_composition_feature_baseline_files(paths: list[Path]) -> list[str]:
    existing = {rel(path).replace("\\", "/") for path in paths}
    baseline_files = {
        path for path, _ in CONCRETE_FEATURE_HEADER_BASELINE
    } | {
        path for path, _ in CONCRETE_FEATURE_SYMBOL_BASELINE
    } | {
        path for path, _ in COMPOSITION_FEATURE_HELPER_BASELINE
    }
    return [
        f"{path}: concrete feature baseline names a missing file; remove its entries"
        for path in sorted(baseline_files - existing)
    ]


def validate_feature_config_ownership(path: Path, text: str) -> list[str]:
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/features/") and not relative.startswith(
        "include/reach/features/"
    ):
        return []
    if "config_store" not in strip_comments(text):
        return []
    return [
        f"{relative}: features must consume reach_config_service snapshots, not config_store"
    ]


def validate_registered_feature_lifecycle(path: Path, text: str) -> list[str]:
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/composition/") or relative == (
        "src/composition/feature_registry.cpp"
    ):
        return []
    if REGISTERED_FEATURE_LIFECYCLE_RE.search(strip_comments(text)) is None:
        return []
    return [
        f"{relative}: registered feature create/destroy calls belong only in "
        "feature_registry.cpp"
    ]


def validate_interfeature_routes(path: Path, text: str) -> list[str]:
    """An effect that opens or changes another feature may only be reached from the
    interfeature routing seam, from the file that defines it, or from the shared
    composition declarations."""
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/composition/") or relative in (
        INTERFEATURE_ROUTES_SEAM,
        "src/composition/host_internal.h",
    ):
        return []

    scan_text = strip_comments(text)
    violations: list[str] = []
    for symbol, owner in INTERFEATURE_ROUTE_TARGETS.items():
        if relative == owner:
            continue
        if re.search(rf"\b{symbol}\s*\(", scan_text) is not None:
            violations.append(
                f"{relative}: {symbol} is an interfeature effect; route it from "
                f"{INTERFEATURE_ROUTES_SEAM}"
            )
    return violations


def validate_feature_action_vocabulary(path: Path, text: str) -> list[str]:
    """Composition dispatches the shared reach_feature_action_kind vocabulary. A
    feature-private action enum outside the input-policy file means a translator
    came back."""
    relative = rel(path).replace("\\", "/")
    if (
        not relative.startswith("src/composition/")
        or relative in FEATURE_ACTION_POLICY_ALLOWED
    ):
        return []
    match = FEATURE_ACTION_ENUM_RE.search(strip_comments(text))
    if match is None:
        return []
    return [
        f"{relative}: {match.group(0)} is a feature-private action kind; dispatch the "
        "shared reach_feature_action_kind vocabulary instead"
    ]


def function_body(text: str, name: str) -> str | None:
    match = re.search(rf"^[A-Za-z_][A-Za-z0-9_ *]*\b{name}\s*\(", text, re.MULTILINE)
    if match is None:
        return None
    open_brace = text.find("{", match.end())
    if open_brace < 0:
        return None
    depth = 0
    for index in range(open_brace, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace : index + 1]
    return None


def validate_generic_surface_loops() -> list[str]:
    """Every registered surface joins lifecycle, layout, transition, input, and frame
    work by being in the runtime table. Each loop must iterate the whole table, so a
    hand-maintained feature list cannot come back."""
    violations: list[str] = []
    for relative, function, loop in GENERIC_SURFACE_LOOPS:
        text = strip_comments(read(ROOT / relative))
        body = function_body(text, function)
        if body is None:
            violations.append(f"{relative}: missing the generic {loop} entry point {function}")
            continue
        if SURFACE_TABLE_LOOP_RE.search(body) is None:
            violations.append(
                f"{relative}: {function} must iterate the whole runtime table so every "
                f"registered surface joins the {loop} loop automatically"
            )
    return violations


def validate_migrated_surface_frames(path: Path, text: str) -> list[str]:
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/composition/"):
        return []
    if MIGRATED_SURFACE_FRAME_RE.search(strip_comments(text)) is None:
        return []
    return [
        f"{relative}: migrated features must use the registered generic surface frame operations"
    ]


def validate_uniform_feature_runtime(path: Path, text: str) -> list[str]:
    relative = rel(path).replace("\\", "/")
    if not relative.startswith("src/composition/"):
        return []

    scan_text = strip_comments(text)
    violations: list[str] = []
    legacy = LEGACY_FEATURE_RUNTIME_RE.search(scan_text)
    if legacy is not None:
        violations.append(
            f"{relative}: {legacy.group(0)} is obsolete; use the canonical feature "
            "definition and runtime"
        )
    if (
        relative != FEATURE_REGISTRY_SEAM
        and REGISTERED_CAPSULE_OPS_RE.search(scan_text) is not None
    ):
        violations.append(
            f"{relative}: registered capsule operations belong only in "
            f"{FEATURE_REGISTRY_SEAM}"
        )
    return violations


def validate_feature_registry_contract() -> list[str]:
    violations: list[str] = []
    registry_text = strip_comments(read(ROOT / FEATURE_REGISTRY_SEAM))
    host_internal_text = strip_comments(
        read(ROOT / "src" / "composition" / "host_internal.h")
    )

    runtime_match = FEATURE_RUNTIME_STRUCT_RE.search(host_internal_text)
    if runtime_match is None:
        violations.append(
            "src/composition/host_internal.h: missing reach_feature_runtime declaration"
        )
    else:
        runtime_body = runtime_match.group("body")
        for field in IMMUTABLE_RUNTIME_FIELDS:
            if re.search(rf"\b{field}\s*(?:\[|;)", runtime_body) is not None:
                violations.append(
                    "src/composition/host_internal.h: immutable feature policy field "
                    f"{field} belongs in reach_feature_definition"
                )

    capsule_alias = TYPED_CAPSULE_ALIAS_RE.search(host_internal_text)
    if capsule_alias is not None:
        violations.append(
            "src/composition/host_internal.h: typed capsule alias "
            f"{capsule_alias.group(0)[:-1]} duplicates reach_feature_runtime"
        )

    for surface_id in REGISTERED_SURFACE_IDS:
        definition_count = len(
            re.findall(
                rf"\breach_host_define_feature\s*\(\s*host\s*,\s*{surface_id}\b",
                registry_text,
            )
        )
        if definition_count != 1:
            violations.append(
                f"{FEATURE_REGISTRY_SEAM}: {surface_id} must have exactly one runtime binding"
            )
        if (
            re.search(rf"\[\s*{surface_id}\s*\]\.surface_ops\s*=", registry_text)
            is None
        ):
            violations.append(
                f"{FEATURE_REGISTRY_SEAM}: {surface_id} must declare uniform surface_ops"
            )

    host_update_text = strip_comments(
        read(ROOT / "src" / "composition" / "host_update.cpp")
    )
    if re.search(r"(?:->|\.)frame\s*\(", host_update_text) is not None:
        violations.append(
            "src/composition/host_update.cpp: registered surfaces must not use a named frame fallback"
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
    feature_headers = concrete_feature_headers()
    feature_symbols = concrete_feature_symbols(feature_headers)
    feature_helper_pattern = composition_feature_helper_pattern(feature_headers)
    source_files = iter_source_files()
    violations.extend(validate_layer_directories())
    violations.extend(validate_document_contract())
    violations.extend(validate_cmake_dependencies())
    violations.extend(validate_feature_registry_contract())
    violations.extend(validate_generic_surface_loops())
    violations.extend(validate_composition_feature_baseline_files(source_files))

    for path in source_files:
        text = read(path)
        violations.extend(validate_imports(path, text))
        violations.extend(validate_windows_boundary(path, text))
        violations.extend(validate_public_inner_api(path, text))
        violations.extend(validate_capsule_state_encapsulation(path, text))
        violations.extend(
            validate_feature_peer_dependencies(path, text, feature_headers, feature_symbols)
        )
        violations.extend(
            validate_composition_feature_boundary(path, text, feature_headers, feature_symbols)
        )
        violations.extend(
            validate_composition_feature_helpers(path, text, feature_helper_pattern)
        )
        violations.extend(validate_feature_config_ownership(path, text))
        violations.extend(validate_registered_feature_lifecycle(path, text))
        violations.extend(validate_migrated_surface_frames(path, text))
        violations.extend(validate_uniform_feature_runtime(path, text))
        violations.extend(validate_interfeature_routes(path, text))
        violations.extend(validate_feature_action_vocabulary(path, text))
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
