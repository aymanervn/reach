#ifndef REACH_APPLIST_H
#define REACH_APPLIST_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_app_entry {
    uint16_t name[128];
    uint16_t path[260];
    uint16_t arguments[260];
    uint16_t icon_path[260];
} reach_app_entry;

typedef struct reach_applist reach_applist;

reach_result reach_applist_create(reach_applist **out_list);
void reach_applist_destroy(reach_applist *list);
reach_result reach_applist_load_from_config(reach_applist *list, const uint16_t *config_path);
size_t reach_applist_count(const reach_applist *list);
const reach_app_entry *reach_applist_get(const reach_applist *list, size_t index);

#ifdef __cplusplus
}
#endif

#endif
