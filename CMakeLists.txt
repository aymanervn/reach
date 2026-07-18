cmake_minimum_required(VERSION 3.20)

project(reach VERSION 1.0  LANGUAGES C CXX)

option(REACH_BUILD_TESTS "Build Reach test executables and register CTest tests" ON)
option(REACH_RUN_ARCHITECTURE_CHECK "Run architecture boundary checks as part of the default build" ON)

if(REACH_BUILD_TESTS)
    enable_testing()
endif()

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

add_library(reach_protocol STATIC
    src/protocol/reach_service_protocol.cpp
)

reach_configure_common(reach_protocol)
target_compile_features(reach_protocol PUBLIC
    cxx_std_17
)
target_link_libraries(reach_protocol PUBLIC
    reach_support
)

add_library(reach_services STATIC
    src/services/app_control.cpp
    src/services/config.cpp
    src/services/icon_service.cpp
    src/services/idle_watch.cpp
    src/services/now_playing.cpp
    src/services/pin_config.cpp
    src/services/search.cpp
    src/services/system_status.cpp
    src/services/window_tracking.cpp
)

reach_configure_common(reach_services)
target_compile_features(reach_services PUBLIC
    cxx_std_17
)
target_link_libraries(reach_services PUBLIC
    reach_support
)

add_library(reach_core STATIC
    src/core/clipboard.c
    src/core/scrollbar.cpp
    src/core/ui_state.c
    src/core/ui_layout.c
    src/core/render_commands.c
    src/core/runtime_policy.c
    src/core/theme.c
)

reach_configure_common(reach_core)
target_compile_features(reach_core PUBLIC
    c_std_11
    cxx_std_17
)

add_library(reach_features STATIC
    src/features/common/scrollbar/scrollbar_render.cpp
    src/features/common/text_edit/text_edit.cpp
    src/features/common/ui_controls/ui_controls.cpp
    src/features/clipboard/clipboard_feature.cpp
    src/features/clipboard/clipboard_interaction.cpp
    src/features/clipboard/clipboard_model.cpp
    src/features/clipboard/clipboard_render.cpp
    src/features/context_menu/context_menu_feature.cpp
    src/features/context_menu/context_menu_interaction.cpp
    src/features/context_menu/context_menu_render.cpp
    src/features/dock/dock_feature.cpp
    src/features/dock/dock_interaction.cpp
    src/features/dock/dock_model.cpp
    src/features/dock/dock_now_playing.cpp
    src/features/dock/dock_render.cpp
    src/features/launcher/launcher_feature.cpp
    src/features/launcher/launcher_interaction.cpp
    src/features/launcher/launcher_render.cpp
    src/features/launcher/launcher_state.cpp
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
)

reach_configure_common(reach_features)
target_compile_features(reach_features PUBLIC
    cxx_std_17
)
target_link_libraries(reach_features PUBLIC
    reach_core
    reach_services
    reach_support
)

add_library(reach_windows_adapters STATIC
    src/adapters/windows/clipboard_win32.cpp
    src/adapters/windows/window_win32.cpp
    src/adapters/windows/explorer_desktop_compat_host_win32.cpp
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
    src/adapters/windows/settings_launcher_win32.cpp
    src/adapters/windows/tray_win32.cpp
    src/adapters/windows/icon_shell.cpp
    src/adapters/windows/windows_icon_handle.cpp
    src/adapters/windows/input_win32.cpp
    src/adapters/windows/media_controls_win32.cpp
    src/adapters/windows/config_ini.cpp
    src/adapters/windows/config_path_win32.cpp
    src/adapters/windows/window_management/window_query_win32.cpp
    src/adapters/windows/window_management/reach_service_client_win32.cpp
    src/adapters/windows/window_management/reach_service_shared_state_win32.cpp
    src/adapters/windows/window_management/reach_service_task_win32.cpp
    src/adapters/windows/window_management/window_manager_win32.cpp
    src/adapters/windows/shell_registration_win32.cpp
    src/adapters/windows/explorer_service_win32.cpp
    src/adapters/windows/wallpaper_win32.cpp
    src/adapters/windows/wallpaper_surface_win32.cpp
    src/adapters/windows/popup_capture_win32.cpp
    src/adapters/windows/power_session_win32.cpp
    src/adapters/windows/audio_volume_win32.cpp
    src/adapters/windows/system_controls_network.cpp
    src/adapters/windows/system_controls_win32.cpp
    src/adapters/windows/dock_reveal_edge_win32.cpp
    src/adapters/windows/monitor_win32.cpp
    src/adapters/windows/startup_apps_win32.cpp
    src/adapters/windows/user_account_win32.cpp
    src/adapters/windows/windows_update_wua.cpp
)

reach_configure_common(reach_windows_adapters)
reach_configure_windows(reach_windows_adapters)
target_compile_features(reach_windows_adapters PUBLIC
    cxx_std_17
)
target_link_libraries(reach_windows_adapters PUBLIC
    reach_core
    reach_protocol
    reach_support
)
target_link_libraries(reach_windows_adapters PRIVATE
    d2d1
    dwrite
    dcomp
    d3d11
    dxgi
    dxguid
    user32
    shell32
    comctl32
    shlwapi
    shcore
    advapi32
    ole32
    oleaut32
    taskschd
    runtimeobject
    uuid
    windowscodecs
    dwmapi
    powrprof
    wlanapi
    iphlpapi
    wbemuuid
    Wuguid
)

add_library(reach_composition STATIC
    src/composition/host_clipboard.cpp
    src/composition/host_app_control.cpp
    src/composition/host_context_menu_orchestration.cpp
    src/composition/host_config.cpp
    src/composition/host_dock_layout.cpp
    src/composition/host_dock_orchestration.cpp
    src/composition/host_event_dispatch.cpp
    src/composition/host_feedback.cpp
    src/composition/host_game_mode.cpp
    src/composition/host_icons.cpp
    src/composition/host_input.cpp
    src/composition/host_launcher_orchestration.cpp
    src/composition/host_launcher_search.cpp
    src/composition/host_lifecycle.cpp
    src/composition/host_popup.cpp
    src/composition/host_popup_surface.cpp
    src/composition/host_quick_settings.cpp
    src/composition/host_render.cpp
    src/composition/host_surface_frames.cpp
    src/composition/host_surfaces.cpp
    src/composition/host_system_actions.cpp
    src/composition/host_switcher_input.cpp
    src/composition/host_tray_orchestration.cpp
    src/composition/host_update.cpp
    src/composition/host_window_tracking.cpp
    src/composition/surface_runtime.cpp
    src/composition/composition_root.cpp
)

reach_configure_common(reach_composition)
reach_configure_windows(reach_composition)
target_compile_features(reach_composition PUBLIC
    c_std_11
    cxx_std_17
)
target_link_libraries(reach_composition PUBLIC
    reach_core
    reach_features
    reach_services
    reach_support
    reach_windows_adapters
)
add_executable(reach WIN32
    src/tools/reach_main.cpp
    resources/reach.rc
)

if (MSVC)
    target_link_options(reach PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/app.manifest"
    )
endif()

reach_configure_common(reach)
reach_configure_windows(reach)
target_compile_features(reach PRIVATE
    c_std_11
    cxx_std_17
)
target_link_libraries(reach PRIVATE
    reach_composition
    reach_core
    reach_features
    reach_support
    reach_windows_adapters
    d2d1
    dwrite
    dcomp
    d3d11
    dxgi
    dxguid
    user32
    shell32
    comctl32
    shlwapi
    shcore
    advapi32
    ole32
    oleaut32
    runtimeobject
    uuid
    windowscodecs
    dwmapi
    powrprof
    wlanapi
    iphlpapi
    wbemuuid
    Wuguid
)

add_library(reach_settings_feature STATIC
    src/apps/settings/feature/settings_account.cpp
    src/apps/settings/feature/settings_bluetooth.cpp
    src/apps/settings/feature/settings_feature.cpp
    src/apps/settings/feature/settings_monitors_scaling.cpp
    src/apps/settings/feature/settings_power_sleep.cpp
    src/apps/settings/feature/settings_render.cpp
    src/apps/settings/feature/settings_startup_apps.cpp
    src/apps/settings/feature/settings_update.cpp
    src/apps/settings/feature/windows_update_policy.cpp
    src/apps/settings/feature/settings_wifi.cpp
)
reach_configure_common(reach_settings_feature)
target_compile_features(reach_settings_feature PUBLIC cxx_std_17)
target_link_libraries(reach_settings_feature PUBLIC
    reach_core
    reach_features
    reach_support
)

add_executable(reach_settings WIN32
    src/apps/settings/app/settings_main.cpp
    src/apps/settings/app/settings_app.cpp
    resources/settings.rc
)
set_target_properties(reach_settings PROPERTIES OUTPUT_NAME "reachSetting")

if (MSVC)
    target_link_options(reach_settings PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/settings.manifest"
    )
endif()

reach_configure_common(reach_settings)
reach_configure_windows(reach_settings)
target_compile_features(reach_settings PRIVATE cxx_std_17)
target_link_libraries(reach_settings PRIVATE
    reach_core
    reach_settings_feature
    reach_support
    reach_windows_adapters
    d2d1
    dwrite
    dcomp
    d3d11
    dxgi
    dxguid
    user32
    shell32
    comctl32
    shlwapi
    shcore
    advapi32
    ole32
    oleaut32
    runtimeobject
    uuid
    windowscodecs
    dwmapi
    powrprof
    wlanapi
    iphlpapi
    wbemuuid
    Wuguid
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
    src/tools/reachctl/reachctl_service.cpp
    src/adapters/windows/window_management/reach_service_task_win32.cpp
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
    oleaut32
    taskschd
    uuid
)

add_executable(reachWatchdog WIN32
    src/tools/reach_watchdog/reach_watchdog.cpp
)
set_target_properties(reachWatchdog PROPERTIES OUTPUT_NAME "reachWatchdog")

reach_configure_common(reachWatchdog)
reach_configure_windows(reachWatchdog)
target_compile_features(reachWatchdog PRIVATE
    cxx_std_17
)
target_link_libraries(reachWatchdog PRIVATE
    shell32
    shlwapi
    user32
    advapi32
)

add_executable(reach_service WIN32
    src/tools/reach_service/reach_service.cpp
    src/tools/reach_service/hotkeys.cpp
    src/tools/reach_service/window_actions.cpp
    src/protocol/reach_service_protocol.cpp
    src/adapters/windows/window_management/reach_service_shared_state_win32.cpp
    src/adapters/windows/window_management/window_query_win32.cpp
    src/support/layout.cpp
    src/support/util.cpp
)
set_target_properties(reach_service PROPERTIES OUTPUT_NAME "reachService")

if (MSVC)
    target_link_options(reach_service PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTUAC:NO"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/elevation_helper.manifest"
    )
endif()

reach_configure_common(reach_service)
reach_configure_windows(reach_service)
target_compile_features(reach_service PRIVATE
    cxx_std_17
)
target_link_libraries(reach_service PRIVATE
    user32
    shell32
    advapi32
    ole32
    propsys
    uuid
)
add_executable(reach_update_helper WIN32
    src/tools/reach_update_helper.cpp
)

if (MSVC)
    target_link_options(reach_update_helper PRIVATE
        "/MANIFEST:EMBED"
        "/MANIFESTUAC:NO"
        "/MANIFESTINPUT:${CMAKE_SOURCE_DIR}/resources/elevation_helper.manifest"
    )
endif()

reach_configure_common(reach_update_helper)
reach_configure_windows(reach_update_helper)
target_compile_features(reach_update_helper PRIVATE cxx_std_17)
target_link_libraries(reach_update_helper PRIVATE
    reach_windows_adapters
    shell32
    ole32
    oleaut32
    Wuguid
)

set(REACH_DIST_ROOT "${CMAKE_BINARY_DIR}/dist")
set(REACH_DIST_DIR "${REACH_DIST_ROOT}/$<CONFIG>")
set(REACH_ZIP_PATH "${REACH_DIST_ROOT}/reach-v${PROJECT_VERSION}-$<CONFIG>.zip")

set(REACH_RELEASE_TARGETS
    reach
    reach_settings
    reachctl
    reachWatchdog
    reach_service
    reach_update_helper
)

add_custom_target(reach_release_zip
    COMMAND ${CMAKE_COMMAND} -E rm -rf "${REACH_DIST_DIR}"
    COMMAND ${CMAKE_COMMAND} -E make_directory "${REACH_DIST_DIR}/reach"

    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "$<TARGET_FILE:reach>"
        "$<TARGET_FILE:reach_settings>"
        "$<TARGET_FILE:reachctl>"
        "$<TARGET_FILE:reachWatchdog>"
        "$<TARGET_FILE:reach_service>"
        "$<TARGET_FILE:reach_update_helper>"
        "${EVERYTHING_DLL}"
        "${REACH_RELEASE_IMAGE}"
        "${REACH_DIST_DIR}/reach"

    COMMAND ${CMAKE_COMMAND} -E chdir "${REACH_DIST_DIR}"
        ${CMAKE_COMMAND} -E tar "cfv" "${REACH_ZIP_PATH}" --format=zip
        "reach"

    DEPENDS ${REACH_RELEASE_TARGETS}
    BYPRODUCTS "${REACH_ZIP_PATH}"
    COMMENT "Creating Reach Release ZIP"
    VERBATIM
)

add_executable(reach_tray_probe EXCLUDE_FROM_ALL
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

if(REACH_BUILD_TESTS)
add_executable(reach_pin_config_tests
    tests/pin_config_tests.cpp
)

reach_configure_common(reach_pin_config_tests)
target_compile_features(reach_pin_config_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_pin_config_tests PRIVATE
    reach_services
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

add_executable(reach_context_menu_feature_tests
    tests/context_menu_feature_tests.cpp
)

reach_configure_common(reach_context_menu_feature_tests)
target_compile_features(reach_context_menu_feature_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_context_menu_feature_tests PRIVATE
    reach_features
)

add_executable(reach_wallpaper_feature_tests
    tests/wallpaper_feature_tests.cpp
)

reach_configure_common(reach_wallpaper_feature_tests)
target_compile_features(reach_wallpaper_feature_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_wallpaper_feature_tests PRIVATE
    reach_features
)

add_executable(reach_now_playing_service_tests
    tests/now_playing_service_tests.cpp
)

reach_configure_common(reach_now_playing_service_tests)
target_compile_features(reach_now_playing_service_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_now_playing_service_tests PRIVATE
    reach_services
)

add_executable(reach_idle_watch_tests
    tests/idle_watch_tests.cpp
)

reach_configure_common(reach_idle_watch_tests)
target_compile_features(reach_idle_watch_tests PRIVATE
    cxx_std_17
)
target_link_libraries(reach_idle_watch_tests PRIVATE
    reach_services
)

add_executable(reach_settings_feature_tests
    tests/settings_feature_tests.cpp
)

reach_configure_common(reach_settings_feature_tests)
target_compile_features(reach_settings_feature_tests PRIVATE cxx_std_17)
target_link_libraries(reach_settings_feature_tests PRIVATE reach_settings_feature)

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

add_executable(reach_clipboard_feature_tests
    tests/clipboard_model_tests.cpp
)

reach_configure_common(reach_clipboard_feature_tests)
target_compile_features(reach_clipboard_feature_tests PRIVATE cxx_std_17)
target_link_libraries(reach_clipboard_feature_tests PRIVATE reach_features)

add_executable(reach_text_edit_tests
    tests/text_edit_tests.cpp
)

reach_configure_common(reach_text_edit_tests)
target_compile_features(reach_text_edit_tests PRIVATE cxx_std_17)
target_link_libraries(reach_text_edit_tests PRIVATE reach_features)

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

add_executable(reach_system_controls_network_tests
    tests/system_controls_network_tests.cpp
    src/adapters/windows/system_controls_network.cpp
)

reach_configure_common(reach_system_controls_network_tests)
reach_configure_windows(reach_system_controls_network_tests)
target_compile_features(reach_system_controls_network_tests PRIVATE
    cxx_std_17
)

    add_test(NAME reach_pin_config_tests COMMAND reach_pin_config_tests)
    add_test(NAME reach_quick_settings_feature_tests COMMAND reach_quick_settings_feature_tests)
    add_test(NAME reach_context_menu_feature_tests COMMAND reach_context_menu_feature_tests)
    add_test(NAME reach_wallpaper_feature_tests COMMAND reach_wallpaper_feature_tests)
    add_test(NAME reach_now_playing_service_tests COMMAND reach_now_playing_service_tests)
    add_test(NAME reach_idle_watch_tests COMMAND reach_idle_watch_tests)
    add_test(NAME reach_settings_feature_tests COMMAND reach_settings_feature_tests)
    add_test(NAME reach_launcher_feature_tests COMMAND reach_launcher_feature_tests)
    add_test(NAME reach_clipboard_feature_tests COMMAND reach_clipboard_feature_tests)
    add_test(NAME reach_text_edit_tests COMMAND reach_text_edit_tests)
    add_test(NAME reach_search_ranking_tests COMMAND reach_search_ranking_tests)
    add_test(NAME reach_system_controls_network_tests COMMAND reach_system_controls_network_tests)
endif()

if(REACH_RUN_ARCHITECTURE_CHECK)
    add_custom_target(check_architecture ALL
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_architecture.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking Reach architecture boundaries"
        VERBATIM
    )
else()
    add_custom_target(check_architecture
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tools/check_architecture.py
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking Reach architecture boundaries"
        VERBATIM
    )
endif()
