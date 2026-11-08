#include "reach/adapters/windows/diagnostics_log.h"

#include "reach/support/diagnostics_log.h"
#include "reach/support/util.h"

#include <windows.h>
#include <knownfolders.h>
#include <shlobj.h>
#include <shlwapi.h>

#include <wchar.h>

static const wchar_t *REACH_DIAGNOSTICS_MUTEX_NAME = L"Local\\Reach.Diagnostics.Log";
static const DWORD REACH_DIAGNOSTICS_MUTEX_WAIT_MS = 2000;

static char reach_diagnostics_process[32] = "reach";
static HANDLE reach_diagnostics_mutex = nullptr;
static wchar_t reach_diagnostics_dir[MAX_PATH] = {};
static INIT_ONCE reach_diagnostics_dir_once = INIT_ONCE_STATIC_INIT;

static BOOL CALLBACK reach_diagnostics_resolve_directory(PINIT_ONCE, PVOID, PVOID *)
{
    PWSTR local_app_data = nullptr;
    if (FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData, KF_FLAG_DEFAULT, nullptr,
                                    &local_app_data)) ||
        local_app_data == nullptr)
    {
        return TRUE;
    }

    wchar_t path[MAX_PATH] = {};
    int ok = wcscpy_s(path, MAX_PATH, local_app_data) == 0;
    CoTaskMemFree(local_app_data);
    if (!ok || !PathAppendW(path, L"Reach"))
    {
        return TRUE;
    }

    CreateDirectoryW(path, nullptr);
    if (!PathAppendW(path, L"diagnostics"))
    {
        return TRUE;
    }

    if (!CreateDirectoryW(path, nullptr) && GetLastError() != ERROR_ALREADY_EXISTS)
    {
        return TRUE;
    }

    wcscpy_s(reach_diagnostics_dir, MAX_PATH, path);
    return TRUE;
}

int32_t reach_diagnostics_directory(wchar_t *out_path, size_t path_count)
{
    if (out_path == nullptr || path_count == 0)
    {
        return 0;
    }

    InitOnceExecuteOnce(&reach_diagnostics_dir_once, reach_diagnostics_resolve_directory, nullptr,
                        nullptr);
    if (reach_diagnostics_dir[0] == 0)
    {
        out_path[0] = 0;
        return 0;
    }

    return wcscpy_s(out_path, path_count, reach_diagnostics_dir) == 0;
}

static int reach_diagnostics_generation_path(int32_t generation, wchar_t *out_path)
{
    wchar_t directory[MAX_PATH] = {};
    if (!reach_diagnostics_directory(directory, MAX_PATH))
    {
        return 0;
    }

    wchar_t name[32] = {};
    if (generation == 0)
    {
        wcscpy_s(name, L"events.log");
    }
    else
    {
        swprintf_s(name, L"events.%d.log", (int)generation);
    }

    wcscpy_s(out_path, MAX_PATH, directory);
    return PathAppendW(out_path, name);
}

static void reach_diagnostics_rotate(void)
{
    reach_log_rotation_step steps[REACH_DIAGNOSTICS_LOG_KEEP + 1] = {};
    uint32_t count =
        reach_log_rotation_steps(REACH_DIAGNOSTICS_LOG_KEEP, steps, REACH_DIAGNOSTICS_LOG_KEEP + 1);
    for (uint32_t index = 0; index < count; ++index)
    {
        wchar_t from[MAX_PATH] = {};
        if (!reach_diagnostics_generation_path(steps[index].from, from))
        {
            continue;
        }

        if (steps[index].to < 0)
        {
            DeleteFileW(from);
            continue;
        }

        wchar_t to[MAX_PATH] = {};
        if (reach_diagnostics_generation_path(steps[index].to, to))
        {
            MoveFileExW(from, to, MOVEFILE_REPLACE_EXISTING);
        }
    }
}

static HANDLE reach_diagnostics_open_live(void)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_diagnostics_generation_path(0, path))
    {
        return INVALID_HANDLE_VALUE;
    }

    return CreateFileW(path, FILE_APPEND_DATA,
                       FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_ALWAYS,
                       FILE_ATTRIBUTE_NORMAL, nullptr);
}

void reach_diagnostics_log_write(const char *level, const char *message)
{
    if (message == nullptr)
    {
        return;
    }

    SYSTEMTIME now = {};
    GetSystemTime(&now);
    reach_log_timestamp stamp = {now.wYear,   now.wMonth,  now.wDay,         now.wHour,
                                 now.wMinute, now.wSecond, now.wMilliseconds};

    char line[REACH_DIAGNOSTICS_LINE_CAPACITY] = {};
    size_t length = reach_log_format_line(line, sizeof(line), &stamp, reach_diagnostics_process,
                                          GetCurrentProcessId(), level, message);
    if (length == 0)
    {
        return;
    }

    bool locked = false;
    if (reach_diagnostics_mutex != nullptr)
    {
        DWORD wait = WaitForSingleObject(reach_diagnostics_mutex, REACH_DIAGNOSTICS_MUTEX_WAIT_MS);
        locked = wait == WAIT_OBJECT_0 || wait == WAIT_ABANDONED;
    }

    HANDLE file = reach_diagnostics_open_live();
    if (file != INVALID_HANDLE_VALUE)
    {
        LARGE_INTEGER size = {};
        if (locked && GetFileSizeEx(file, &size) &&
            reach_log_rotation_needed((uint64_t)size.QuadPart, length,
                                      REACH_DIAGNOSTICS_LOG_LIMIT_BYTES))
        {
            CloseHandle(file);
            reach_diagnostics_rotate();
            file = reach_diagnostics_open_live();
        }

        if (file != INVALID_HANDLE_VALUE)
        {
            DWORD written = 0;
            WriteFile(file, line, (DWORD)length, &written, nullptr);
            CloseHandle(file);
        }
    }

    if (locked)
    {
        ReleaseMutex(reach_diagnostics_mutex);
    }
}

static void reach_diagnostics_sink(const char *level, const char *message)
{
    reach_diagnostics_log_write(level, message);
}

void reach_diagnostics_log_install(const char *process_name)
{
    if (process_name != nullptr && process_name[0] != 0)
    {
        strncpy_s(reach_diagnostics_process, sizeof(reach_diagnostics_process), process_name,
                  _TRUNCATE);
    }

    if (reach_diagnostics_mutex == nullptr)
    {
        reach_diagnostics_mutex = CreateMutexW(nullptr, FALSE, REACH_DIAGNOSTICS_MUTEX_NAME);
    }

    reach_log_set_sink(reach_diagnostics_sink);
}
