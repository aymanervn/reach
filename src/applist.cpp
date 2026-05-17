#include "reach/applist.h"

#include <windows.h>
#include <shellapi.h>
#include <shlwapi.h>

struct reach_applist {
    int unused;
};

reach_result reach_applist_create(reach_applist **out_list)
{
    if (out_list == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    // Legacy module is kept only until config_store supplies pinned apps to the core model.
    *out_list = nullptr;
    return REACH_NOT_IMPLEMENTED;
}

void reach_applist_destroy(reach_applist *list)
{
    (void)list;
}

reach_result reach_applist_load_from_config(reach_applist *list, const uint16_t *config_path)
{
    // Legacy module is kept only until config_store supplies pinned apps to the core model.
    (void)list;
    (void)config_path;
    return REACH_NOT_IMPLEMENTED;
}

size_t reach_applist_count(const reach_applist *list)
{
    (void)list;
    return 0;
}

const reach_app_entry *reach_applist_get(const reach_applist *list, size_t index)
{
    (void)list;
    (void)index;
    return nullptr;
}
