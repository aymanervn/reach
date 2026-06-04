#include "elevation_helper_session_win32.h"

#include "elevation_helper_client_win32.h"

#include <windows.h>
#include <sddl.h>

#include <vector>

struct reach_elevation_helper_session
{
    HANDLE event_thread;
    HANDLE event_stop;
    HANDLE state_changed;
    wchar_t event_pipe_name[128];
    LONG state;
    reach_elevation_helper_session_event_callback event_callback;
    reach_elevation_helper_session_state_callback state_callback;
    void *callback_user;
};

static reach_elevation_helper_session g_helper_session;

static reach_result reach_helper_session_query_token_user(HANDLE token, std::vector<BYTE> *out_user)
{
    if (token == nullptr || out_user == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    DWORD needed = 0;
    (void)GetTokenInformation(token, TokenUser, nullptr, 0, &needed);
    if (needed == 0)
    {
        return REACH_ERROR;
    }

    out_user->resize(needed);
    if (!GetTokenInformation(token, TokenUser, out_user->data(), needed, &needed))
    {
        out_user->clear();
        return REACH_ERROR;
    }

    return REACH_OK;
}

static int32_t reach_helper_session_same_user_client(HANDLE pipe)
{
    HANDLE process_token = nullptr;
    if (!OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &process_token))
    {
        return 0;
    }

    std::vector<BYTE> process_user;
    reach_result process_result =
        reach_helper_session_query_token_user(process_token, &process_user);
    CloseHandle(process_token);
    if (process_result != REACH_OK)
    {
        return 0;
    }

    if (!ImpersonateNamedPipeClient(pipe))
    {
        return 0;
    }

    HANDLE client_token = nullptr;
    BOOL opened_client = OpenThreadToken(GetCurrentThread(), TOKEN_QUERY, TRUE, &client_token);
    RevertToSelf();
    if (!opened_client)
    {
        return 0;
    }

    std::vector<BYTE> client_user;
    reach_result client_result = reach_helper_session_query_token_user(client_token, &client_user);
    CloseHandle(client_token);
    if (client_result != REACH_OK)
    {
        return 0;
    }

    TOKEN_USER *process_token_user = reinterpret_cast<TOKEN_USER *>(process_user.data());
    TOKEN_USER *client_token_user = reinterpret_cast<TOKEN_USER *>(client_user.data());
    if (process_token_user == nullptr || client_token_user == nullptr ||
        process_token_user->User.Sid == nullptr || client_token_user->User.Sid == nullptr)
    {
        return 0;
    }

    return EqualSid(process_token_user->User.Sid, client_token_user->User.Sid);
}

static SECURITY_ATTRIBUTES reach_helper_session_pipe_security(PSECURITY_DESCRIPTOR *sd)
{
    *sd = nullptr;
    SECURITY_ATTRIBUTES attributes = {};
    attributes.nLength = sizeof(attributes);
    attributes.bInheritHandle = FALSE;

    const wchar_t *sddl = L"D:P(A;;GA;;;IU)(A;;GA;;;SY)(A;;GA;;;BA)";
    if (ConvertStringSecurityDescriptorToSecurityDescriptorW(sddl, SDDL_REVISION_1, sd, nullptr))
    {
        attributes.lpSecurityDescriptor = *sd;
    }

    return attributes;
}

static HANDLE reach_helper_session_create_event_pipe(const wchar_t *name)
{
    PSECURITY_DESCRIPTOR descriptor = nullptr;
    SECURITY_ATTRIBUTES security = reach_helper_session_pipe_security(&descriptor);
    HANDLE pipe = CreateNamedPipeW(
        name, PIPE_ACCESS_DUPLEX, PIPE_TYPE_MESSAGE | PIPE_READMODE_MESSAGE | PIPE_WAIT, 1,
        sizeof(reach_elevation_helper_event), sizeof(reach_elevation_helper_event), 250,
        security.lpSecurityDescriptor != nullptr ? &security : nullptr);

    if (descriptor != nullptr)
    {
        LocalFree(descriptor);
    }

    return pipe;
}

static reach_elevation_helper_session_state reach_helper_session_state_from_long(LONG state)
{
    switch (state)
    {
    case REACH_ELEVATION_HELPER_SESSION_CONNECTED:
        return REACH_ELEVATION_HELPER_SESSION_CONNECTED;
    case REACH_ELEVATION_HELPER_SESSION_DISCONNECTED:
        return REACH_ELEVATION_HELPER_SESSION_DISCONNECTED;
    default:
        return REACH_ELEVATION_HELPER_SESSION_UNKNOWN;
    }
}

static void reach_helper_session_set_state(reach_elevation_helper_session_state state)
{
    LONG previous = InterlockedExchange(&g_helper_session.state, static_cast<LONG>(state));
    if (previous != static_cast<LONG>(state) && g_helper_session.state_changed != nullptr)
    {
        SetEvent(g_helper_session.state_changed);
    }
    if (previous != static_cast<LONG>(state) && g_helper_session.state_callback != nullptr)
    {
        g_helper_session.state_callback(g_helper_session.callback_user, state);
    }
}

static void reach_helper_session_dispatch_event(const reach_elevation_helper_event *event)
{
    if (event == nullptr || event->version != reach_elevation_helper_protocol_version())
    {
        return;
    }

    if (event->type == REACH_ELEVATION_HELPER_EVENT_CONNECTED)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_CONNECTED);
        return;
    }

    if (event->type == REACH_ELEVATION_HELPER_EVENT_DISCONNECTING)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
        return;
    }

    if (g_helper_session.event_callback != nullptr)
    {
        g_helper_session.event_callback(g_helper_session.callback_user, event);
    }
}

static DWORD WINAPI reach_helper_session_event_thread(void *param)
{
    (void)param;

    for (;;)
    {
        if (WaitForSingleObject(g_helper_session.event_stop, 0) == WAIT_OBJECT_0)
        {
            return 0;
        }

        HANDLE pipe = reach_helper_session_create_event_pipe(g_helper_session.event_pipe_name);
        if (pipe == INVALID_HANDLE_VALUE)
        {
            Sleep(250);
            continue;
        }

        BOOL connected =
            ConnectNamedPipe(pipe, nullptr) ? TRUE : GetLastError() == ERROR_PIPE_CONNECTED;
        if (connected)
        {
            int32_t same_user = 0;
            int32_t same_user_checked = 0;
            for (;;)
            {
                reach_elevation_helper_event event = {};
                DWORD read = 0;
                if (!ReadFile(pipe, &event, sizeof(event), &read, nullptr) || read != sizeof(event))
                {
                    break;
                }
                if (!same_user_checked)
                {
                    same_user = reach_helper_session_same_user_client(pipe);
                    same_user_checked = 1;
                }
                if (same_user)
                {
                    reach_helper_session_dispatch_event(&event);
                }
            }
            if (same_user_checked && same_user &&
                WaitForSingleObject(g_helper_session.event_stop, 0) != WAIT_OBJECT_0)
            {
                reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
            }
            DisconnectNamedPipe(pipe);
        }

        CloseHandle(pipe);
    }
}

static void reach_helper_session_wake_event_thread(void)
{
    if (g_helper_session.event_pipe_name[0] == 0)
    {
        return;
    }

    HANDLE pipe = CreateFileW(g_helper_session.event_pipe_name, GENERIC_WRITE, 0, nullptr,
                              OPEN_EXISTING, 0, nullptr);
    if (pipe != INVALID_HANDLE_VALUE)
    {
        reach_elevation_helper_event event = {};
        DWORD written = 0;
        (void)WriteFile(pipe, &event, sizeof(event), &written, nullptr);
        CloseHandle(pipe);
    }
}

static reach_result reach_helper_session_ensure_event_thread(void)
{
    if (g_helper_session.event_pipe_name[0] == 0)
    {
        swprintf_s(g_helper_session.event_pipe_name, 128, L"\\\\.\\pipe\\ReachHelperEvents-%lu",
                   static_cast<unsigned long>(GetCurrentProcessId()));
    }

    if (g_helper_session.event_stop == nullptr)
    {
        g_helper_session.event_stop = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }
    else
    {
        ResetEvent(g_helper_session.event_stop);
    }

    if (g_helper_session.event_stop == nullptr)
    {
        return REACH_ERROR;
    }

    if (g_helper_session.state_changed == nullptr)
    {
        g_helper_session.state_changed = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    }

    if (g_helper_session.state_changed == nullptr)
    {
        return REACH_ERROR;
    }

    if (g_helper_session.event_thread == nullptr)
    {
        g_helper_session.event_thread =
            CreateThread(nullptr, 0, reach_helper_session_event_thread, nullptr, 0, nullptr);
    }

    return g_helper_session.event_thread != nullptr ? REACH_OK : REACH_ERROR;
}

reach_result reach_elevation_helper_session_start(
    reach_elevation_helper_session_event_callback event_callback,
    reach_elevation_helper_session_state_callback state_callback, void *user)
{
    g_helper_session.event_callback = event_callback;
    g_helper_session.state_callback = state_callback;
    g_helper_session.callback_user = user;

    if (reach_helper_session_ensure_event_thread() != REACH_OK)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
        return REACH_ERROR;
    }

    return reach_elevation_helper_session_reconnect();
}

void reach_elevation_helper_session_stop(void)
{
    g_helper_session.event_callback = nullptr;
    g_helper_session.state_callback = nullptr;
    g_helper_session.callback_user = nullptr;

    (void)reach_elevation_helper_set_hotkey_forwarding(0, 0, nullptr);
    (void)reach_elevation_helper_set_event_channel(0, nullptr);

    if (g_helper_session.event_stop != nullptr)
    {
        SetEvent(g_helper_session.event_stop);
        reach_helper_session_wake_event_thread();
    }
    if (g_helper_session.event_thread != nullptr)
    {
        WaitForSingleObject(g_helper_session.event_thread, 1000);
        CloseHandle(g_helper_session.event_thread);
        g_helper_session.event_thread = nullptr;
    }
    if (g_helper_session.event_stop != nullptr)
    {
        CloseHandle(g_helper_session.event_stop);
        g_helper_session.event_stop = nullptr;
    }
    if (g_helper_session.state_changed != nullptr)
    {
        CloseHandle(g_helper_session.state_changed);
        g_helper_session.state_changed = nullptr;
    }

    reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
}

reach_result reach_elevation_helper_session_reconnect(void)
{
    if (reach_helper_session_ensure_event_thread() != REACH_OK)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
        return REACH_ERROR;
    }

    reach_result result = reach_elevation_helper_set_event_channel(
        1, g_helper_session.event_pipe_name);
    if (result == REACH_OK)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_CONNECTED);
    }
    else
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
    }
    return result;
}

reach_elevation_helper_session_state reach_elevation_helper_session_get_state(void)
{
    return reach_helper_session_state_from_long(InterlockedCompareExchange(&g_helper_session.state,
                                                                           0, 0));
}

reach_result reach_elevation_helper_session_wait_connected(uint32_t timeout_ms)
{
    if (reach_helper_session_ensure_event_thread() != REACH_OK)
    {
        return REACH_ERROR;
    }

    DWORD start = GetTickCount();
    for (;;)
    {
        if (reach_elevation_helper_session_get_state() == REACH_ELEVATION_HELPER_SESSION_CONNECTED)
        {
            return REACH_OK;
        }

        DWORD elapsed = GetTickCount() - start;
        if (elapsed >= timeout_ms)
        {
            return REACH_ERROR;
        }

        ResetEvent(g_helper_session.state_changed);
        if (reach_elevation_helper_session_get_state() == REACH_ELEVATION_HELPER_SESSION_CONNECTED)
        {
            return REACH_OK;
        }
        DWORD remaining = timeout_ms - elapsed;
        DWORD wait = WaitForSingleObject(g_helper_session.state_changed, remaining);
        if (wait != WAIT_OBJECT_0 && wait != WAIT_TIMEOUT)
        {
            return REACH_ERROR;
        }
    }
}

reach_result reach_elevation_helper_session_send(reach_elevation_helper_command command,
                                                 uintptr_t window_id, reach_split_mode mode)
{
    if (reach_elevation_helper_session_get_state() != REACH_ELEVATION_HELPER_SESSION_CONNECTED)
    {
        return REACH_ERROR;
    }

    return reach_elevation_helper_send(command, window_id, mode);
}

reach_result reach_elevation_helper_session_set_hotkey_forwarding(int32_t enabled,
                                                                  uint32_t hotkey_mask)
{
    if (reach_elevation_helper_session_get_state() != REACH_ELEVATION_HELPER_SESSION_CONNECTED)
    {
        return REACH_ERROR;
    }

    reach_result result = reach_elevation_helper_set_hotkey_forwarding(enabled, hotkey_mask,
                                                                       nullptr);
    if (result != REACH_OK && enabled)
    {
        reach_helper_session_set_state(REACH_ELEVATION_HELPER_SESSION_DISCONNECTED);
    }
    return result;
}
