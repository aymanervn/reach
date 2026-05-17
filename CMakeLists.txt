cmake_minimum_required(VERSION 3.20)

project(reach LANGUAGES C CXX)

add_executable(reach WIN32
    src/main.cpp
    src/app/composition_root.cpp
    src/shell.cpp
    src/core/ui_state.c
    src/core/ui_layout.c
    src/core/ui_events.c
    src/core/render_commands.c
    src/core/app_controller.c
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
    resources/reach.rc
    src/animation.cpp
    src/dock.cpp
    src/wm.cpp
    src/layout.cpp
    src/monitor.cpp
    src/hotkeys.cpp
    src/util.cpp
)

target_include_directories(reach PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(reach PRIVATE
    c_std_11
    cxx_std_17
)

target_compile_definitions(reach PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(reach PRIVATE
    d2d1
    dwrite
    dcomp
    dxgi
    user32
    shell32
    shlwapi
    shcore
    advapi32
    ole32
    windowscodecs
    dwmapi
)

add_executable(reachctl
    src/tools/reachctl.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/util.cpp
)

target_include_directories(reachctl PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(reachctl PRIVATE
    cxx_std_17
)

target_compile_definitions(reachctl PRIVATE
    UNICODE
    _UNICODE
    WIN32_LEAN_AND_MEAN
    NOMINMAX
)

target_link_libraries(reachctl PRIVATE
    shell32
    shlwapi
    advapi32
)

add_executable(reach_core_tests
    tests/core_ui_tests.c
    src/core/ui_state.c
    src/core/ui_layout.c
    src/core/ui_events.c
    src/core/render_commands.c
    src/core/app_controller.c
)

target_include_directories(reach_core_tests PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

target_compile_features(reach_core_tests PRIVATE
    c_std_11
)

target_compile_definitions(reach_core_tests PRIVATE
    UNICODE
    _UNICODE
)
