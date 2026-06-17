#ifndef REACH_ADAPTERS_WINDOWS_SYSTEM_CONTROLS_NETWORK_H
#define REACH_ADAPTERS_WINDOWS_SYSTEM_CONTROLS_NETWORK_H

#include "reach/ports/system_controls.h"

#include <stddef.h>
#include <stdint.h>

#define REACH_WINDOWS_NETWORK_IF_TYPE_ETHERNET 6u
#define REACH_WINDOWS_NETWORK_IF_TYPE_WIFI 71u

typedef struct reach_windows_network_adapter
{
    uint32_t if_type;
    int32_t oper_up;
    int32_t has_unicast;
} reach_windows_network_adapter;

reach_network_kind
reach_windows_network_kind_from_adapters(const reach_windows_network_adapter *adapters,
                                         size_t adapter_count);

#endif
