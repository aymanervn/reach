cmake_minimum_required(VERSION 3.20)

project(reach LANGUAGES C CXX)

set(REACH_INCLUDE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/include)

function(reach_configure_common target)
    target_include_directories(${target} PUBLIC
        ${REACH_INCLUDE_DIR}
    )
    target_compile_definitions(${target} PUBLIC
        UNICODE
        _UNICODE
    )
endfunction()

function(reach_configure_windows target)
    target_compile_definitions(${target} PUBLIC
        WIN32_LEAN_AND_MEAN
        NOMINMAX
    )
endfunction()

add_library(reach_support STATIC
    src/util.cpp
    src/config_path.cpp
)

reach_configure_common(reach_support)
reach_configure_windows(reach_support)
target_compile_features(reach_support PUBLIC
    cxx_std_17
)

target_link_libraries(reach_support PUBLIC
    shlwapi
    user32
)

add_library(reach_core STATIC
    src/core/ui_state.c
    src/core/ui_layout.c
    src/core/ui_events.c
    src/core/render_commands.c
    src/core/app_controller.c
)

reach_configure_common(reach_core)
target_compile_features(reach_core PUBLIC
    c_std_11
)

add_library(reach_features STATIC
    src/features/dock/dock_feature.cpp
    src/features/dock/dock_icons.cpp
    src/features/dock/dock_model.cpp
    src/features/dock/dock_render.cpp
)

reach_configure_common(reach_features)
target_compile_features(reach_features PUBLIC
    cxx_std_17
)
target_link_libraries(reach_features PUBLIC
    reach_core
)

add_library(reach_windows_adapters STATIC
    src/adapters/windows/window_win32.cpp
    src/adapters/windows/render_d2d.cpp
    src/adapters/windows/search_stub.cpp
    src/adapters/windows/app_launcher_win32.cpp
    src/adapters/windows/tray_win32.cpp
    src/adapters/windows/icon_shell.cpp
    src/adapters/windows/input_win32.cpp
    src/adapters/windows/config_ini.cpp
    src/adapters/windows/window_manager_win32.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/adapters/windows/explorer_service_win32.cpp
    src/adapters/windows/wallpaper_win32.cpp
    src/adapters/windows/wallpaper_surface_win32.cpp
)

reach_configure_common(reach_windows_adapters)
reach_configure_windows(reach_windows_adapters)
target_compile_features(reach_windows_adapters PUBLIC
    cxx_std_17
)
target_link_libraries(reach_windows_adapters PUBLIC
    reach_support
)
target_link_libraries(reach_windows_adapters PRIVATE
    d2d1
    dwrite
    dcomp
    d3d11
    dxgi
    user32
    shell32
    comctl32
    shlwapi
    shcore
    advapi32
    ole32
    runtimeobject
    uuid
    windowscodecs
    dwmapi
)

add_library(reach_shell STATIC
    src/shell/shell.cpp
    src/shell/surface_runtime.cpp
    src/pin_config.cpp
    src/theme.c
    src/animation.cpp
    src/layout.cpp
    src/monitor.cpp
    src/hotkeys.cpp
)

reach_configure_common(reach_shell)
reach_configure_windows(reach_shell)
target_compile_features(reach_shell PUBLIC
    c_std_11
    cxx_std_17
)
target_link_libraries(reach_shell PUBLIC
    reach_core
    reach_features
    reach_support
)
target_link_libraries(reach_shell PRIVATE
    dwrite
    user32
    shlwapi
    shcore
    dwmapi
)

add_executable(reach WIN32
    src/main.cpp
    src/app/composition_root.cpp
    resources/reach.rc
)

reach_configure_common(reach)
reach_configure_windows(reach)
target_compile_features(reach PRIVATE
    c_std_11
    cxx_std_17
)
target_link_libraries(reach PRIVATE
    reach_shell
    reach_core
    reach_features
    reach_support
    reach_windows_adapters
    d2d1
    dwrite
    dcomp
    d3d11
    dxgi
    user32
    shell32
    comctl32
    shlwapi
    shcore
    advapi32
    ole32
    runtimeobject
    uuid
    windowscodecs
    dwmapi
)

add_executable(reachctl
    src/tools/reachctl.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/adapters/windows/config_ini.cpp
    src/adapters/windows/wallpaper_win32.cpp
    src/config_path.cpp
    src/pin_config.cpp
    src/util.cpp
)

reach_configure_common(reachctl)
reach_configure_windows(reachctl)
target_compile_features(reachctl PRIVATE
    cxx_std_17
)
target_link_libraries(reachctl PRIVATE
    shell32
    shlwapi
    user32
    advapi32
    ole32
    uuid
)

add_executable(reach_tray_probe
    src/tools/tray_probe.cpp
)

reach_configure_common(reach_tray_probe)
reach_configure_windows(reach_tray_probe)
target_compile_features(reach_tray_probe PRIVATE
    cxx_std_17
)
target_link_libraries(reach_tray_probe PRIVATE
    shell32
    user32
)

add_executable(reach_core_tests
    tests/core_ui_tests.c
)

reach_configure_common(reach_core_tests)
target_compile_features(reach_core_tests PRIVATE
    c_std_11
)
target_link_libraries(reach_core_tests PRIVATE
    reach_core
)

add_executable(reach_pin_config_tests
    tests/pin_config_tests.cpp
    src/pin_config.cpp
    src/util.cpp
)

reach_configure_common(reach_pin_config_tests)
reach_configure_windows(reach_pin_config_tests)
target_compile_features(reach_pin_config_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_pin_config_tests PRIVATE
    shlwapi
    user32
)
