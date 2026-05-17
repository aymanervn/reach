#include "reach/config.h"

#include <windows.h>
#include <shlwapi.h>

struct reach_config {
    int unused;
};

reach_result reach_config_create(reach_config **out_config)
{
    if (out_config == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    // Legacy module is kept only until callers migrate to reach_config_store_port.
    *out_config = nullptr;
    return REACH_NOT_IMPLEMENTED;
}

void reach_config_destroy(reach_config *config)
{
    (void)config;
}

reach_result reach_config_load_ini(reach_config *config, const uint16_t *path)
{
    // Legacy module is kept only until callers migrate to reach_config_store_port.
    (void)config;
    (void)path;
    return REACH_NOT_IMPLEMENTED;
}

reach_result reach_config_save_ini(const reach_config *config, const uint16_t *path)
{
    // Legacy module is kept only until callers migrate to reach_config_store_port.
    (void)config;
    (void)path;
    return REACH_NOT_IMPLEMENTED;
}

reach_result reach_config_get_dock(const reach_config *config, reach_dock_config *out_config)
{
    // Legacy module is kept only until callers migrate to reach_config_store_port.
    (void)config;
    (void)out_config;
    return REACH_NOT_IMPLEMENTED;
}
