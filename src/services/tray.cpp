#include "reach/services/tray.h"

#include <new>

struct reach_tray_service
{
    reach_tray_provider_port provider;
    reach_tray_item items[REACH_MAX_TRAY_ITEMS];
    size_t item_count;
};

reach_result reach_tray_service_create(reach_tray_provider_port provider,
                                       reach_tray_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    *out_service = nullptr;
    reach_tray_service *service = new (std::nothrow) reach_tray_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }
    service->provider = provider;
    *out_service = service;
    return REACH_OK;
}

void reach_tray_service_destroy(reach_tray_service *service)
{
    if (service == nullptr)
    {
        return;
    }
    if (service->provider.ops.destroy != nullptr)
    {
        service->provider.ops.destroy(service->provider.provider);
    }
    delete service;
}

reach_result reach_tray_service_refresh(reach_tray_service *service)
{
    if (service == nullptr || service->provider.ops.refresh == nullptr ||
        service->provider.ops.item_count == nullptr || service->provider.ops.item_at == nullptr)
    {
        return REACH_OK;
    }
    reach_result result = service->provider.ops.refresh(service->provider.provider);
    if (result != REACH_OK)
    {
        return result;
    }
    size_t count = service->provider.ops.item_count(service->provider.provider);
    service->item_count = count < REACH_MAX_TRAY_ITEMS ? count : REACH_MAX_TRAY_ITEMS;
    for (size_t index = 0; index < service->item_count; ++index)
    {
        if (service->provider.ops.item_at(service->provider.provider, index,
                                          &service->items[index]) != REACH_OK)
        {
            service->items[index] = {};
        }
    }
    return REACH_OK;
}

int32_t reach_tray_service_needs_refresh(const reach_tray_service *service)
{
    return service != nullptr && service->provider.ops.needs_refresh != nullptr
               ? service->provider.ops.needs_refresh(service->provider.provider)
               : 0;
}

size_t reach_tray_service_item_count(const reach_tray_service *service)
{
    return service != nullptr ? service->item_count : 0;
}

const reach_tray_item *reach_tray_service_item_at(const reach_tray_service *service, size_t index)
{
    return service != nullptr && index < service->item_count ? &service->items[index] : nullptr;
}

reach_result reach_tray_service_activate(reach_tray_service *service, uint32_t item_id,
                                         reach_tray_action action)
{
    return service != nullptr && service->provider.ops.activate != nullptr
               ? service->provider.ops.activate(service->provider.provider, item_id, action)
               : REACH_OK;
}

int32_t reach_tray_service_take_retired_icon(reach_tray_service *service, uint64_t *out_icon_id)
{
    return service != nullptr && service->provider.ops.take_retired_icon != nullptr
               ? service->provider.ops.take_retired_icon(service->provider.provider, out_icon_id)
               : 0;
}

void reach_tray_service_release_retired_icon(reach_tray_service *service, uint64_t icon_id)
{
    if (service != nullptr && service->provider.ops.release_retired_icon != nullptr)
    {
        service->provider.ops.release_retired_icon(service->provider.provider, icon_id);
    }
}
