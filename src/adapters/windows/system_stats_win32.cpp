#include "windows_adapters_internal.h"

#include <winsock2.h>
#include <windows.h>
#include <iphlpapi.h>
#include <ws2tcpip.h>

#include <winternl.h>
#include <d3dkmthk.h>
#include <dxgi1_4.h>
#include <wrl/client.h>
#include <pdh.h>
#include <pdhmsg.h>
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <cwchar>
#include <map>
#include <string>
#include <vector>

#pragma comment(lib, "gdi32.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "pdh.lib")
#pragma comment(lib, "iphlpapi.lib")
#pragma comment(lib, "ws2_32.lib")

struct reach_system_stats_source
{
    PDH_HQUERY monitor_query;
    PDH_HCOUNTER gpu_engine;
    PDH_HCOUNTER gpu_memory;
    Microsoft::WRL::ComPtr<IDXGIFactory4> dxgi_factory;
    PDH_HCOUNTER cpu_frequency;
    bool monitor_probe_attempted;
    uint32_t memory_speed_mt_s;
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

template <typename T>
static T reach_system_stats_read_firmware_value(const BYTE *record, size_t offset)
{
    T value = {};
    std::memcpy(&value, record + offset, sizeof(value));
    return value;
}

static uint32_t reach_system_stats_read_memory_speed()
{
    constexpr DWORD raw_smbios = 0x52534d42;
    constexpr size_t header_size = 8;
    const UINT size = GetSystemFirmwareTable(raw_smbios, 0, nullptr, 0);
    if (size < header_size)
    {
        return 0;
    }
    std::vector<BYTE> firmware(size);
    if (GetSystemFirmwareTable(raw_smbios, 0, firmware.data(), size) != size)
    {
        return 0;
    }
    const auto table_size = reach_system_stats_read_firmware_value<uint32_t>(firmware.data(), 4);
    if (table_size > size - header_size)
    {
        return 0;
    }
    uint32_t memory_speed = 0;
    const BYTE *record = firmware.data() + header_size;
    const BYTE *end = record + table_size;
    while (end - record >= 4 && record[0] != 127)
    {
        const size_t length = record[1];
        if (length < 4 || length > (size_t)(end - record))
        {
            break;
        }
        if (record[0] == 17 && length >= 0x22 &&
            reach_system_stats_read_firmware_value<uint16_t>(record, 0x0c) != 0)
        {
            uint32_t speed = reach_system_stats_read_firmware_value<uint16_t>(record, 0x20);
            if (speed == 0xffff)
            {
                speed = length >= 0x5c
                            ? reach_system_stats_read_firmware_value<uint32_t>(record, 0x58)
                            : 0;
            }
            if (speed != 0)
            {
                memory_speed = memory_speed == 0 ? speed : std::min(memory_speed, speed);
            }
        }
        record += length;
        while (end - record >= 2 && (record[0] != 0 || record[1] != 0))
        {
            ++record;
        }
        if (end - record < 2)
        {
            break;
        }
        record += 2;
    }
    return memory_speed;
}

static uint32_t reach_system_stats_read_gpu_clock(const wchar_t *identity)
{
    unsigned long high = 0;
    unsigned long low = 0;
    D3DKMT_NODE_PERFDATA frequency = {};
    if (swscanf_s(identity, L"luid_0x%lx_0x%lx_phys_%u_eng_%u", &high, &low,
                  &frequency.PhysicalAdapterIndex, &frequency.NodeOrdinal) != 4)
    {
        return 0;
    }
    D3DKMT_OPENADAPTERFROMLUID adapter = {};
    adapter.AdapterLuid.HighPart = (LONG)high;
    adapter.AdapterLuid.LowPart = low;
    if (D3DKMTOpenAdapterFromLuid(&adapter) != 0)
    {
        return 0;
    }
    D3DKMT_QUERYADAPTERINFO query = {};
    query.hAdapter = adapter.hAdapter;
    query.Type = KMTQAITYPE_NODEPERFDATA;
    query.pPrivateDriverData = &frequency;
    query.PrivateDriverDataSize = sizeof(frequency);
    const auto status = D3DKMTQueryAdapterInfo(&query);
    D3DKMT_CLOSEADAPTER close = {adapter.hAdapter};
    D3DKMTCloseAdapter(&close);
    return status == 0 ? (uint32_t)(frequency.Frequency / 1'000'000u) : 0;
}

static bool reach_system_stats_read_counter_array(PDH_HCOUNTER counter, DWORD format,
                                                   std::vector<BYTE> &buffer, DWORD &count)
{
    DWORD bytes = 0;
    if (counter == nullptr ||
        PdhGetFormattedCounterArrayW(counter, format, &bytes, &count, nullptr) != PDH_MORE_DATA ||
        bytes == 0)
    {
        return false;
    }
    buffer.resize(bytes);
    return PdhGetFormattedCounterArrayW(
               counter, format, &bytes, &count,
               reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data())) == ERROR_SUCCESS;
}

static void reach_system_stats_sample_vram(reach_system_stats_source *source, LUID luid,
                                           reach_system_stats_sample *sample)
{
    std::vector<BYTE> buffer;
    DWORD count = 0;
    if (!reach_system_stats_read_counter_array(source->gpu_memory, PDH_FMT_LARGE, buffer, count))
    {
        return;
    }
    const auto *items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
    uint64_t used = 0;
    bool found = false;
    for (DWORD index = 0; index < count; ++index)
    {
        const auto &item = items[index];
        unsigned long high = 0;
        unsigned long low = 0;
        if ((item.FmtValue.CStatus == PDH_CSTATUS_VALID_DATA ||
             item.FmtValue.CStatus == PDH_CSTATUS_NEW_DATA) &&
            item.FmtValue.largeValue >= 0 && item.szName != nullptr &&
            swscanf_s(item.szName, L"luid_0x%lx_0x%lx_phys_", &high, &low) == 2 &&
            (LONG)high == luid.HighPart && low == luid.LowPart)
        {
            used += (uint64_t)item.FmtValue.largeValue;
            found = true;
        }
    }
    if (!found)
    {
        return;
    }

    if (source->dxgi_factory != nullptr && !source->dxgi_factory->IsCurrent())
    {
        source->dxgi_factory.Reset();
    }
    if (source->dxgi_factory == nullptr &&
        FAILED(CreateDXGIFactory1(IID_PPV_ARGS(source->dxgi_factory.ReleaseAndGetAddressOf()))))
    {
        return;
    }
    Microsoft::WRL::ComPtr<IDXGIAdapter1> adapter;
    DXGI_ADAPTER_DESC1 desc = {};
    if (SUCCEEDED(source->dxgi_factory->EnumAdapterByLuid(luid, IID_PPV_ARGS(adapter.GetAddressOf()))) &&
        SUCCEEDED(adapter->GetDesc1(&desc)) && desc.DedicatedVideoMemory != 0)
    {
        sample->vram_used_bytes = used;
        sample->vram_total_bytes = desc.DedicatedVideoMemory;
    }
}

static void reach_system_stats_sample_gpu_temperature(LUID luid, UINT physical,
                                                     reach_system_stats_sample *sample);

static void reach_system_stats_sample_gpu_engines(reach_system_stats_source *source,
                                                  reach_system_stats_sample *out_sample)
{
    std::vector<BYTE> buffer;
    DWORD count = 0;
    if (!reach_system_stats_read_counter_array(source->gpu_engine, PDH_FMT_DOUBLE, buffer, count))
    {
        return;
    }
    const auto *items = reinterpret_cast<const PDH_FMT_COUNTERVALUE_ITEM_W *>(buffer.data());
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
    const auto most_used = std::max_element(engines.begin(), engines.end(),
        [](const auto &a, const auto &b) { return a.second < b.second; });
    if (most_used != engines.end())
    {
        out_sample->gpu_percent = (float)std::clamp(most_used->second, 0.0, 100.0);
        out_sample->gpu_available = 1;
        const auto &identity = most_used->first;
        unsigned long high = 0;
        unsigned long low = 0;
        UINT physical = 0;
        if (swscanf_s(identity.c_str(), L"luid_0x%lx_0x%lx_phys_%u", &high, &low, &physical) == 3)
        {
            const LUID luid = {low, (LONG)high};
            reach_system_stats_sample_vram(source, luid, out_sample);
            reach_system_stats_sample_gpu_temperature(luid, physical, out_sample);
        }
        const size_t engine_offset = identity.find(L"_eng_");
        if (engine_offset != std::wstring::npos)
        {
            const auto graphics = std::find_if(engines.begin(), engines.end(), [&](const auto &item)
            {
                const size_t kind = item.first.find(L"_engtype_", engine_offset);
                return kind != std::wstring::npos &&
                       item.first.compare(0, engine_offset, identity, 0, engine_offset) == 0 &&
                       _wcsicmp(item.first.c_str() + kind, L"_engtype_3d") == 0;
            });
            if (graphics != engines.end())
            {
                out_sample->gpu_clock_mhz =
                    reach_system_stats_read_gpu_clock(graphics->first.c_str());
            }
            if (out_sample->gpu_clock_mhz == 0 && graphics != most_used)
            {
                out_sample->gpu_clock_mhz = reach_system_stats_read_gpu_clock(identity.c_str());
            }
        }
    }
}

struct reach_system_stats_nvml_session
{
    using initialize_fn = int (*)();
    using device_fn = int (*)(const char *, void **);
    using temperature_fn = int (*)(void *, unsigned int, unsigned int *);
    HMODULE library = nullptr;
    initialize_fn shutdown = nullptr;
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
        device =
            reinterpret_cast<device_fn>(GetProcAddress(library, "nvmlDeviceGetHandleByPciBusId_v2"));
        temperature =
            reinterpret_cast<temperature_fn>(GetProcAddress(library, "nvmlDeviceGetTemperature"));
        initialized = initialize != nullptr && shutdown != nullptr &&
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

static void reach_system_stats_sample_gpu_temperature(LUID luid, UINT physical,
                                                     reach_system_stats_sample *sample)
{
    D3DKMT_OPENADAPTERFROMLUID adapter = {};
    adapter.AdapterLuid = luid;
    if (D3DKMTOpenAdapterFromLuid(&adapter) != 0)
    {
        return;
    }
    D3DKMT_ADAPTER_PERFDATA performance = {};
    performance.PhysicalAdapterIndex = physical;
    D3DKMT_QUERYADAPTERINFO query = {adapter.hAdapter, KMTQAITYPE_ADAPTERPERFDATA,
                                    &performance, sizeof(performance)};
    const auto status = D3DKMTQueryAdapterInfo(&query);

    D3DKMT_ADAPTERADDRESS address = {};
    bool has_address = false;
    if (status == 0 && performance.Temperature > 0 && performance.Temperature < 1500)
    {
        sample->gpu_temperature_c = performance.Temperature / 10.0f;
        sample->gpu_temperature_available = 1;
    }
    else if (physical == 0)
    {
        query = {adapter.hAdapter, KMTQAITYPE_ADAPTERADDRESS, &address, sizeof(address)};
        has_address = D3DKMTQueryAdapterInfo(&query) == 0;
    }
    D3DKMT_CLOSEADAPTER close = {adapter.hAdapter};
    D3DKMTCloseAdapter(&close);

    if (has_address)
    {
        thread_local reach_system_stats_nvml_session session;
        if (session.initialized)
        {
            char pci_id[24] = {};
            sprintf_s(pci_id, "0000:%02x:%02x.%x", (unsigned int)address.BusNumber,
                      (unsigned int)address.DeviceNumber, (unsigned int)address.FunctionNumber);
            void *device = nullptr;
            unsigned int temperature = 0;
            if (session.device(pci_id, &device) == 0 &&
                session.temperature(device, 0, &temperature) == 0 && temperature < 150)
            {
                sample->gpu_temperature_c = (float)temperature;
                sample->gpu_temperature_available = 1;
            }
        }
    }
}

static void reach_system_stats_sample_monitor(reach_system_stats_source *source,
                                              reach_system_stats_sample *out_sample)
{
    if (!source->monitor_probe_attempted)
    {
        source->monitor_probe_attempted = true;
        source->memory_speed_mt_s = reach_system_stats_read_memory_speed();
        if (PdhOpenQueryW(nullptr, 0, &source->monitor_query) == ERROR_SUCCESS)
        {
            (void)PdhAddEnglishCounterW(source->monitor_query,
                                        L"\\GPU Engine(*)\\Utilization Percentage", 0,
                                        &source->gpu_engine);
            (void)PdhAddEnglishCounterW(source->monitor_query,
                                        L"\\GPU Adapter Memory(*)\\Dedicated Usage", 0,
                                        &source->gpu_memory);
            (void)PdhAddEnglishCounterW(source->monitor_query,
                                        L"\\Processor Information(_Total)\\Processor Frequency", 0,
                                        &source->cpu_frequency);
        }
    }

    out_sample->memory_speed_mt_s = source->memory_speed_mt_s;
    if (source->monitor_query != nullptr &&
        PdhCollectQueryData(source->monitor_query) == ERROR_SUCCESS)
    {
        reach_system_stats_sample_gpu_engines(source, out_sample);
        PDH_FMT_COUNTERVALUE frequency = {};
        if (source->cpu_frequency != nullptr &&
            PdhGetFormattedCounterValue(source->cpu_frequency, PDH_FMT_LONG, nullptr,
                                        &frequency) == ERROR_SUCCESS &&
            (frequency.CStatus == PDH_CSTATUS_VALID_DATA ||
             frequency.CStatus == PDH_CSTATUS_NEW_DATA) &&
            frequency.longValue > 0)
        {
            out_sample->cpu_clock_mhz = (uint32_t)frequency.longValue;
        }
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
    if (source != nullptr)
    {
        if (source->monitor_query != nullptr)
            PdhCloseQuery(source->monitor_query);
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
    out_port->ops.sample_monitor = reach_system_stats_sample_monitor;
    out_port->ops.destroy = reach_system_stats_destroy;
    return REACH_OK;
}
