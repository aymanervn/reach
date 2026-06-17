#include "system_controls_network.h"

static int32_t reach_windows_network_adapter_connected(const reach_windows_network_adapter *adapter)
{
    return adapter != nullptr && adapter->oper_up && adapter->has_unicast;
}

reach_network_kind
reach_windows_network_kind_from_adapters(const reach_windows_network_adapter *adapters,
                                         size_t adapter_count)
{
    if (adapters == nullptr)
    {
        return REACH_NETWORK_KIND_NONE;
    }

    int32_t has_ethernet = 0;
    for (size_t index = 0; index < adapter_count; ++index)
    {
        const reach_windows_network_adapter *adapter = &adapters[index];
        if (!reach_windows_network_adapter_connected(adapter))
        {
            continue;
        }
        if (adapter->if_type == REACH_WINDOWS_NETWORK_IF_TYPE_WIFI)
        {
            return REACH_NETWORK_KIND_WIFI;
        }
        if (adapter->if_type == REACH_WINDOWS_NETWORK_IF_TYPE_ETHERNET)
        {
            has_ethernet = 1;
        }
    }

    return has_ethernet ? REACH_NETWORK_KIND_ETHERNET : REACH_NETWORK_KIND_NONE;
}
