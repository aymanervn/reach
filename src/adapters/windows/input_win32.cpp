#include "reach/platform/windows_adapters.h"

#include "reach/ports/input_source.h"

#include <windows.h>

#include <new>

struct reach_input_source {
    reach_input_event_callback callback;
    void *user;
};

static reach_result reach_input_start(reach_input_source *source, reach_input_event_callback callback, void *user)
{
    REACH_ASSERT(source != nullptr);
    REACH_ASSERT(callback != nullptr);
    if (source == nullptr || callback == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    source->callback = callback;
    source->user = user;
    // Hook WM_HOTKEY, WM_CHAR, mouse hit tests, and Windows-key handling into this source.
    return REACH_OK;
}

static reach_result reach_input_stop(reach_input_source *source)
{
    REACH_ASSERT(source != nullptr);
    if (source == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    source->callback = nullptr;
    source->user = nullptr;
    return REACH_OK;
}

static void reach_input_destroy(reach_input_source *source)
{
    delete source;
}

reach_result reach_windows_create_input_source(reach_input_source_port *out_port)
{
    REACH_ASSERT(out_port != nullptr);
    if (out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_input_source *source = new (std::nothrow) reach_input_source();
    if (source == nullptr) {
        return REACH_ERROR;
    }

    out_port->source = source;
    out_port->ops.start = reach_input_start;
    out_port->ops.stop = reach_input_stop;
    out_port->ops.destroy = reach_input_destroy;
    return REACH_OK;
}
