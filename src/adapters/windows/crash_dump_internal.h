#ifndef REACH_ADAPTERS_WINDOWS_CRASH_DUMP_INTERNAL_H
#define REACH_ADAPTERS_WINDOWS_CRASH_DUMP_INTERNAL_H

#include <windows.h>
#include <dbghelp.h>

static const wchar_t *REACH_CRASH_DUMP_DIRECTORY = L"dumps";
static const MINIDUMP_TYPE REACH_CRASH_DUMP_TYPE =
    (MINIDUMP_TYPE)(MiniDumpWithDataSegs | MiniDumpWithIndirectlyReferencedMemory |
                    MiniDumpWithThreadInfo | MiniDumpWithUnloadedModules);

#endif
