#ifndef REACH_PORTS_CLIPBOARD_H
#define REACH_PORTS_CLIPBOARD_H

#include "reach/core/clipboard.h"
#include "reach/support/util.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_clipboard_provider reach_clipboard_provider;
    typedef void (*reach_clipboard_changed_callback)(void *user);

    typedef struct reach_clipboard_provider_ops
    {
        reach_result (*start)(reach_clipboard_provider *provider,
                              reach_clipboard_changed_callback callback, void *user);
        reach_result (*stop)(reach_clipboard_provider *provider);
        reach_result (*capture_current)(reach_clipboard_provider *provider,
                                        reach_clipboard_item *out_item);
        reach_result (*restore)(reach_clipboard_provider *provider, uint64_t item_id);
        void (*release)(reach_clipboard_provider *provider, uint64_t item_id);
        void (*destroy)(reach_clipboard_provider *provider);
    } reach_clipboard_provider_ops;

    typedef struct reach_clipboard_port
    {
        reach_clipboard_provider *provider;
        reach_clipboard_provider_ops ops;
    } reach_clipboard_port;

#ifdef __cplusplus
}
#endif

#endif
