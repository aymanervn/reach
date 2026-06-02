#include "reach/features/tray.h"

static size_t reach_tray_min_size(size_t a, size_t b)
{
    return a < b ? a : b;
}

void reach_tray_model_init(reach_tray_model *model)
{
    if (model == nullptr)
    {
        return;
    }

    *model = {};
}

reach_result reach_tray_model_refresh(reach_tray_model *model, reach_tray_provider_port *provider)
{
    if (model == nullptr || provider == nullptr || provider->ops.refresh == nullptr ||
        provider->ops.item_count == nullptr || provider->ops.item_at == nullptr)
    {
        return REACH_OK;
    }

    reach_result result = provider->ops.refresh(provider->provider);
    if (result != REACH_OK)
    {
        return result;
    }

    size_t count = provider->ops.item_count(provider->provider);
    model->item_count = reach_tray_min_size(count, REACH_MAX_TRAY_ITEMS);
    for (size_t index = 0; index < model->item_count; ++index)
    {
        if (provider->ops.item_at(provider->provider, index, &model->items[index]) != REACH_OK)
        {
            model->items[index] = {};
        }
    }
    return REACH_OK;
}
