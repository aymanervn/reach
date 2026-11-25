#include "reach/adapters/windows/crash_report.h"

#include "reach/adapters/windows/crash_dump.h"
#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/util.h"
#include "crash_dump_internal.h"

#include <windows.h>
#include <dbghelp.h>
#include <shlwapi.h>

#include <stdio.h>
#include <string.h>
#include <wchar.h>

static const wchar_t *REACH_CRASH_REPORT_RUN_FILE = L"last_run";
static const wchar_t *REACH_CRASH_REPORT_SYMBOLS_DIRECTORY = L"symbols";
static const DWORD REACH_CRASH_REPORT_CODEVIEW_SIGNATURE = 0x53445352; // "RSDS"
static const size_t REACH_CRASH_REPORT_MIN_BUILD_ID = 33;

// The marker of one Reach run: rewritten at every start, marked clean at a normal exit.
struct reach_crash_report_run
{
    uint32_t process_id;
    uint32_t clean;
    uint64_t start_time;
    char build_id[REACH_CRASH_REPORT_BUILD_ID_CAPACITY];
};

struct reach_crash_report_codeview
{
    DWORD signature;
    GUID guid;
    DWORD age;
    char pdb_path[1];
};

static const reach_crash_report_codeview *reach_crash_report_codeview_record(void)
{
    const BYTE *base = reinterpret_cast<const BYTE *>(GetModuleHandleW(nullptr));
    const IMAGE_DOS_HEADER *dos = reinterpret_cast<const IMAGE_DOS_HEADER *>(base);
    if (base == nullptr || dos->e_magic != IMAGE_DOS_SIGNATURE)
    {
        return nullptr;
    }
    const IMAGE_NT_HEADERS *nt = reinterpret_cast<const IMAGE_NT_HEADERS *>(base + dos->e_lfanew);
    const IMAGE_DATA_DIRECTORY &directory =
        nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_DEBUG];
    const IMAGE_DEBUG_DIRECTORY *entries =
        reinterpret_cast<const IMAGE_DEBUG_DIRECTORY *>(base + directory.VirtualAddress);
    size_t count = directory.Size / sizeof(IMAGE_DEBUG_DIRECTORY);
    for (size_t index = 0; directory.VirtualAddress != 0 && index < count; ++index)
    {
        if (entries[index].Type != IMAGE_DEBUG_TYPE_CODEVIEW ||
            entries[index].AddressOfRawData == 0 ||
            entries[index].SizeOfData < sizeof(reach_crash_report_codeview))
        {
            continue;
        }
        const reach_crash_report_codeview *record =
            reinterpret_cast<const reach_crash_report_codeview *>(base +
                                                                  entries[index].AddressOfRawData);
        if (record->signature == REACH_CRASH_REPORT_CODEVIEW_SIGNATURE)
        {
            return record;
        }
    }
    return nullptr;
}

static void reach_crash_report_format_id(const GUID &guid, DWORD age, char *out_id, size_t capacity)
{
    snprintf(out_id, capacity, "%08lX%04X%04X%02X%02X%02X%02X%02X%02X%02X%02X%lX",
             (unsigned long)guid.Data1, guid.Data2, guid.Data3, guid.Data4[0], guid.Data4[1],
             guid.Data4[2], guid.Data4[3], guid.Data4[4], guid.Data4[5], guid.Data4[6],
             guid.Data4[7], (unsigned long)age);
}

int32_t reach_crash_report_build_id(char *out_id, size_t capacity)
{
    if (out_id == nullptr || capacity < REACH_CRASH_REPORT_BUILD_ID_CAPACITY)
    {
        return 0;
    }
    out_id[0] = 0;
    const reach_crash_report_codeview *record = reach_crash_report_codeview_record();
    if (record == nullptr)
    {
        return 0;
    }
    reach_crash_report_format_id(record->guid, record->age, out_id, capacity);
    return 1;
}

static int32_t reach_crash_report_path(const wchar_t *first, const wchar_t *second,
                                       wchar_t *out_path)
{
    return reach_diagnostics_directory(out_path, MAX_PATH) && PathAppendW(out_path, first) &&
           (second == nullptr || PathAppendW(out_path, second));
}

static int32_t reach_crash_report_read_run(reach_crash_report_run *out_run)
{
    *out_run = {};
    wchar_t path[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_REPORT_RUN_FILE, nullptr, path))
    {
        return 0;
    }
    HANDLE file = CreateFileW(path, GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING,
                              FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    DWORD read = 0;
    BOOL ok = ReadFile(file, out_run, sizeof(*out_run), &read, nullptr);
    CloseHandle(file);
    if (!ok || read != sizeof(*out_run))
    {
        *out_run = {};
        return 0;
    }
    out_run->build_id[REACH_CRASH_REPORT_BUILD_ID_CAPACITY - 1] = 0;
    return 1;
}

static void reach_crash_report_write_run(const reach_crash_report_run *run)
{
    wchar_t path[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_REPORT_RUN_FILE, nullptr, path))
    {
        return;
    }
    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        reach_log_error("run marker not written");
        return;
    }
    DWORD written = 0;
    WriteFile(file, run, sizeof(*run), &written, nullptr);
    CloseHandle(file);
}

static uint64_t reach_crash_report_filetime(FILETIME time)
{
    return ((uint64_t)time.dwHighDateTime << 32) | time.dwLowDateTime;
}

// Reach names its own dumps "<process>-<date>-<time>-<pid>-<build>.dmp"; Windows Error Reporting
// names them "<image>.<pid>.dmp", and those are renamed to carry the build of the run that wrote
// them so the symbol prune can see which builds still have dumps.
static int32_t reach_crash_report_dump_of(const wchar_t *name, uint32_t process_id,
                                          int32_t *out_from_wer)
{
    wchar_t wer[32] = {};
    swprintf_s(wer, L".%lu.dmp", (unsigned long)process_id);
    size_t length = wcslen(name);
    size_t wer_length = wcslen(wer);
    *out_from_wer = length > wer_length && _wcsicmp(name + length - wer_length, wer) == 0;
    if (*out_from_wer)
    {
        return 1;
    }

    wchar_t stem[MAX_PATH] = {};
    if (wcscpy_s(stem, name) != 0)
    {
        return 0;
    }
    PathRemoveExtensionW(stem);
    wchar_t *build = wcsrchr(stem, L'-');
    if (build == nullptr)
    {
        return 0;
    }
    *build = 0;
    wchar_t *pid = wcsrchr(stem, L'-');
    wchar_t *end = nullptr;
    return pid != nullptr && pid[1] != 0 && wcstoul(pid + 1, &end, 10) == process_id &&
           end != nullptr && *end == 0;
}

static void reach_crash_report_scan_dumps(const reach_crash_report_run *previous, wchar_t *out_dump,
                                          size_t dump_count)
{
    out_dump[0] = 0;
    wchar_t directory[MAX_PATH] = {};
    wchar_t pattern[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_DUMP_DIRECTORY, nullptr, directory) ||
        wcscpy_s(pattern, directory) != 0 || !PathAppendW(pattern, L"*.dmp"))
    {
        return;
    }

    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if ((found.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) != 0 ||
            reach_crash_report_filetime(found.ftLastWriteTime) < previous->start_time)
        {
            continue;
        }
        int32_t from_wer = 0;
        if (previous->clean ||
            !reach_crash_report_dump_of(found.cFileName, previous->process_id, &from_wer))
        {
            char line[MAX_PATH + 48] = {};
            snprintf(line, sizeof(line), "dump since last start: %ls", found.cFileName);
            reach_log_info(line);
            continue;
        }

        wcscpy_s(out_dump, dump_count, found.cFileName);
        if (from_wer && previous->build_id[0] != 0)
        {
            wchar_t renamed[MAX_PATH] = {};
            wcscpy_s(renamed, found.cFileName);
            PathRemoveExtensionW(renamed);
            wchar_t suffix[REACH_CRASH_REPORT_BUILD_ID_CAPACITY + 8] = {};
            swprintf_s(suffix, L"-%hs.dmp", previous->build_id);
            wchar_t from[MAX_PATH] = {};
            wchar_t to[MAX_PATH] = {};
            if (wcscat_s(renamed, suffix) == 0 && wcscpy_s(from, directory) == 0 &&
                PathAppendW(from, found.cFileName) && wcscpy_s(to, directory) == 0 &&
                PathAppendW(to, renamed) && MoveFileW(from, to))
            {
                wcscpy_s(out_dump, dump_count, renamed);
            }
        }
    } while (FindNextFileW(search, &found));
    FindClose(search);
}

typedef BOOL(WINAPI *reach_crash_report_index_fn)(PCWSTR, PSYMSRV_INDEX_INFOW, DWORD);

// Calling thread: the archive worker. Confirms pdb_path is the PDB of build_id before keeping it.
static int32_t reach_crash_report_pdb_matches(const wchar_t *pdb_path, const char *build_id)
{
    HMODULE dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    reach_crash_report_index_fn index =
        dbghelp != nullptr ? reinterpret_cast<reach_crash_report_index_fn>(
                                 GetProcAddress(dbghelp, "SymSrvGetFileIndexInfoW"))
                           : nullptr;
    SYMSRV_INDEX_INFOW info = {};
    info.sizeofstruct = sizeof(info);
    char pdb_id[REACH_CRASH_REPORT_BUILD_ID_CAPACITY] = {};
    if (index != nullptr && index(pdb_path, &info, 0))
    {
        reach_crash_report_format_id(info.guid, info.age, pdb_id, sizeof(pdb_id));
    }
    if (dbghelp != nullptr)
    {
        FreeLibrary(dbghelp);
    }
    return pdb_id[0] != 0 && strcmp(pdb_id, build_id) == 0;
}

static void reach_crash_report_keep_symbols(const char *build_id)
{
    const reach_crash_report_codeview *record = reach_crash_report_codeview_record();
    if (record == nullptr)
    {
        return;
    }

    wchar_t source[MAX_PATH] = {};
    MultiByteToWideChar(CP_UTF8, 0, record->pdb_path, -1, source, MAX_PATH);
    const wchar_t *pdb_name = PathFindFileNameW(source);
    if (!PathFileExistsW(source))
    {
        wchar_t beside[MAX_PATH] = {};
        if (GetModuleFileNameW(nullptr, beside, MAX_PATH) == 0 || !PathRemoveFileSpecW(beside) ||
            !PathAppendW(beside, pdb_name))
        {
            return;
        }
        wcscpy_s(source, beside);
        pdb_name = PathFindFileNameW(source);
    }

    char line[160] = {};
    if (!reach_crash_report_pdb_matches(source, build_id))
    {
        snprintf(line, sizeof(line), "symbols unavailable build=%s reason=pdb_mismatch", build_id);
        reach_log_error(line);
        return;
    }

    wchar_t build[REACH_CRASH_REPORT_BUILD_ID_CAPACITY] = {};
    MultiByteToWideChar(CP_UTF8, 0, build_id, -1, build, REACH_CRASH_REPORT_BUILD_ID_CAPACITY);
    wchar_t target[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_REPORT_SYMBOLS_DIRECTORY, nullptr, target))
    {
        return;
    }
    CreateDirectoryW(target, nullptr);
    PathAppendW(target, pdb_name);
    CreateDirectoryW(target, nullptr);
    PathAppendW(target, build);
    CreateDirectoryW(target, nullptr);
    PathAppendW(target, pdb_name);
    if (PathFileExistsW(target))
    {
        return;
    }
    snprintf(line, sizeof(line),
             CopyFileW(source, target, TRUE) ? "symbols kept build=%s"
                                             : "symbols not copied build=%s",
             build_id);
    reach_log_info(line);
}

static int32_t reach_crash_report_dump_needs(const wchar_t *build)
{
    wchar_t pattern[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_DUMP_DIRECTORY, L"*.dmp", pattern))
    {
        return 1;
    }
    WIN32_FIND_DATAW found = {};
    HANDLE search = FindFirstFileW(pattern, &found);
    if (search == INVALID_HANDLE_VALUE)
    {
        return 0;
    }
    int32_t needed = 0;
    do
    {
        needed = wcsstr(found.cFileName, build) != nullptr;
    } while (!needed && FindNextFileW(search, &found));
    FindClose(search);
    return needed;
}

// Layout: symbols\<pdb name>\<build id>\<pdb name>, the symbol-store layout debuggers search.
static void reach_crash_report_prune_symbols(void)
{
    wchar_t root[MAX_PATH] = {};
    wchar_t pattern[MAX_PATH] = {};
    if (!reach_crash_report_path(REACH_CRASH_REPORT_SYMBOLS_DIRECTORY, nullptr, root) ||
        wcscpy_s(pattern, root) != 0 || !PathAppendW(pattern, L"*"))
    {
        return;
    }
    WIN32_FIND_DATAW pdb = {};
    HANDLE pdbs = FindFirstFileW(pattern, &pdb);
    if (pdbs == INVALID_HANDLE_VALUE)
    {
        return;
    }
    do
    {
        if ((pdb.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 || pdb.cFileName[0] == L'.')
        {
            continue;
        }
        wchar_t builds_pattern[MAX_PATH] = {};
        if (wcscpy_s(builds_pattern, root) != 0 || !PathAppendW(builds_pattern, pdb.cFileName) ||
            !PathAppendW(builds_pattern, L"*"))
        {
            continue;
        }
        WIN32_FIND_DATAW build = {};
        HANDLE builds = FindFirstFileW(builds_pattern, &build);
        if (builds == INVALID_HANDLE_VALUE)
        {
            continue;
        }
        do
        {
            if ((build.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == 0 ||
                build.cFileName[0] == L'.' ||
                wcslen(build.cFileName) < REACH_CRASH_REPORT_MIN_BUILD_ID ||
                reach_crash_report_dump_needs(build.cFileName))
            {
                continue;
            }
            wchar_t directory[MAX_PATH] = {};
            wchar_t file[MAX_PATH] = {};
            if (wcscpy_s(directory, root) == 0 && PathAppendW(directory, pdb.cFileName) &&
                PathAppendW(directory, build.cFileName) && wcscpy_s(file, directory) == 0 &&
                PathAppendW(file, pdb.cFileName))
            {
                DeleteFileW(file);
                RemoveDirectoryW(directory);
            }
        } while (FindNextFileW(builds, &build));
        FindClose(builds);
    } while (FindNextFileW(pdbs, &pdb));
    FindClose(pdbs);
}

// Copying a PDB of a hundred megabytes stays off the UI thread.
// Every file touched here can be slow on a cold disk or under antivirus scanning, and the PDB copy
// moves about a hundred megabytes, so the whole report runs off the UI thread.
static DWORD WINAPI reach_crash_report_run_proc(void *)
{
    reach_crash_dump_prune();

    reach_crash_report_run previous = {};
    int32_t has_previous = reach_crash_report_read_run(&previous);

    reach_crash_report_run current = {};
    current.process_id = GetCurrentProcessId();
    FILETIME now = {};
    GetSystemTimeAsFileTime(&now);
    current.start_time = reach_crash_report_filetime(now);
    reach_crash_report_build_id(current.build_id, sizeof(current.build_id));

    int32_t keep = 0;
    if (has_previous)
    {
        wchar_t dump[MAX_PATH] = {};
        reach_crash_report_scan_dumps(&previous, dump, MAX_PATH);
        if (!previous.clean)
        {
            char line[MAX_PATH + 160] = {};
            snprintf(line, sizeof(line),
                     "previous run pid=%lu build=%s ended without a clean exit dump=%ls",
                     (unsigned long)previous.process_id,
                     previous.build_id[0] != 0 ? previous.build_id : "unknown",
                     dump[0] != 0 ? dump : L"none");
            reach_log_info(line);

            if (dump[0] != 0)
            {
                if (previous.build_id[0] != 0 && strcmp(previous.build_id, current.build_id) == 0)
                {
                    keep = 1;
                }
                else
                {
                    snprintf(line, sizeof(line),
                             "symbols unavailable build=%s reason=build_replaced",
                             previous.build_id[0] != 0 ? previous.build_id : "unknown");
                    reach_log_error(line);
                }
            }
        }
    }
    reach_crash_report_write_run(&current);

    if (keep)
    {
        reach_crash_report_keep_symbols(current.build_id);
    }
    reach_crash_report_prune_symbols();
    return 0;
}

void reach_crash_report_begin_run(void)
{
    HANDLE thread = CreateThread(nullptr, 0, reach_crash_report_run_proc, nullptr, 0, nullptr);
    if (thread != nullptr)
    {
        CloseHandle(thread);
    }
    else
    {
        reach_log_error("crash report not started");
    }
}

void reach_crash_report_end_run(void)
{
    reach_crash_report_run run = {};
    if (reach_crash_report_read_run(&run) && run.process_id == GetCurrentProcessId())
    {
        run.clean = 1;
        reach_crash_report_write_run(&run);
    }
}
