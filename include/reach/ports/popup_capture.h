#ifndef REACH_PORTS_POPUP_CAPTURE_H
#define REACH_PORTS_POPUP_CAPTURE_H

#include "reach/ports/platform_window.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef void (*reach_popup_capture_mouse_down_callback)(
    void *userdata,
    int32_t x,
    int32_t y
);

typedef struct reach_popup_capture_port {
    void *userdata;

    reach_result (*begin_capture)(
        void *userdata,
        reach_platform_window *surface
    );

    void (*end_capture)(
        void *userdata,
        reach_platform_window *surface
    );

    int32_t (*is_capture_active)(
        void *userdata,
        reach_platform_window *surface
    );

    reach_result (*sync_mouse_hook)(
        void *userdata,
        int32_t should_hook,
        reach_popup_capture_mouse_down_callback callback,
        void *callback_userdata
    );

    void (*destroy)(
        void *userdata
    );
} reach_popup_capture_port;

#ifdef __cplusplus
}
#endif

#endif
