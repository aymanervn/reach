#include "reach/platform/windows_adapters.h"

#include "reach/ports/tray_provider.h"

#include <windows.h>
#include <shellapi.h>

#include <new>

struct reach_tray_provider {
    int unused;
};

static reach_result reach_tray_refresh(reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    // Implement notification-area enumeration and cache stable tray item ids here.
    (void)provider;
    return REACH_NOT_IMPLEMENTED;
}

static size_t reach_tray_item_count(const reach_tray_provider *provider)
{
    REACH_ASSERT(provider != nullptr);
    (void)provider;
    return 0;
}

static reach_result reach_tray_item_at(const reach_tray_provider *provider, size_t index, reach_tray_item *out_item)
{
    REACH_ASSERT(provider != nullptr);
    REACH_ASSERT(out_item != nullptr);
    // Return cached tray item metadata without exposing HWND or shell structures.
    (void)provider;
    (void)index;
    (void)out_item;
    return REACH_NOT_IMPLEMENTED;
}

static reach_result reach_tray_open_menu(reach_tray_provider *provider, uint32_t item_id)
{
    REACH_ASSERT(provider != nullptr);
    // Forward the click/menu request to the owning notification icon window.
    (void)provider;
    (void)item_id;
    return REACH_NOT_IMPLEMENTED;
}

static void reach_tray_destroy(reach_tray_provider *provider)
{
    delete provider;
}

reach_result reach_windows_create_tray_provider(reach_tray_provider_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_tray_provider *provider = new (std::nothrow) reach_tray_provider();
    if (provider == nullptr) {
        return REACH_ERROR;
    }

    out_port->provider = provider;
    out_port->ops.refresh = reach_tray_refresh;
    out_port->ops.item_count = reach_tray_item_count;
    out_port->ops.item_at = reach_tray_item_at;
    out_port->ops.open_menu = reach_tray_open_menu;
    out_port->ops.destroy = reach_tray_destroy;
    return REACH_OK;
}
