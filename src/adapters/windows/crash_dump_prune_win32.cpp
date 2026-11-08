#include "reach/adapters/windows/crash_dump.h"

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/crash_dump_prune.h"

#include <windows.h>
#include <shlwapi.h>

#include <wchar.h>

static const wchar_t *REACH_CRASH_DUMP_DIRECTORY = L"dumps";
static const wchar_t *REACH_CRASH_DUMP_PATTERN = L"*.dmp";

static uint64_t reach_crash_dump_filetime(const FILETIME &time)
{
    return ((uint64_t)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

void reach_crash_dump_prune(void)
{
    wchar_t directory[MAX_PATH] = {};
    if (!reach_diagnostics_directory(directory, MAX_PATH) ||
        !PathAppendW(directory, REACH_CRASH_DUMP_DIRECTORY))
    {
        return;
    }

    wchar_t pattern[MAX_PATH] = {};
    wcscpy_s(pattern, MAX_PATH, directory);
    if (!PathAppendW(pattern, REACH_CRASH_DUMP_PATTERN))
    {
        return;
    }

    static wchar_t names[REACH_CRASH_DUMP_PRUNE_CAPACITY][MAX_PATH];
    reach_crash_dump_entry entries[REACH_CRASH_DUMP_PRUNE_CAPACITY] = {};
    uint32_t count = 0;

    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        return;
    }

    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            count == REACH_CRASH_DUMP_PRUNE_CAPACITY)
        {
            continue;
        }

        wcscpy_s(names[count], MAX_PATH, found.cFileName);
        entries[count].size = ((uint64_t)found.nFileSizeHigh << 32) | found.nFileSizeLow;
        entries[count].modified = reach_crash_dump_filetime(found.ftLastWriteTime);
        count += 1;
    } while (FindNextFileW(search, &found));
    FindClose(search);

    int32_t remove[REACH_CRASH_DUMP_PRUNE_CAPACITY] = {};
    reach_crash_dump_prune_select(entries, count, REACH_CRASH_DUMP_KEEP,
                                  REACH_CRASH_DUMP_LIMIT_BYTES, remove);
    for (uint32_t index = 0; index < count; ++index)
    {
        if (!remove[index])
        {
            continue;
        }

        wchar_t path[MAX_PATH] = {};
        wcscpy_s(path, MAX_PATH, directory);
        if (PathAppendW(path, names[index]))
        {
            DeleteFileW(path);
        }
    }
}
