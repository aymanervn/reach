#ifndef REACH_PORTS_TRAY_PROVIDER_H
#define REACH_PORTS_TRAY_PROVIDER_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_tray_provider reach_tray_provider;

#define REACH_MAX_TRAY_ITEMS 64

typedef enum reach_tray_action {
    REACH_TRAY_ACTION_LEFT_CLICK = 1,
    REACH_TRAY_ACTION_RIGHT_CLICK = 2
} reach_tray_action;

typedef struct reach_tray_item {
    uint32_t id;
    uint16_t title[128];
    uint16_t icon_ref[260];
    uint64_t icon_id;
} reach_tray_item;

typedef struct reach_tray_provider_ops {
    reach_result (*refresh)(reach_tray_provider *provider);
    size_t (*item_count)(const reach_tray_provider *provider);
    reach_result (*item_at)(const reach_tray_provider *provider, size_t index, reach_tray_item *out_item);
    reach_result (*activate)(reach_tray_provider *provider, uint32_t item_id, reach_tray_action action);
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
