#include "windows_adapters_internal.h"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <cmath>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

struct reach_system_stats_source
{
    PDH_HQUERY gpu_query;
    PDH_HCOUNTER gpu_engine;
    bool gpu_probe_attempted;
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

static void reach_system_stats_sample_gpu_engines(reach_system_stats_source *source,
                                          reach_system_stats_sample *out_sample)
{
    if (source->gpu_query == nullptr || source->gpu_engine == nullptr ||
        PdhCollectQueryData(source->gpu_query) != ERROR_SUCCESS)
        return;
    DWORD bytes = 0;
    DWORD count = 0;
    if (PdhGetFormattedCounterArrayW(source->gpu_engine, PDH_FMT_DOUBLE, &bytes, &count, nullptr) !=
            PDH_MORE_DATA ||
        bytes == 0)
        return;
    std::vector<BYTE> buffer(bytes);
    auto *items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
    if (PdhGetFormattedCounterArrayW(source->gpu_engine, PDH_FMT_DOUBLE, &bytes, &count, items) !=
        ERROR_SUCCESS)
        return;
    std::map<std::wstring, double> engines;
    for (DWORD index = 0; index < count; ++index)
    {
        const auto &item = items[index];
        if ((item.FmtValue.CStatus != PDH_CSTATUS_VALID_DATA &&
             item.FmtValue.CStatus != PDH_CSTATUS_NEW_DATA) ||
            item.szName == nullptr || !std::isfinite(item.FmtValue.doubleValue))
            continue;
        const wchar_t *identity = wcsstr(item.szName, L"luid_");
        if (identity == nullptr)
            continue;
        engines[identity] += std::max(0.0, item.FmtValue.doubleValue);
    }
    for (const auto &engine : engines)
    {
        out_sample->gpu_percent =
            std::max(out_sample->gpu_percent, (float)std::clamp(engine.second, 0.0, 100.0));
        out_sample->gpu_available = 1;
    }
}

struct reach_system_stats_nvml_session
{
    using initialize_fn = int (*)();
    using count_fn = int (*)(unsigned int *);
    using device_fn = int (*)(unsigned int, void **);
    using temperature_fn = int (*)(void *, unsigned int, unsigned int *);
    HMODULE library = nullptr;
    initialize_fn shutdown = nullptr;
    count_fn count = nullptr;
    device_fn device = nullptr;
    temperature_fn temperature = nullptr;
    bool initialized = false;

    reach_system_stats_nvml_session()
    {
        library = LoadLibraryExW(L"nvml.dll", nullptr, LOAD_LIBRARY_SEARCH_SYSTEM32);
        if (library == nullptr)
            return;
        auto initialize = reinterpret_cast<initialize_fn>(GetProcAddress(library, "nvmlInit_v2"));
        shutdown = reinterpret_cast<initialize_fn>(GetProcAddress(library, "nvmlShutdown"));
        count = reinterpret_cast<count_fn>(GetProcAddress(library, "nvmlDeviceGetCount_v2"));
        device =
            reinterpret_cast<device_fn>(GetProcAddress(library, "nvmlDeviceGetHandleByIndex_v2"));
        temperature =
            reinterpret_cast<temperature_fn>(GetProcAddress(library, "nvmlDeviceGetTemperature"));
        initialized = initialize != nullptr && shutdown != nullptr && count != nullptr &&
                      device != nullptr && temperature != nullptr && initialize() == 0;
    }

    ~reach_system_stats_nvml_session()
    {
        if (initialized)
            shutdown();
        if (library != nullptr)
            FreeLibrary(library);
    }
};

static void reach_system_stats_sample_gpu_temperature(reach_system_stats_sample *out_sample)
{
    thread_local reach_system_stats_nvml_session session;
    unsigned int count = 0;
    if (!session.initialized || session.count(&count) != 0)
        return;
    for (unsigned int index = 0; index < count; ++index)
    {
        void *device = nullptr;
        unsigned int temperature = 0;
        if (session.device(index, &device) == 0 &&
            session.temperature(device, 0, &temperature) == 0 && temperature < 150)
        {
            out_sample->gpu_temperature_c =
                std::max(out_sample->gpu_temperature_c, (float)temperature);
            out_sample->gpu_temperature_available = 1;
        }
    }
}


static void reach_system_stats_sample_gpu(reach_system_stats_source *source,
                                          reach_system_stats_sample *out_sample)
{
    if (source->gpu_query == nullptr && !source->gpu_probe_attempted)
    {
        source->gpu_probe_attempted = true;
        if (PdhOpenQueryW(nullptr, 0, &source->gpu_query) == ERROR_SUCCESS &&
            PdhAddEnglishCounterW(source->gpu_query,
                                  L"\\GPU Engine(*)\\Utilization Percentage", 0,
                                  &source->gpu_engine) != ERROR_SUCCESS)
        {
            PdhCloseQuery(source->gpu_query);
            source->gpu_query = nullptr;
        }
    }
    reach_system_stats_sample_gpu_engines(source, out_sample);
    reach_system_stats_sample_gpu_temperature(out_sample);
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
    if (source != nullptr)
    {
        if (source->gpu_query != nullptr)
            PdhCloseQuery(source->gpu_query);
        *source = {};
    }
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
    out_port->ops.sample_gpu = reach_system_stats_sample_gpu;
    out_port->ops.destroy = reach_system_stats_destroy;
    return REACH_OK;
}
