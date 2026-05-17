#include "reach/platform/windows_adapters.h"

#include "reach/ports/config_store.h"

#include <windows.h>
#include <shlwapi.h>

#include <new>

struct reach_config_store {
    uint16_t path[260];
};

static reach_result reach_config_store_load(reach_config_store *store, reach_config_snapshot *out_snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(out_snapshot != nullptr);
    // Read dock settings and up to REACH_MAX_PINNED_APPS app records from the INI file.
    (void)store;
    (void)out_snapshot;
    return REACH_NOT_IMPLEMENTED;
}

static reach_result reach_config_store_save(reach_config_store *store, const reach_config_snapshot *snapshot)
{
    REACH_ASSERT(store != nullptr);
    REACH_ASSERT(snapshot != nullptr);
    // Persist the same snapshot shape used by load; do not write adapter-specific state.
    (void)store;
    (void)snapshot;
    return REACH_NOT_IMPLEMENTED;
}

static void reach_config_store_destroy(reach_config_store *store)
{
    delete store;
}

reach_result reach_windows_create_config_store(const uint16_t *path, reach_config_store_port *out_port)
{
    REACH_ASSERT(path != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (path == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_config_store *store = new (std::nothrow) reach_config_store();
    if (store == nullptr) {
        return REACH_ERROR;
    }

    reach_copy_utf16(store->path, 260, path);
    out_port->store = store;
    out_port->ops.load = reach_config_store_load;
    out_port->ops.save = reach_config_store_save;
    out_port->ops.destroy = reach_config_store_destroy;
    return REACH_OK;
}
