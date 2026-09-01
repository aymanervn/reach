#include "reach/services/input_language.h"

#include <new>

// Cycling posts WM_INPUTLANGCHANGEREQUEST to another process, which applies it on its own thread,
// so the new layout is not readable at the moment of the request. This is the only window in which
// the service samples repeatedly; it closes as soon as the layout actually changes.
#define REACH_INPUT_LANGUAGE_SETTLE_SECONDS 1.0

struct reach_input_language_service
{
    reach_input_language_port source;
    reach_input_language_snapshot snapshot;
    double settle_seconds;
};

reach_result reach_input_language_service_create(reach_input_language_port source,
                                                 reach_input_language_service **out_service)
{
    if (out_service == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    reach_input_language_service *service = new (std::nothrow) reach_input_language_service();
    if (service == nullptr)
    {
        return REACH_ERROR;
    }

    service->source = source;
    *out_service = service;
    return REACH_OK;
}

void reach_input_language_service_destroy(reach_input_language_service *service)
{
    if (service != nullptr && service->source.ops.destroy != nullptr)
    {
        service->source.ops.destroy(service->source.language);
    }
    delete service;
}

int32_t reach_input_language_service_refresh(reach_input_language_service *service,
                                             reach_window_id foreground_window)
{
    if (service == nullptr || service->source.ops.get_state == nullptr)
    {
        return 0;
    }

    reach_input_language_state state = {};
    if (service->source.ops.get_state(service->source.language, foreground_window, &state) !=
        REACH_OK)
    {
        return 0;
    }

    int32_t changed = !service->snapshot.valid;
    for (size_t index = 0; index < 8 && !changed; ++index)
    {
        changed = service->snapshot.code[index] != state.code[index];
    }
    if (!changed)
    {
        return 0;
    }

    reach_copy_utf16(service->snapshot.code, 8, state.code);
    service->snapshot.valid = 1;
    service->settle_seconds = 0.0;
    return 1;
}

int32_t reach_input_language_service_settling(const reach_input_language_service *service)
{
    return service != nullptr && service->settle_seconds > 0.0;
}

int32_t reach_input_language_service_tick_settle(reach_input_language_service *service,
                                                 double delta_seconds,
                                                 reach_window_id foreground_window)
{
    if (!reach_input_language_service_settling(service))
    {
        return 0;
    }

    service->settle_seconds -= delta_seconds;
    if (service->settle_seconds < 0.0)
    {
        service->settle_seconds = 0.0;
    }
    return reach_input_language_service_refresh(service, foreground_window);
}

reach_result reach_input_language_service_cycle_next(reach_input_language_service *service,
                                                     reach_window_id foreground_window)
{
    if (service == nullptr || service->source.ops.cycle_next == nullptr)
    {
        return REACH_OK;
    }

    service->settle_seconds = REACH_INPUT_LANGUAGE_SETTLE_SECONDS;
    return service->source.ops.cycle_next(service->source.language, foreground_window);
}

void reach_input_language_service_snapshot_take(const reach_input_language_service *service,
                                                reach_input_language_snapshot *out_snapshot)
{
    if (out_snapshot == nullptr)
    {
        return;
    }
    *out_snapshot = service != nullptr ? service->snapshot : reach_input_language_snapshot{};
}
