#ifndef REACH_PORTS_FOREGROUND_WATCHER_H
#define REACH_PORTS_FOREGROUND_WATCHER_H

#include <stdint.h>

#include "reach/core/window_id.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_foreground_watcher reach_foreground_watcher;

    typedef void (*reach_foreground_watcher_callback)(void *user);

    typedef struct reach_foreground_watcher_ops
    {
        reach_result (*start)(reach_foreground_watcher *watcher,
                              reach_foreground_watcher_callback callback, void *user);

        reach_result (*stop)(reach_foreground_watcher *watcher);

        reach_window_id (*foreground)(const reach_foreground_watcher *watcher);

        void (*destroy)(reach_foreground_watcher *watcher);
    } reach_foreground_watcher_ops;

    typedef struct reach_foreground_watcher_port
    {
        reach_foreground_watcher *watcher;
        reach_foreground_watcher_ops ops;
    } reach_foreground_watcher_port;

#ifdef __cplusplus
}
#endif

#endif
