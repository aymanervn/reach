#ifndef REACH_PORTS_TRAY_PROVIDER_H
#define REACH_PORTS_TRAY_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_tray_provider reach_tray_provider;

typedef struct reach_tray_item {
    uint32_t id;
    uint16_t title[128];
    uint16_t icon_ref[260];
} reach_tray_item;

typedef struct reach_tray_provider_ops {
    reach_result (*refresh)(reach_tray_provider *provider);
    size_t (*item_count)(const reach_tray_provider *provider);
    reach_result (*item_at)(const reach_tray_provider *provider, size_t index, reach_tray_item *out_item);
    reach_result (*open_menu)(reach_tray_provider *provider, uint32_t item_id);
    void (*destroy)(reach_tray_provider *provider);
} reach_tray_provider_ops;

typedef struct reach_tray_provider_port {
    reach_tray_provider *provider;
    reach_tray_provider_ops ops;
} reach_tray_provider_port;

#ifdef __cplusplus
}
#endif

#endif
