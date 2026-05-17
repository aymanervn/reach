#ifndef REACH_SEARCH_H
#define REACH_SEARCH_H

#include <stddef.h>
#include <stdint.h>

#include "reach/util.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct reach_search reach_search;

reach_result reach_search_create(reach_search **out_search);
void reach_search_destroy(reach_search *search);
reach_result reach_search_query(reach_search *search, const uint16_t *query);
size_t reach_search_result_count(const reach_search *search);

#ifdef __cplusplus
}
#endif

#endif
