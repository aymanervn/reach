#include "windows_adapters_internal.h"

#include <windows.h>
#include <powrprof.h>

#include <new>

struct reach_power_session
{
    int32_t unused;
};

static reach_result reach_power_enable_shutdown_privilege(void)
{
    HANDLE token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_ADJUST_PRIVILEGES | TOKEN_QUERY, &token))
    {
        return REACH_ERROR;
    }

    TOKEN_PRIVILEGES privileges = {};
    privileges.PrivilegeCount = 1;
    privileges.Privileges[0].Attributes = SE_PRIVILEGE_ENABLED;
    if (!LookupPrivilegeValueW(nullptr, SE_SHUTDOWN_NAME, &privileges.Privileges[0].Luid))
    {
        CloseHandle(token);
        return REACH_ERROR;
    }

    BOOL adjusted = AdjustTokenPrivileges(token, FALSE, &privileges, 0, nullptr, nullptr);
    CloseHandle(token);
    return adjusted ? REACH_OK : REACH_ERROR;
}

static reach_result reach_power_session_lock(reach_power_session *session)
{
    (void)session;
    return LockWorkStation() ? REACH_OK : REACH_ERROR;
}

static reach_result reach_power_session_sleep(reach_power_session *session)
{
    (void)session;
    return SetSuspendState(FALSE, FALSE, FALSE) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_power_session_exit_windows(UINT flags)
{
    if ((flags & (EWX_REBOOT | EWX_POWEROFF)) != 0 &&
        reach_power_enable_shutdown_privilege() != REACH_OK)
    {
        return REACH_ERROR;
    }
    return ExitWindowsEx(flags, 0) ? REACH_OK : REACH_ERROR;
}

static reach_result reach_power_session_restart(reach_power_session *session)
{
    (void)session;
    return reach_power_session_exit_windows(EWX_REBOOT);
}

static reach_result reach_power_session_shutdown(reach_power_session *session)
{
    (void)session;
    return reach_power_session_exit_windows(EWX_POWEROFF);
}

static reach_result reach_power_session_sign_out(reach_power_session *session)
{
    (void)session;
    return reach_power_session_exit_windows(EWX_LOGOFF);
}

static reach_result reach_power_session_input_idle_milliseconds(reach_power_session *session,
                                                                uint64_t *out_milliseconds)
{
    (void)session;
    REACH_ASSERT(out_milliseconds != nullptr);
    if (out_milliseconds == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    LASTINPUTINFO info = {};
    info.cbSize = sizeof(info);
    if (!GetLastInputInfo(&info))
    {
        return REACH_ERROR;
    }
    *out_milliseconds = (uint64_t)(uint32_t)(GetTickCount() - info.dwTime);
    return REACH_OK;
}

static reach_result reach_power_session_system_awake_required(reach_power_session *session,
                                                              int32_t *out_required)
{
    (void)session;
    REACH_ASSERT(out_required != nullptr);
    if (out_required == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    ULONG state = 0;
    if (CallNtPowerInformation(SystemExecutionState, nullptr, 0, &state, sizeof(state)) != 0)
    {
        return REACH_ERROR;
    }
    *out_required = (state & (ES_SYSTEM_REQUIRED | ES_DISPLAY_REQUIRED)) != 0;
    return REACH_OK;
}

static uint64_t reach_power_session_now_milliseconds(reach_power_session *session)
{
    (void)session;
    return (uint64_t)GetTickCount64();
}

static void reach_power_session_destroy(reach_power_session *session)
{
    delete session;
}

reach_result reach_windows_create_power_session(reach_power_session_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_power_session *session = new (std::nothrow) reach_power_session();
    if (session == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->session = session;
    out_port->ops.lock = reach_power_session_lock;
    out_port->ops.sleep = reach_power_session_sleep;
    out_port->ops.restart = reach_power_session_restart;
    out_port->ops.shutdown = reach_power_session_shutdown;
    out_port->ops.sign_out = reach_power_session_sign_out;
    out_port->ops.input_idle_milliseconds = reach_power_session_input_idle_milliseconds;
    out_port->ops.system_awake_required = reach_power_session_system_awake_required;
    out_port->ops.now_milliseconds = reach_power_session_now_milliseconds;
    out_port->ops.destroy = reach_power_session_destroy;
    return REACH_OK;
}
