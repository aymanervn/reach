#include "windows_adapters_internal.h"

#include <windows.h>

#include <new>
#include <string>
#include <thread>

struct reach_windows_config_watch
{
    HANDLE directory;
    HANDLE stop_event;
    HANDLE io_event;
    std::wstring file_name;
    void (*changed)(void *user);
    void *user;
    std::thread thread;
};

static int32_t reach_config_watch_names_file(const reach_windows_config_watch *watch,
                                             const FILE_NOTIFY_INFORMATION *info)
{
    int length = (int)(info->FileNameLength / sizeof(WCHAR));
    return CompareStringOrdinal(info->FileName, length, watch->file_name.c_str(),
                                (int)watch->file_name.size(), TRUE) == CSTR_EQUAL;
}

static int32_t reach_config_watch_batch_names_file(const reach_windows_config_watch *watch,
                                                   const BYTE *buffer, DWORD bytes)
{
    // Overflow (zero bytes) means changes were dropped; treat it as a change.
    if (bytes == 0)
    {
        return 1;
    }
    const BYTE *cursor = buffer;
    for (;;)
    {
        const FILE_NOTIFY_INFORMATION *info =
            reinterpret_cast<const FILE_NOTIFY_INFORMATION *>(cursor);
        if (reach_config_watch_names_file(watch, info))
        {
            return 1;
        }
        if (info->NextEntryOffset == 0 || cursor + info->NextEntryOffset >= buffer + bytes)
        {
            return 0;
        }
        cursor += info->NextEntryOffset;
    }
}

static void reach_config_watch_run(reach_windows_config_watch *watch)
{
    // Saves write a temp file and rename it over the config, so watch names as well as writes.
    const DWORD filter = FILE_NOTIFY_CHANGE_FILE_NAME | FILE_NOTIFY_CHANGE_LAST_WRITE;
    alignas(DWORD) BYTE buffer[4096];
    for (;;)
    {
        OVERLAPPED overlapped = {};
        overlapped.hEvent = watch->io_event;
        if (!ReadDirectoryChangesW(watch->directory, buffer, sizeof(buffer), FALSE, filter, nullptr,
                                   &overlapped, nullptr))
        {
            return;
        }
        HANDLE handles[2] = {watch->stop_event, watch->io_event};
        DWORD wait = WaitForMultipleObjects(2, handles, FALSE, INFINITE);
        if (wait != WAIT_OBJECT_0 + 1)
        {
            CancelIoEx(watch->directory, &overlapped);
            DWORD ignored = 0;
            (void)GetOverlappedResult(watch->directory, &overlapped, &ignored, TRUE);
            return;
        }
        DWORD bytes = 0;
        if (!GetOverlappedResult(watch->directory, &overlapped, &bytes, FALSE))
        {
            // The kernel dropped notifications; the file may have changed, so treat it as one.
            if (GetLastError() != ERROR_NOTIFY_ENUM_DIR)
            {
                return;
            }
            bytes = 0;
        }
        if (reach_config_watch_batch_names_file(watch, buffer, bytes))
        {
            watch->changed(watch->user);
        }
    }
}

static void reach_config_watch_close(reach_windows_config_watch *watch)
{
    if (watch->directory != INVALID_HANDLE_VALUE)
    {
        CloseHandle(watch->directory);
    }
    if (watch->stop_event != nullptr)
    {
        CloseHandle(watch->stop_event);
    }
    if (watch->io_event != nullptr)
    {
        CloseHandle(watch->io_event);
    }
    delete watch;
}

reach_result reach_windows_config_watch_create(const uint16_t *config_path,
                                               void (*changed)(void *user), void *user,
                                               reach_windows_config_watch **out_watch)
{
    if (config_path == nullptr || changed == nullptr || out_watch == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_watch = nullptr;

    std::wstring path(reinterpret_cast<const wchar_t *>(config_path));
    size_t slash = path.find_last_of(L"\\/");
    if (slash == std::wstring::npos || slash + 1 >= path.size())
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_windows_config_watch *watch = new (std::nothrow) reach_windows_config_watch();
    if (watch == nullptr)
    {
        return REACH_ERROR;
    }
    watch->file_name = path.substr(slash + 1);
    watch->changed = changed;
    watch->user = user;
    watch->directory =
        CreateFileW(path.substr(0, slash).c_str(), FILE_LIST_DIRECTORY,
                    FILE_SHARE_READ | FILE_SHARE_WRITE | FILE_SHARE_DELETE, nullptr, OPEN_EXISTING,
                    FILE_FLAG_BACKUP_SEMANTICS | FILE_FLAG_OVERLAPPED, nullptr);
    watch->stop_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    watch->io_event = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (watch->directory == INVALID_HANDLE_VALUE || watch->stop_event == nullptr ||
        watch->io_event == nullptr)
    {
        reach_config_watch_close(watch);
        return REACH_ERROR;
    }

    try
    {
        watch->thread = std::thread(reach_config_watch_run, watch);
    }
    catch (...)
    {
        reach_config_watch_close(watch);
        return REACH_ERROR;
    }
    *out_watch = watch;
    return REACH_OK;
}

void reach_windows_config_watch_destroy(reach_windows_config_watch *watch)
{
    if (watch == nullptr)
    {
        return;
    }
    SetEvent(watch->stop_event);
    if (watch->thread.joinable())
    {
        watch->thread.join();
    }
    reach_config_watch_close(watch);
}
