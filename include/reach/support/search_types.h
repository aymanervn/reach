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
#define REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY 128
#define REACH_SEARCH_VISIBLE_RESULTS 5
#define REACH_SEARCH_SCROLL_STEP 4
#define REACH_SEARCH_MAX_RESULTS 50
#define REACH_SEARCH_RANK_CANDIDATE_CAP 1024

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

    typedef enum reach_search_source
    {
        REACH_SEARCH_SOURCE_INDEX = 0,
        REACH_SEARCH_SOURCE_CATALOG = 1
    } reach_search_source;

    typedef enum reach_search_match_tier
    {
        REACH_SEARCH_MATCH_NONE = 0,
        REACH_SEARCH_MATCH_PATH = 1,
        REACH_SEARCH_MATCH_SUBSTRING = 2,
        REACH_SEARCH_MATCH_WORD_PREFIX = 3,
        REACH_SEARCH_MATCH_PREFIX = 4,
        REACH_SEARCH_MATCH_EXACT = 5
    } reach_search_match_tier;

    typedef enum reach_search_launch_class
    {
        REACH_SEARCH_LAUNCH_NONE = 0,
        REACH_SEARCH_LAUNCH_SCRIPT = 1,
        REACH_SEARCH_LAUNCH_CPL = 2,
        REACH_SEARCH_LAUNCH_MSI = 3,
        REACH_SEARCH_LAUNCH_MSC = 4,
        REACH_SEARCH_LAUNCH_EXE = 5
    } reach_search_launch_class;

    typedef enum reach_search_location_rank
    {
        REACH_SEARCH_LOCATION_TRANSIENT = 0,
        REACH_SEARCH_LOCATION_OTHER = 1,
        REACH_SEARCH_LOCATION_PROGRAMS = 2,
        REACH_SEARCH_LOCATION_SYSTEM = 3
    } reach_search_location_rank;

    typedef struct reach_search_candidate
    {
        uint16_t name[REACH_SEARCH_RESULT_NAME_CAPACITY];
        uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY];
        uint16_t arguments[REACH_SEARCH_RESULT_ARGUMENTS_CAPACITY];
        reach_search_result_kind kind;
        int32_t source;
        int32_t match_tier;
        int32_t pinned;
        int32_t is_directory;
        int32_t score;
    } reach_search_candidate;

    typedef struct reach_search_rank_key
    {
        int32_t pinned;
        int32_t launchable;
        int32_t match_tier;
        int32_t stem_length;
        int32_t launch_class;
        int32_t location;
    } reach_search_rank_key;

    reach_search_result_kind reach_search_classify_result(const uint16_t *path,
                                                          int32_t is_directory);

    const uint16_t *reach_search_extension(const uint16_t *path);
    size_t reach_search_match_stem_length(const uint16_t *name);
    reach_search_launch_class reach_search_classify_launch(const uint16_t *path,
                                                           int32_t is_directory);
    reach_search_match_tier reach_search_match_name(const uint16_t *name, const uint16_t *query);
    reach_search_location_rank reach_search_classify_location(const uint16_t *path);

    void reach_search_build_rank_key(const uint16_t *query, const reach_search_candidate *candidate,
                                     reach_search_rank_key *out_key);
    int32_t reach_search_compare_rank_keys(const reach_search_rank_key *a,
                                           const reach_search_rank_key *b);

    size_t reach_search_rank_candidates(const uint16_t *query, reach_search_candidate *candidates,
                                        size_t candidate_count, size_t max_results);

#ifdef __cplusplus
}
#endif

#endif
