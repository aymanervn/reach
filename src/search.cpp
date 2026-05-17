#include "reach/search.h"

struct reach_search {
    int unused;
};

reach_result reach_search_create(reach_search **out_search)
{
    if (out_search == nullptr) {
        return REACH_INVALID_ARGUMENT;
    }

    // Legacy module is kept only until callers migrate to reach_search_provider_port.
    *out_search = nullptr;
    return REACH_NOT_IMPLEMENTED;
}

void reach_search_destroy(reach_search *search)
{
    (void)search;
}

reach_result reach_search_query(reach_search *search, const uint16_t *query)
{
    // Legacy module is kept only until callers migrate to reach_search_provider_port.
    (void)search;
    (void)query;
    return REACH_NOT_IMPLEMENTED;
}

size_t reach_search_result_count(const reach_search *search)
{
    (void)search;
    return 0;
}
