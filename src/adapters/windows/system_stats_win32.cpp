#include "windows_adapters_internal.h"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>

struct reach_system_stats_source
{
    MIB_IFTABLE *if_table;
    ULONG if_table_size;
};

#define REACH_SYSTEM_STATS_IF_TABLE_INITIAL_BYTES (64u * 1024u)

static reach_system_stats_source reach_system_stats_instance = {};

static uint64_t reach_system_stats_filetime_to_u64(FILETIME time)
{
    ULARGE_INTEGER value = {};
    value.LowPart = time.dwLowDateTime;
    value.HighPart = time.dwHighDateTime;
    return value.QuadPart;
}

static void reach_system_stats_sample_cpu(reach_system_stats_sample *out_sample)
{
    FILETIME idle = {};
    FILETIME kernel = {};
    FILETIME user = {};
    if (!GetSystemTimes(&idle, &kernel, &user))
    {
        return;
    }

    out_sample->cpu_idle_time = reach_system_stats_filetime_to_u64(idle);
    out_sample->cpu_total_time = reach_system_stats_filetime_to_u64(kernel) +
                                 reach_system_stats_filetime_to_u64(user);
}

static void reach_system_stats_sample_memory(reach_system_stats_sample *out_sample)
{
    MEMORYSTATUSEX status = {};
    status.dwLength = sizeof(status);
    if (!GlobalMemoryStatusEx(&status))
    {
        return;
    }

    out_sample->memory_total_bytes = status.ullTotalPhys;
    out_sample->memory_used_bytes =
        status.ullTotalPhys > status.ullAvailPhys ? status.ullTotalPhys - status.ullAvailPhys : 0;
}

static int32_t reach_system_stats_reserve_if_table(reach_system_stats_source *source, ULONG size)
{
    if (source->if_table != nullptr && source->if_table_size >= size)
    {
        return 1;
    }
    if (source->if_table != nullptr)
    {
        HeapFree(GetProcessHeap(), 0, source->if_table);
    }
    source->if_table = (MIB_IFTABLE *)HeapAlloc(GetProcessHeap(), 0, size);
    source->if_table_size = source->if_table != nullptr ? size : 0;
    return source->if_table != nullptr;
}

static void reach_system_stats_sample_network(reach_system_stats_source *source,
                                              reach_system_stats_sample *out_sample)
{
    if (!reach_system_stats_reserve_if_table(source,
                                             REACH_SYSTEM_STATS_IF_TABLE_INITIAL_BYTES))
    {
        return;
    }

    ULONG size = source->if_table_size;
    DWORD result = GetIfTable(source->if_table, &size, FALSE);
    if (result == ERROR_INSUFFICIENT_BUFFER)
    {
        if (!reach_system_stats_reserve_if_table(source, size))
        {
            return;
        }
        size = source->if_table_size;
        result = GetIfTable(source->if_table, &size, FALSE);
    }
    if (result != NO_ERROR)
    {
        return;
    }

    for (DWORD index = 0; index < source->if_table->dwNumEntries; ++index)
    {
        const MIB_IFROW *row = &source->if_table->table[index];
        if (row->dwType == IF_TYPE_SOFTWARE_LOOPBACK)
        {
            continue;
        }
        out_sample->network_received_bytes += row->dwInOctets;
        out_sample->network_sent_bytes += row->dwOutOctets;
    }
}

static reach_result reach_system_stats_sample_source(reach_system_stats_source *source,
                                                     reach_system_stats_sample *out_sample)
{
    if (source == nullptr || out_sample == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_sample = {};
    reach_system_stats_sample_cpu(out_sample);
    reach_system_stats_sample_memory(out_sample);
    reach_system_stats_sample_network(source, out_sample);
    return REACH_OK;
}

static void reach_system_stats_destroy(reach_system_stats_source *source)
{
    if (source == nullptr || source->if_table == nullptr)
    {
        return;
    }
    HeapFree(GetProcessHeap(), 0, source->if_table);
    source->if_table = nullptr;
    source->if_table_size = 0;
}

reach_result reach_windows_create_system_stats(reach_system_stats_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    out_port->source = &reach_system_stats_instance;
    out_port->ops.sample = reach_system_stats_sample_source;
    out_port->ops.destroy = reach_system_stats_destroy;
    return REACH_OK;
}
