#ifndef REACH_SUPPORT_SEARCH_TYPES_H
#define REACH_SUPPORT_SEARCH_TYPES_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

#define REACH_SEARCH_QUERY_CAPACITY 256
#define REACH_SEARCH_RESULT_NAME_CAPACITY 128
#define REACH_SEARCH_RESULT_PATH_CAPACITY 260
#define REACH_SEARCH_MAX_RESULTS 5
#define REACH_SEARCH_CANDIDATE_MAX 128

    typedef enum reach_search_result_kind
    {
        REACH_SEARCH_RESULT_APP = 0,
        REACH_SEARCH_RESULT_FOLDER = 1,
        REACH_SEARCH_RESULT_PHOTO = 2,
        REACH_SEARCH_RESULT_VIDEO = 3,
        REACH_SEARCH_RESULT_MUSIC = 4,
        REACH_SEARCH_RESULT_DOCUMENT = 5,
        REACH_SEARCH_RESULT_FILE = 6
    } reach_search_result_kind;

    typedef struct reach_search_candidate
    {
        uint16_t name[REACH_SEARCH_RESULT_NAME_CAPACITY];
        uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY];
        reach_search_result_kind kind;
        int32_t is_directory;
        int32_t score;
    } reach_search_candidate;

    reach_search_result_kind reach_search_classify_result(const uint16_t *path,
                                                          int32_t is_directory);
    int32_t reach_search_score_candidate(const uint16_t *query,
                                         const reach_search_candidate *candidate);
    size_t reach_search_rank_candidates(const uint16_t *query, reach_search_candidate *candidates,
                                        size_t candidate_count, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif
