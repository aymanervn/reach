#include "reach_service_client_win32.h"

#include <windows.h>

static const DWORD REACH_SERVICE_IO_TIMEOUT_MS = 750;

static BOOL reach_service_wait_for_overlapped(HANDLE pipe, OVERLAPPED *overlapped,
                                                       DWORD *out_transferred)
{
    if (pipe == INVALID_HANDLE_VALUE || overlapped == nullptr || out_transferred == nullptr)
    {
        return FALSE;
    }

    DWORD transferred = 0;
    DWORD wait = WaitForSingleObject(overlapped->hEvent, REACH_SERVICE_IO_TIMEOUT_MS);
    if (wait == WAIT_OBJECT_0)
    {
        BOOL ok = GetOverlappedResult(pipe, overlapped, &transferred, FALSE);
        if (ok)
        {
            *out_transferred = transferred;
        }
        return ok;
    }

    CancelIo(pipe);
    return FALSE;
}

static BOOL reach_service_write_request(HANDLE pipe,
                                                 const reach_service_request *request,
                                                 DWORD *out_written)
{
    if (pipe == INVALID_HANDLE_VALUE || request == nullptr || out_written == nullptr)
    {
        return FALSE;
    }

    *out_written = 0;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr)
    {
        return FALSE;
    }

    DWORD written = 0;
    BOOL ok = WriteFile(pipe, request, sizeof(*request), nullptr, &overlapped);
    if (ok)
    {
        ok = GetOverlappedResult(pipe, &overlapped, &written, FALSE);
    }
    else if (GetLastError() == ERROR_IO_PENDING)
    {
        ok = reach_service_wait_for_overlapped(pipe, &overlapped, &written);
    }

    CloseHandle(overlapped.hEvent);
    if (ok)
    {
        *out_written = written;
    }
    return ok;
}

static BOOL reach_service_read_response(HANDLE pipe,
                                                 reach_service_response *response,
                                                 DWORD *out_read)
{
    if (pipe == INVALID_HANDLE_VALUE || response == nullptr || out_read == nullptr)
    {
        return FALSE;
    }

    *out_read = 0;
    OVERLAPPED overlapped = {};
    overlapped.hEvent = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (overlapped.hEvent == nullptr)
    {
        return FALSE;
    }

    DWORD read = 0;
    BOOL ok = ReadFile(pipe, response, sizeof(*response), nullptr, &overlapped);
    if (ok)
    {
        ok = GetOverlappedResult(pipe, &overlapped, &read, FALSE);
    }
    else if (GetLastError() == ERROR_IO_PENDING)
    {
        ok = reach_service_wait_for_overlapped(pipe, &overlapped, &read);
    }

    CloseHandle(overlapped.hEvent);
    if (ok)
    {
        *out_read = read;
    }
    return ok;
}

reach_result reach_service_send_request(const reach_service_request *request,
                                                 reach_service_response *out_response)
{
    if (!reach_service_request_valid(request))
    {
        return REACH_INVALID_ARGUMENT;
    }
    if (!WaitNamedPipeW(REACH_SERVICE_PIPE_NAME, 50))
    {
        return REACH_ERROR;
    }

    HANDLE pipe = CreateFileW(REACH_SERVICE_PIPE_NAME, GENERIC_READ | GENERIC_WRITE, 0,
                              nullptr, OPEN_EXISTING, FILE_FLAG_OVERLAPPED, nullptr);
    if (pipe == INVALID_HANDLE_VALUE)
    {
        return REACH_ERROR;
    }

    DWORD mode_bytes = PIPE_READMODE_MESSAGE;
    (void)SetNamedPipeHandleState(pipe, &mode_bytes, nullptr, nullptr);

    DWORD written = 0;
    BOOL ok = reach_service_write_request(pipe, request, &written);
    if (!ok || written != sizeof(*request))
    {
        CloseHandle(pipe);
        return REACH_ERROR;
    }

    reach_service_response response = {};
    DWORD read = 0;
    ok = reach_service_read_response(pipe, &response, &read);
    CloseHandle(pipe);

    if (!ok || read != sizeof(response) ||
        response.version != reach_service_protocol_version())
    {
        return REACH_ERROR;
    }

    if (out_response != nullptr)
    {
        *out_response = response;
    }
    return static_cast<reach_result>(response.result);
}

reach_result reach_service_send(reach_service_command command,
                                         uintptr_t window_id, reach_split_mode mode)
{
    reach_service_request request = {};
    request.version = reach_service_protocol_version();
    request.command = command;
    request.window = static_cast<uint64_t>(window_id);
    request.split_mode = static_cast<int32_t>(mode);
    return reach_service_send_request(&request, nullptr);
}
