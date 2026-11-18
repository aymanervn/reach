#include "windows_adapters_internal.h"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

struct reach_system_stats_source
{
};

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
    out_sample->cpu_total_time =
        reach_system_stats_filetime_to_u64(kernel) + reach_system_stats_filetime_to_u64(user);
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

static bool get_best_interface_index(DWORD *out_index)
{
    sockaddr_in dest = {};
    dest.sin_family = AF_INET;
    dest.sin_port = htons(53);

    if (InetPtonW(AF_INET, L"8.8.8.8", &dest.sin_addr) != 1)
    {
        return false;
    }

    DWORD best_if_index = 0;
    if (GetBestInterfaceEx((sockaddr *)&dest, &best_if_index) != NO_ERROR)
    {
        return false;
    }

    *out_index = best_if_index;
    return true;
}

static void reach_system_stats_sample_network(reach_system_stats_source *source,
                                              reach_system_stats_sample *out_sample)
{
    DWORD best_index = 0;
    if (!get_best_interface_index(&best_index))
    {
        return;
    }

    MIB_IFROW row = {};
    row.dwIndex = best_index;
    if (GetIfEntry(&row) != NO_ERROR)
    {
        return;
    }

    if (row.dwOperStatus != IF_OPER_STATUS_OPERATIONAL)
    {
        return;
    }

    out_sample->network_received_bytes = row.dwInOctets;
    out_sample->network_sent_bytes = row.dwOutOctets;
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
    (void)source;
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
