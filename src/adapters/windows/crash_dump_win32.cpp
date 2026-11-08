#include "reach/adapters/windows/crash_dump.h"

#include "reach/adapters/windows/diagnostics_log.h"
#include "reach/support/util.h"

#include <windows.h>
#include <dbghelp.h>
#include <intrin.h>
#include <shlwapi.h>
#include <werapi.h>

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

#include <exception>

typedef BOOL(WINAPI *reach_crash_dump_write_fn)(HANDLE, DWORD, HANDLE, MINIDUMP_TYPE,
                                                PMINIDUMP_EXCEPTION_INFORMATION,
                                                PMINIDUMP_USER_STREAM_INFORMATION,
                                                PMINIDUMP_CALLBACK_INFORMATION);

static const wchar_t *REACH_CRASH_DUMP_DIRECTORY = L"dumps";
static const DWORD REACH_CRASH_DUMP_STACK_BYTES = 256u * 1024u;
static const ULONG REACH_CRASH_DUMP_STACK_GUARANTEE = 64u * 1024u;
static const DWORD REACH_CRASH_DUMP_WAIT_MS = 15000;
static const DWORD REACH_CRASH_DUMP_CODE_INVALID_PARAMETER = 0xC0000417u;
static const DWORD REACH_CRASH_DUMP_CODE_PURE_CALL = 0xE0000001u;
static const DWORD REACH_CRASH_DUMP_CODE_TERMINATE = 0xE0000002u;
static const DWORD REACH_CRASH_DUMP_CODE_ABORT = 0xE0000003u;

static wchar_t reach_crash_dump_process[32] = L"reach";
static wchar_t reach_crash_dump_directory[MAX_PATH] = {};
static reach_crash_dump_write_fn reach_crash_dump_write = nullptr;
static HANDLE reach_crash_dump_request = nullptr;
static HANDLE reach_crash_dump_thread = nullptr;
static volatile LONG reach_crash_dump_claimed = 0;
static EXCEPTION_POINTERS *reach_crash_dump_pointers = nullptr;
static DWORD reach_crash_dump_thread_id = 0;
static DWORD reach_crash_dump_exit_code = 1;

static EXCEPTION_RECORD reach_crash_dump_record = {};
static CONTEXT reach_crash_dump_context = {};
static EXCEPTION_POINTERS reach_crash_dump_synthetic = {&reach_crash_dump_record,
                                                        &reach_crash_dump_context};

static int reach_crash_dump_build_path(wchar_t *out_path, size_t out_count, wchar_t *out_name,
                                       size_t name_count)
{
    SYSTEMTIME now = {};
    GetLocalTime(&now);
    swprintf_s(out_name, name_count, L"%ls-%04u%02u%02u-%02u%02u%02u-%lu.dmp",
               reach_crash_dump_process, (unsigned)now.wYear, (unsigned)now.wMonth,
               (unsigned)now.wDay, (unsigned)now.wHour, (unsigned)now.wMinute,
               (unsigned)now.wSecond, (unsigned long)GetCurrentProcessId());
    if (wcscpy_s(out_path, out_count, reach_crash_dump_directory) != 0)
    {
        return 0;
    }

    return PathAppendW(out_path, out_name);
}

static int reach_crash_dump_write_file(wchar_t *out_name, size_t name_count)
{
    if (reach_crash_dump_write == nullptr || reach_crash_dump_directory[0] == 0)
    {
        return 0;
    }

    wchar_t path[MAX_PATH] = {};
    if (!reach_crash_dump_build_path(path, MAX_PATH, out_name, name_count))
    {
        return 0;
    }

    HANDLE file =
        CreateFileW(path, GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        return 0;
    }

    MINIDUMP_EXCEPTION_INFORMATION exception = {};
    exception.ThreadId = reach_crash_dump_thread_id;
    exception.ExceptionPointers = reach_crash_dump_pointers;
    exception.ClientPointers = FALSE;

    MINIDUMP_TYPE type =
        (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                        MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);
    BOOL written = reach_crash_dump_write(
        GetCurrentProcess(), GetCurrentProcessId(), file, type,
        reach_crash_dump_pointers != nullptr ? &exception : nullptr, nullptr, nullptr);
    CloseHandle(file);
    if (!written)
    {
        DeleteFileW(path);
    }

    return written != FALSE;
}

static DWORD WINAPI reach_crash_dump_thread_main(void *)
{
    WaitForSingleObject(reach_crash_dump_request, INFINITE);

    wchar_t name[96] = {};
    int written = reach_crash_dump_write_file(name, 96);

    void *address = reach_crash_dump_pointers != nullptr &&
                            reach_crash_dump_pointers->ExceptionRecord != nullptr
                        ? reach_crash_dump_pointers->ExceptionRecord->ExceptionAddress
                        : nullptr;
    char message[256] = {};
    if (written)
    {
        snprintf(message, sizeof(message), "crash code=0x%08lX address=%p dump=%ls",
                 (unsigned long)reach_crash_dump_exit_code, address, name);
    }
    else
    {
        snprintf(message, sizeof(message), "crash code=0x%08lX address=%p dump=none",
                 (unsigned long)reach_crash_dump_exit_code, address);
    }
    reach_log_error(message);

    TerminateProcess(GetCurrentProcess(), reach_crash_dump_exit_code);
    return 0;
}

static void reach_crash_dump_signal(EXCEPTION_POINTERS *pointers, DWORD code)
{
    if (InterlockedCompareExchange(&reach_crash_dump_claimed, 1, 0) != 0)
    {
        Sleep(INFINITE);
    }

    reach_crash_dump_exit_code = code != 0 ? code : 1;
    if (reach_crash_dump_request == nullptr)
    {
        TerminateProcess(GetCurrentProcess(), reach_crash_dump_exit_code);
    }

    reach_crash_dump_pointers = pointers;
    reach_crash_dump_thread_id = GetCurrentThreadId();
    SetEvent(reach_crash_dump_request);
    WaitForSingleObject(reach_crash_dump_thread, REACH_CRASH_DUMP_WAIT_MS);
    TerminateProcess(GetCurrentProcess(), reach_crash_dump_exit_code);
}

__declspec(noinline) static void reach_crash_dump_signal_synthetic(DWORD code, void *address)
{
    RtlCaptureContext(&reach_crash_dump_context);
    reach_crash_dump_record.ExceptionCode = code;
    reach_crash_dump_record.ExceptionFlags = EXCEPTION_NONCONTINUABLE;
    reach_crash_dump_record.ExceptionAddress = address;
    reach_crash_dump_signal(&reach_crash_dump_synthetic, code);
}

static LONG WINAPI reach_crash_dump_filter(EXCEPTION_POINTERS *pointers)
{
    DWORD code = pointers != nullptr && pointers->ExceptionRecord != nullptr
                     ? pointers->ExceptionRecord->ExceptionCode
                     : 1;
    reach_crash_dump_signal(pointers, code);
    return EXCEPTION_EXECUTE_HANDLER;
}

static void __cdecl reach_crash_dump_invalid_parameter(const wchar_t *, const wchar_t *,
                                                       const wchar_t *, unsigned int, uintptr_t)
{
    reach_crash_dump_signal_synthetic(REACH_CRASH_DUMP_CODE_INVALID_PARAMETER, _ReturnAddress());
}

static void __cdecl reach_crash_dump_pure_call(void)
{
    reach_crash_dump_signal_synthetic(REACH_CRASH_DUMP_CODE_PURE_CALL, _ReturnAddress());
}

static void reach_crash_dump_terminate(void)
{
    reach_crash_dump_signal_synthetic(REACH_CRASH_DUMP_CODE_TERMINATE, _ReturnAddress());
}

static void __cdecl reach_crash_dump_abort(int)
{
    reach_crash_dump_signal_synthetic(REACH_CRASH_DUMP_CODE_ABORT, _ReturnAddress());
}

static void reach_crash_dump_resolve_directory(void)
{
    if (!reach_diagnostics_directory(reach_crash_dump_directory, MAX_PATH) ||
        !PathAppendW(reach_crash_dump_directory, REACH_CRASH_DUMP_DIRECTORY))
    {
        reach_crash_dump_directory[0] = 0;
        return;
    }

    if (!CreateDirectoryW(reach_crash_dump_directory, nullptr) &&
        GetLastError() != ERROR_ALREADY_EXISTS)
    {
        reach_crash_dump_directory[0] = 0;
    }
}

void reach_crash_dump_install(const char *process_name)
{
    SetThreadErrorMode(SEM_FAILCRITICALERRORS, nullptr);
    WerSetFlags(WER_FAULT_REPORTING_NO_UI);

    if (process_name != nullptr && process_name[0] != 0)
    {
        swprintf_s(reach_crash_dump_process, L"%hs", process_name);
    }

    reach_crash_dump_resolve_directory();

    HMODULE dbghelp = LoadLibraryExW(L"dbghelp.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
    if (dbghelp != nullptr)
    {
        reach_crash_dump_write =
            (reach_crash_dump_write_fn)GetProcAddress(dbghelp, "MiniDumpWriteDump");
    }

    reach_crash_dump_request = CreateEventW(nullptr, TRUE, FALSE, nullptr);
    if (reach_crash_dump_request != nullptr)
    {
        reach_crash_dump_thread = CreateThread(nullptr, REACH_CRASH_DUMP_STACK_BYTES,
                                               reach_crash_dump_thread_main, nullptr, 0, nullptr);
        if (reach_crash_dump_thread == nullptr)
        {
            CloseHandle(reach_crash_dump_request);
            reach_crash_dump_request = nullptr;
        }
    }

    ULONG guarantee = REACH_CRASH_DUMP_STACK_GUARANTEE;
    SetThreadStackGuarantee(&guarantee);

    SetUnhandledExceptionFilter(reach_crash_dump_filter);
    _set_invalid_parameter_handler(reach_crash_dump_invalid_parameter);
    _set_purecall_handler(reach_crash_dump_pure_call);
    std::set_terminate(reach_crash_dump_terminate);
    signal(SIGABRT, reach_crash_dump_abort);
}
