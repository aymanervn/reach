#include "reach/platform/windows_adapters.h"

#include "reach/ports/render_backend.h"

#include <windows.h>
#include <d2d1_1.h>
#include <dcomp.h>
#include <dwrite.h>

#include <new>

struct reach_render_backend {
    HWND hwnd;
};

static reach_result reach_d2d_begin_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    // Begin the Direct2D target/frame associated with the selected platform surface.
    (void)backend;
    return REACH_NOT_IMPLEMENTED;
}

static reach_result reach_d2d_end_frame(reach_render_backend *backend)
{
    REACH_ASSERT(backend != nullptr);
    // End the Direct2D frame and handle target/device loss before returning.
    (void)backend;
    return REACH_NOT_IMPLEMENTED;
}

static reach_result reach_d2d_execute(reach_render_backend *backend, const reach_render_command_buffer *commands)
{
    REACH_ASSERT(backend != nullptr);
    REACH_ASSERT(commands != nullptr);
    // Translate render commands into rounded rects, text, icons, and blur/composition operations.
    (void)backend;
    (void)commands;
    return REACH_NOT_IMPLEMENTED;
}

static void reach_d2d_destroy(reach_render_backend *backend)
{
    delete backend;
}

reach_result reach_windows_create_d2d_render_backend(void *native_window, reach_render_backend_port *out_port)
{
    REACH_ASSERT(native_window != nullptr);
    REACH_ASSERT(out_port != nullptr);
    if (native_window == nullptr || out_port == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_render_backend *backend = new (std::nothrow) reach_render_backend();
    if (backend == nullptr) {
        return REACH_ERROR;
    }

    backend->hwnd = static_cast<HWND>(native_window);
    out_port->backend = backend;
    out_port->ops.begin_frame = reach_d2d_begin_frame;
    out_port->ops.end_frame = reach_d2d_end_frame;
    out_port->ops.execute = reach_d2d_execute;
    out_port->ops.destroy = reach_d2d_destroy;
    return REACH_OK;
}
