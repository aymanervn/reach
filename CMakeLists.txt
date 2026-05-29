cmake_minimum_required(VERSION 3.20)

project(reach VERSION 0.1.0 LANGUAGES C CXX)

enable_testing()
set(REACH_CTEST_DEFAULT_CONFIG ${CMAKE_CURRENT_BINARY_DIR}/CTestDefaultConfig.cmake)
file(WRITE ${REACH_CTEST_DEFAULT_CONFIG} "if(NOT CTEST_CONFIGURATION_TYPE)\n    set(CTEST_CONFIGURATION_TYPE Debug)\nendif()\n")
set_property(DIRECTORY PROPERTY TEST_INCLUDE_FILE ${REACH_CTEST_DEFAULT_CONFIG})

find_package(Python3 REQUIRED COMPONENTS Interpreter)

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
    src/support/util.cpp
    src/support/animation.cpp
    src/support/layout.cpp
    src/support/search_ranking.cpp
)

reach_configure_common(reach_support)
target_compile_features(reach_support PUBLIC
    cxx_std_17
)

add_library(reach_core STATIC
    src/core/ui_state.c
    src/core/ui_layout.c
    src/core/ui_events.c
    src/core/render_commands.c
    src/core/app_controller.c
    src/core/runtime_policy.c
    src/core/theme.c
)

reach_configure_common(reach_core)
target_compile_features(reach_core PUBLIC
    c_std_11
)

add_library(reach_features STATIC
    src/features/context_menu/context_menu_feature.cpp
    src/features/context_menu/context_menu_interaction.cpp
    src/features/context_menu/context_menu_render.cpp
    src/features/dock/dock_feature.cpp
    src/features/dock/dock_icons.cpp
    src/features/dock/dock_interaction.cpp
    src/features/dock/dock_model.cpp
    src/features/dock/dock_render.cpp
    src/features/launcher/launcher_feature.cpp
    src/features/launcher/launcher_interaction.cpp
    src/features/launcher/launcher_render.cpp
    src/features/popup/popup_render.cpp
    src/features/quick_settings/quick_settings_feature.cpp
    src/features/quick_settings/quick_settings_interaction.cpp
    src/features/quick_settings/quick_settings_render.cpp
    src/features/switcher/switcher_feature.cpp
    src/features/switcher/switcher_render.cpp
    src/features/tray/tray_feature.cpp
    src/features/tray/tray_interaction.cpp
    src/features/tray/tray_model.cpp
    src/features/tray/tray_render.cpp
    src/features/wallpaper/wallpaper_feature.cpp
    src/features/pin_config.cpp
)

reach_configure_common(reach_features)
target_compile_features(reach_features PUBLIC
    cxx_std_17
)
target_link_libraries(reach_features PUBLIC
    reach_core
    reach_support
)

add_library(reach_windows_adapters STATIC
    src/adapters/windows/window_win32.cpp
    src/adapters/windows/render_d2d/render_d2d_backend.cpp
    src/adapters/windows/render_d2d/render_d2d_device.cpp
    src/adapters/windows/render_d2d/render_d2d_dcomp.cpp
    src/adapters/windows/render_d2d/render_d2d_wuc.cpp
    src/adapters/windows/render_d2d/render_d2d_frame.cpp
    src/adapters/windows/render_d2d/render_d2d_icons.cpp
    src/adapters/windows/render_d2d/render_d2d_svg.cpp
    src/adapters/windows/render_d2d/render_d2d_shapes.cpp
    src/adapters/windows/render_d2d/render_d2d_text.cpp
    src/adapters/windows/render_d2d/render_d2d_commands.cpp
    src/adapters/windows/search_everything_win32.cpp
    src/adapters/windows/app_launcher_win32.cpp
    src/adapters/windows/tray_win32.cpp
    src/adapters/windows/icon_shell.cpp
    src/adapters/windows/windows_icon_handle.cpp
    src/adapters/windows/input_win32.cpp
    src/adapters/windows/config_ini.cpp
    src/adapters/windows/config_path_win32.cpp
    src/adapters/windows/window_manager_win32.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/adapters/windows/explorer_service_win32.cpp
    src/adapters/windows/wallpaper_win32.cpp
    src/adapters/windows/wallpaper_surface_win32.cpp
    src/adapters/windows/popup_capture_win32.cpp
    src/adapters/windows/power_session_win32.cpp
    src/adapters/windows/audio_volume_win32.cpp
    src/adapters/windows/system_controls_win32.cpp
    src/adapters/windows/dock_reveal_edge_win32.cpp
    src/adapters/windows/monitor_win32.cpp
    src/adapters/windows/hotkeys_win32.cpp
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
    powrprof
    wlanapi
    iphlpapi
    wbemuuid
)

add_library(reach_shell STATIC
    src/shell/shell_icons.cpp
    src/shell/shell_input.cpp
    src/shell/shell_game_mode.cpp
    src/shell/shell_launcher_search.cpp
    src/shell/shell_lifecycle.cpp
    src/shell/shell_popup_surface.cpp
    src/shell/shell_quick_settings.cpp
    src/shell/shell_render.cpp
    src/shell/shell_surfaces.cpp
    src/shell/shell_update.cpp
    src/shell/surface_runtime.cpp
)

reach_configure_common(reach_shell)
target_compile_features(reach_shell PUBLIC
    c_std_11
    cxx_std_17
)
target_link_libraries(reach_shell PUBLIC
    reach_core
    reach_features
    reach_support
)
add_executable(reach WIN32
    src/app/main.cpp
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
    powrprof
    wlanapi
    iphlpapi
    wbemuuid
)

set(EVERYTHING_DLL "${CMAKE_CURRENT_SOURCE_DIR}/third_party/everything/dll/Everything64.dll")

set(REACH_RELEASE_IMAGE "${CMAKE_CURRENT_SOURCE_DIR}/resources/reach.jpg")

add_custom_command(TARGET reach POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EVERYTHING_DLL}"
        "$<TARGET_FILE_DIR:reach>/Everything64.dll"
    COMMENT "Copying Everything64.dll next to reach.exe"
)

add_executable(reachctl
    src/tools/reachctl/reachctl.cpp
    src/tools/reachctl/reachctl_common.cpp
    src/tools/reachctl/reachctl_session_commands.cpp
    src/tools/reachctl/reachctl_config_commands.cpp
    src/tools/reachctl/reachctl_context_menu.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/adapters/windows/config_ini.cpp
    src/adapters/windows/config_path_win32.cpp
    src/adapters/windows/wallpaper_win32.cpp
    src/support/util.cpp
)

reach_configure_common(reachctl)
reach_configure_windows(reachctl)
target_compile_features(reachctl PRIVATE
    cxx_std_17
)
target_link_libraries(reachctl PRIVATE
    reach_features
    shell32
    shlwapi
    user32
    advapi32
    ole32
    uuid
)

add_executable(reach-watchdog WIN32
    src/tools/reach_watchdog.cpp
)

reach_configure_common(reach-watchdog)
reach_configure_windows(reach-watchdog)
target_compile_features(reach-watchdog PRIVATE
    cxx_std_17
)
target_link_libraries(reach-watchdog PRIVATE
    shell32
    shlwapi
    user32
    advapi32
)
set(REACH_DIST_ROOT "${CMAKE_BINARY_DIR}/dist")
set(REACH_DIST_DIR "${REACH_DIST_ROOT}/Release")
set(REACH_ZIP_PATH "${REACH_DIST_ROOT}/reach-v${PROJECT_VERSION}-alpha.zip")

add_custom_target(reach_release_zip
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${REACH_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${REACH_DIST_DIR}"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:reach>"
        "${REACH_DIST_DIR}/reach.exe"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:reachctl>"
        "${REACH_DIST_DIR}/reachctl.exe"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:reach-watchdog>"
        "${REACH_DIST_DIR}/reach-watchdog.exe"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${EVERYTHING_DLL}"
        "${REACH_DIST_DIR}/Everything64.dll"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${REACH_RELEASE_IMAGE}"
        "${REACH_DIST_DIR}/reach.jpg"

    COMMAND ${CMAKE_COMMAND} -E chdir "${REACH_DIST_DIR}"
        ${CMAKE_COMMAND} -E tar "cfv" "${REACH_ZIP_PATH}" --format=zip
        reach.exe
        reachctl.exe
        reach-watchdog.exe
        Everything64.dll
        reach.jpg

    DEPENDS reach reachctl reach-watchdog

    COMMENT "Creating Reach Release ZIP"
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

add_executable(reach_dock_feature_tests
    tests/dock_feature_tests.cpp
)

reach_configure_common(reach_dock_feature_tests)
target_compile_features(reach_dock_feature_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_dock_feature_tests PRIVATE
    reach_features
    reach_support
)

add_executable(reach_pin_config_tests
    tests/pin_config_tests.cpp
)

reach_configure_common(reach_pin_config_tests)
target_compile_features(reach_pin_config_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_pin_config_tests PRIVATE
    reach_features
)

add_executable(reach_quick_settings_feature_tests
    tests/quick_settings_feature_tests.cpp
)

reach_configure_common(reach_quick_settings_feature_tests)
target_compile_features(reach_quick_settings_feature_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_quick_settings_feature_tests PRIVATE
    reach_features
)

add_executable(reach_launcher_feature_tests
    tests/launcher_feature_tests.cpp
)

reach_configure_common(reach_launcher_feature_tests)
target_compile_features(reach_launcher_feature_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_launcher_feature_tests PRIVATE
    reach_features
    reach_support
)

add_executable(reach_search_ranking_tests
    tests/search_ranking_tests.cpp
)

reach_configure_common(reach_search_ranking_tests)
target_compile_features(reach_search_ranking_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_search_ranking_tests PRIVATE
    reach_support
)

if(CMAKE_CONFIGURATION_TYPES)
    add_test(NAME reach_core_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_core_tests.exe)
    add_test(NAME reach_dock_feature_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_dock_feature_tests.exe)
    add_test(NAME reach_pin_config_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_pin_config_tests.exe)
    add_test(NAME reach_quick_settings_feature_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_quick_settings_feature_tests.exe)
    add_test(NAME reach_launcher_feature_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_launcher_feature_tests.exe)
    add_test(NAME reach_search_ranking_tests COMMAND ${CMAKE_BINARY_DIR}/Debug/reach_search_ranking_tests.exe)
else()
    add_test(NAME reach_core_tests COMMAND reach_core_tests)
    add_test(NAME reach_dock_feature_tests COMMAND reach_dock_feature_tests)
    add_test(NAME reach_pin_config_tests COMMAND reach_pin_config_tests)
    add_test(NAME reach_quick_settings_feature_tests COMMAND reach_quick_settings_feature_tests)
    add_test(NAME reach_launcher_feature_tests COMMAND reach_launcher_feature_tests)
    add_test(NAME reach_search_ranking_tests COMMAND reach_search_ranking_tests)
endif()

add_custom_target(check_architecture ALL
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_architecture.py
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    COMMENT "Checking Reach architecture boundaries"
)
