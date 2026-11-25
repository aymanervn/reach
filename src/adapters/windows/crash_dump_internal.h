#ifndef REACH_ADAPTERS_WINDOWS_CRASH_DUMP_INTERNAL_H
#define REACH_ADAPTERS_WINDOWS_CRASH_DUMP_INTERNAL_H

#include <windows.h>
#include <dbghelp.h>

#include <stddef.h>

static const wchar_t *REACH_CRASH_DUMP_DIRECTORY = L"dumps";
static const MINIDUMP_TYPE REACH_CRASH_DUMP_TYPE =
    (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                    MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

// Dumps this process without exception information, from any thread, once
// reach_crash_dump_install ran. Returns 0 when no dump was written or a crash is being handled.
int reach_crash_dump_write_snapshot(wchar_t *out_name, size_t name_count);

#endif
