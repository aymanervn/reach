#include "reachctl_common.h"
#include "reachctl_config_commands.h"
#include "reachctl_elevation_helper.h"
#include "reachctl_session_commands.h"

#include <windows.h>

int wmain(int argc, wchar_t **argv)
{
    uint16_t reach_exe[260] = {};
    if (reachctl_target_exe(reach_exe, 260) != REACH_OK)
    {
        reachctl_print(L"Could not locate sibling reach.exe.");
        return 1;
    }

    for (int index = 1; index < argc; ++index)
    {
        if (lstrcmpiW(argv[index], L"--install") == 0)
        {
            if (!reachctl_is_process_elevated())
            {
                reachctl_print(L"Reach install requires Administrator privileges to "
                               L"create the watchdog scheduled task. "
                               L"Open PowerShell as Administrator and run reachctl "
                               L"--install again.");
                return 1;
            }

            reach_result shell_result = reachctl_install_reach_shell_and_watchdog(reach_exe);
            reach_result helper_result = reachctl_install_elevation_helper();
            int ok = shell_result == REACH_OK && helper_result == REACH_OK;
            reachctl_print(ok ? L"Reach installed. Shell registry, watchdog task, "
                                L"elevation helper task, and Explorer context menus "
                                L"configured."
                              : L"Reach install failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--start") == 0)
        {
            if (!reachctl_is_reach_installed())
            {
                reachctl_print(L"Reach is not installed. Run reachctl --install from "
                               L"an elevated PowerShell first.");
                return 1;
            }

            if (!reachctl_elevation_helper_is_installed())
            {
                reachctl_print(L"Reach elevation helper task is not installed. Run reachctl "
                               L"--install from an elevated PowerShell first.");
                return 1;
            }

            int ok = reachctl_start_reach_session(reach_exe) == REACH_OK;
            reachctl_print(ok ? L"Reach started for current session." : L"Reach start failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--reset") == 0)
        {
            if (!reachctl_is_process_elevated())
            {
                reachctl_print(L"Reach reset requires Administrator privileges to "
                               L"remove the watchdog scheduled task."
                               L"Open PowerShell as Administrator and run reachctl "
                               L"--reset again.");
                return 1;
            }

            int ok = reachctl_reset_to_windows_shell() == REACH_OK;
            reachctl_print(ok ? L"Reach reset complete. Windows shell restored."
                              : L"Reach reset attempted, but one or more steps failed.");
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--list-monitors") == 0)
        {
            int ok = reachctl_list_monitors() == REACH_OK;
            if (!ok)
            {
                reachctl_print(L"Monitor query failed.");
            }
            return ok ? 0 : 1;
        }

        if (lstrcmpiW(argv[index], L"--pin") == 0)
        {
            if (index + 1 >= argc)
            {
                reachctl_print(L"--pin requires a path.");
                return 2;
            }

            return reachctl_pin_command(argv[index + 1]);
        }

        if (lstrcmpiW(argv[index], L"--wallpaper-monitor") == 0)
        {
            if (index + 2 >= argc)
            {
                reachctl_print(L"--wallpaper-monitor requires an index and a path.");
                return 2;
            }

            return reachctl_wallpaper_monitor_command(argv[index + 1], argv[index + 2]);
        }
    }

    reachctl_print(L"Usage: reachctl.exe\n"
                   L"  --install\n"
                   L"  --start\n"
                   L"  --reset\n"
                   L"  --list-monitors\n"
                   L"  --pin <path>\n"
                   L"  --wallpaper-monitor <index> <path>\n");

    return 2;
}
