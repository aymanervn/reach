#ifndef REACH_SUPPORT_SEARCH_CATALOG_H
#define REACH_SUPPORT_SEARCH_CATALOG_H

#include <stddef.h>
#include <stdint.h>

#include "reach/support/search_types.h"

#ifdef __cplusplus
extern "C"
{
#endif

    typedef struct reach_search_alias_entry
    {
        const char *display;
        const char *target;
        const char *arguments;
        const char *aliases;
    } reach_search_alias_entry;

    size_t reach_search_alias_count(void);
    const reach_search_alias_entry *reach_search_alias_at(size_t index);
    reach_search_match_tier reach_search_alias_match(const reach_search_alias_entry *entry,
                                                     const uint16_t *query);

#ifdef __cplusplus
}
#endif

#endif
