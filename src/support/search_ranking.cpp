#include "reach/support/search_types.h"

static uint16_t reach_search_ascii_lower(uint16_t ch)
{
    return ch >= 'A' && ch <= 'Z' ? (uint16_t)(ch - 'A' + 'a') : ch;
}

static int32_t reach_search_char_equal_ci(uint16_t a, uint16_t b)
{
    return reach_search_ascii_lower(a) == reach_search_ascii_lower(b);
}

static size_t reach_search_strlen(const uint16_t *text)
{
    size_t length = 0;
    if (text == nullptr)
    {
        return 0;
    }
    while (text[length] != 0)
    {
        ++length;
    }
    return length;
}

static const uint16_t *reach_search_basename(const uint16_t *path)
{
    const uint16_t *name = path;
    if (path == nullptr)
    {
        return nullptr;
    }
    for (const uint16_t *scan = path; *scan != 0; ++scan)
    {
        if (*scan == '\\' || *scan == '/')
        {
            name = scan + 1;
        }
    }
    return name;
}

const uint16_t *reach_search_extension(const uint16_t *path)
{
    const uint16_t *name = reach_search_basename(path);
    const uint16_t *extension = nullptr;
    if (name == nullptr)
    {
        return nullptr;
    }
    for (const uint16_t *scan = name; *scan != 0; ++scan)
    {
        if (*scan == '.')
        {
            extension = scan;
        }
    }
    return extension;
}

size_t reach_search_match_stem_length(const uint16_t *name)
{
    size_t length = 0;
    if (name == nullptr)
    {
        return 0;
    }
    while (name[length] != 0 && name[length] != '.')
    {
        ++length;
    }
    return length;
}

static int32_t reach_search_extension_is(const uint16_t *extension, const char *expected)
{
    if (extension == nullptr || expected == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (extension[index] != 0 && expected[index] != 0)
    {
        if (!reach_search_char_equal_ci(extension[index], (uint16_t)expected[index]))
        {
            return 0;
        }
        ++index;
    }
    return extension[index] == 0 && expected[index] == 0;
}

static int32_t reach_search_extension_in(const uint16_t *extension, const char *const *values,
                                         size_t count)
{
    for (size_t index = 0; index < count; ++index)
    {
        if (reach_search_extension_is(extension, values[index]))
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_search_equals_ci(const uint16_t *text, const uint16_t *query)
{
    if (text == nullptr || query == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (text[index] != 0 && query[index] != 0)
    {
        if (!reach_search_char_equal_ci(text[index], query[index]))
        {
            return 0;
        }
        ++index;
    }
    return text[index] == 0 && query[index] == 0;
}

static int32_t reach_search_stem_equals_ci(const uint16_t *name, const uint16_t *query)
{
    if (name == nullptr || query == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (name[index] != 0 && name[index] != '.' && query[index] != 0)
    {
        if (!reach_search_char_equal_ci(name[index], query[index]))
        {
            return 0;
        }
        ++index;
    }
    return (name[index] == 0 || name[index] == '.') && query[index] == 0;
}

static int32_t reach_search_starts_with_ci(const uint16_t *text, const uint16_t *query)
{
    if (text == nullptr || query == nullptr || query[0] == 0)
    {
        return 0;
    }
    for (size_t index = 0; query[index] != 0; ++index)
    {
        if (text[index] == 0 || !reach_search_char_equal_ci(text[index], query[index]))
        {
            return 0;
        }
    }
    return 1;
}

static int32_t reach_search_is_word_break(uint16_t ch)
{
    return ch == ' ' || ch == '-' || ch == '_' || ch == '.' || ch == '+';
}

static int32_t reach_search_word_prefix_ci(const uint16_t *text, const uint16_t *query)
{
    if (text == nullptr || query == nullptr || query[0] == 0)
    {
        return 0;
    }
    for (size_t index = 0; text[index] != 0; ++index)
    {
        if (reach_search_is_word_break(text[index]) &&
            reach_search_starts_with_ci(text + index + 1, query))
        {
            return 1;
        }
    }
    return 0;
}

static int32_t reach_search_contains_ci(const uint16_t *text, const uint16_t *query)
{
    if (text == nullptr || query == nullptr || query[0] == 0)
    {
        return 0;
    }
    for (size_t start = 0; text[start] != 0; ++start)
    {
        size_t index = 0;
        while (query[index] != 0 && text[start + index] != 0 &&
               reach_search_char_equal_ci(text[start + index], query[index]))
        {
            ++index;
        }
        if (query[index] == 0)
        {
            return 1;
        }
    }
    return 0;
}

reach_search_match_tier reach_search_match_name(const uint16_t *name, const uint16_t *query)
{
    if (name == nullptr || query == nullptr || query[0] == 0)
    {
        return REACH_SEARCH_MATCH_NONE;
    }
    if (reach_search_equals_ci(name, query) || reach_search_stem_equals_ci(name, query))
    {
        return REACH_SEARCH_MATCH_EXACT;
    }
    if (reach_search_starts_with_ci(name, query))
    {
        return REACH_SEARCH_MATCH_PREFIX;
    }
    if (reach_search_word_prefix_ci(name, query))
    {
        return REACH_SEARCH_MATCH_WORD_PREFIX;
    }
    if (reach_search_contains_ci(name, query))
    {
        return REACH_SEARCH_MATCH_SUBSTRING;
    }
    return REACH_SEARCH_MATCH_NONE;
}

reach_search_result_kind reach_search_classify_result(const uint16_t *path, int32_t is_directory)
{
    if (is_directory)
    {
        return REACH_SEARCH_RESULT_FOLDER;
    }

    static const char *const photo_extensions[] = {".png", ".jpg", ".jpeg", ".webp",
                                                   ".gif", ".bmp", ".tif",  ".tiff"};
    static const char *const video_extensions[] = {".mp4", ".mkv", ".mov", ".avi", ".webm", ".wmv"};
    static const char *const music_extensions[] = {".mp3", ".wav", ".flac", ".aac",
                                                   ".ogg", ".m4a", ".wma"};
    static const char *const document_extensions[] = {".pdf", ".doc",  ".docx", ".xls", ".xlsx",
                                                      ".ppt", ".pptx", ".txt",  ".rtf", ".md"};

    if (reach_search_classify_launch(path, is_directory) != REACH_SEARCH_LAUNCH_NONE)
    {
        return REACH_SEARCH_RESULT_APP;
    }

    const uint16_t *extension = reach_search_extension(path);
    if (reach_search_extension_in(extension, photo_extensions,
                                  sizeof(photo_extensions) / sizeof(photo_extensions[0])))
    {
        return REACH_SEARCH_RESULT_PHOTO;
    }
    if (reach_search_extension_in(extension, video_extensions,
                                  sizeof(video_extensions) / sizeof(video_extensions[0])))
    {
        return REACH_SEARCH_RESULT_VIDEO;
    }
    if (reach_search_extension_in(extension, music_extensions,
                                  sizeof(music_extensions) / sizeof(music_extensions[0])))
    {
        return REACH_SEARCH_RESULT_MUSIC;
    }
    if (reach_search_extension_in(extension, document_extensions,
                                  sizeof(document_extensions) / sizeof(document_extensions[0])))
    {
        return REACH_SEARCH_RESULT_DOCUMENT;
    }
    return REACH_SEARCH_RESULT_FILE;
}

reach_search_launch_class reach_search_classify_launch(const uint16_t *path, int32_t is_directory)
{
    if (is_directory)
    {
        return REACH_SEARCH_LAUNCH_NONE;
    }

    static const char *const script_extensions[] = {".bat", ".cmd"};

    const uint16_t *extension = reach_search_extension(path);
    if (reach_search_extension_is(extension, ".exe"))
    {
        return REACH_SEARCH_LAUNCH_EXE;
    }
    if (reach_search_extension_is(extension, ".msc"))
    {
        return REACH_SEARCH_LAUNCH_MSC;
    }
    if (reach_search_extension_is(extension, ".msi"))
    {
        return REACH_SEARCH_LAUNCH_MSI;
    }
    if (reach_search_extension_is(extension, ".cpl"))
    {
        return REACH_SEARCH_LAUNCH_CPL;
    }
    if (reach_search_extension_in(extension, script_extensions,
                                  sizeof(script_extensions) / sizeof(script_extensions[0])))
    {
        return REACH_SEARCH_LAUNCH_SCRIPT;
    }
    return REACH_SEARCH_LAUNCH_NONE;
}

static int32_t reach_search_path_contains_ascii_ci(const uint16_t *path, const char *fragment)
{
    if (path == nullptr || fragment == nullptr)
    {
        return 0;
    }
    for (size_t start = 0; path[start] != 0; ++start)
    {
        size_t index = 0;
        while (fragment[index] != 0 && path[start + index] != 0 &&
               reach_search_char_equal_ci(path[start + index], (uint16_t)fragment[index]))
        {
            ++index;
        }
        if (fragment[index] == 0)
        {
            return 1;
        }
    }
    return 0;
}

reach_search_location_rank reach_search_classify_location(const uint16_t *path)
{
    static const char *const transient_fragments[] = {
        "\\temp\\",     "\\$recycle.bin\\", "\\node_modules\\", "\\.cache\\",
        "\\winsxs\\",   "\\packagecache\\", "\\installer\\",    "\\package cache\\",
        "\\.git\\",     "\\obj\\",          "\\.nuget\\"};
    static const char *const system_fragments[] = {"\\windows\\system32\\", "\\windows\\syswow64\\"};
    static const char *const program_fragments[] = {"\\program files\\", "\\program files (x86)\\"};

    if (path == nullptr)
    {
        return REACH_SEARCH_LOCATION_OTHER;
    }

    for (size_t index = 0; index < sizeof(transient_fragments) / sizeof(transient_fragments[0]);
         ++index)
    {
        if (reach_search_path_contains_ascii_ci(path, transient_fragments[index]))
        {
            return REACH_SEARCH_LOCATION_TRANSIENT;
        }
    }
    for (size_t index = 0; index < sizeof(system_fragments) / sizeof(system_fragments[0]); ++index)
    {
        if (reach_search_path_contains_ascii_ci(path, system_fragments[index]))
        {
            return REACH_SEARCH_LOCATION_SYSTEM;
        }
    }
    for (size_t index = 0; index < sizeof(program_fragments) / sizeof(program_fragments[0]); ++index)
    {
        if (reach_search_path_contains_ascii_ci(path, program_fragments[index]))
        {
            return REACH_SEARCH_LOCATION_PROGRAMS;
        }
    }
    return REACH_SEARCH_LOCATION_OTHER;
}

void reach_search_build_rank_key(const uint16_t *query, const reach_search_candidate *candidate,
                                 reach_search_rank_key *out_key)
{
    if (out_key == nullptr)
    {
        return;
    }

    *out_key = {};
    if (query == nullptr || candidate == nullptr)
    {
        return;
    }

    const uint16_t *name =
        candidate->name[0] != 0 ? candidate->name : reach_search_basename(candidate->path);

    reach_search_match_tier tier = reach_search_match_name(name, query);
    if (tier == REACH_SEARCH_MATCH_NONE && reach_search_contains_ci(candidate->path, query))
    {
        tier = REACH_SEARCH_MATCH_PATH;
    }
    if (candidate->match_tier > (int32_t)tier)
    {
        tier = (reach_search_match_tier)candidate->match_tier;
    }

    reach_search_launch_class launch_class =
        reach_search_classify_launch(candidate->path, candidate->is_directory);

    out_key->pinned = candidate->pinned != 0 && tier != REACH_SEARCH_MATCH_NONE ? 1 : 0;
    out_key->launchable = launch_class != REACH_SEARCH_LAUNCH_NONE ? 1 : 0;
    out_key->match_tier = (int32_t)tier;
    out_key->stem_length = (int32_t)reach_search_match_stem_length(name);
    out_key->launch_class = (int32_t)launch_class;
    out_key->location = (int32_t)reach_search_classify_location(candidate->path);
}

int32_t reach_search_compare_rank_keys(const reach_search_rank_key *a,
                                       const reach_search_rank_key *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    if (a->pinned != b->pinned)
    {
        return a->pinned > b->pinned ? -1 : 1;
    }
    if (a->launchable != b->launchable)
    {
        return a->launchable > b->launchable ? -1 : 1;
    }
    if (a->match_tier != b->match_tier)
    {
        return a->match_tier > b->match_tier ? -1 : 1;
    }
    if (a->stem_length != b->stem_length)
    {
        return a->stem_length < b->stem_length ? -1 : 1;
    }
    if (a->launch_class != b->launch_class)
    {
        return a->launch_class > b->launch_class ? -1 : 1;
    }
    if (a->location != b->location)
    {
        return a->location > b->location ? -1 : 1;
    }
    return 0;
}

static int32_t reach_search_compare_paths_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        uint16_t left = reach_search_ascii_lower(a[index]);
        uint16_t right = reach_search_ascii_lower(b[index]);
        if (left != right)
        {
            return left < right ? -1 : 1;
        }
        ++index;
    }
    if (a[index] == b[index])
    {
        return 0;
    }
    return a[index] == 0 ? -1 : 1;
}

size_t reach_search_rank_candidates(const uint16_t *query, reach_search_candidate *candidates,
                                    size_t candidate_count, size_t max_results)
{
    if (query == nullptr || candidates == nullptr || max_results == 0)
    {
        return 0;
    }
    if (candidate_count > REACH_SEARCH_RANK_CANDIDATE_CAP)
    {
        candidate_count = REACH_SEARCH_RANK_CANDIDATE_CAP;
    }

    reach_search_rank_key keys[REACH_SEARCH_RANK_CANDIDATE_CAP];
    for (size_t index = 0; index < candidate_count; ++index)
    {
        reach_search_build_rank_key(query, &candidates[index], &keys[index]);
        candidates[index].score = keys[index].match_tier;
    }

    size_t selected = candidate_count < max_results ? candidate_count : max_results;
    for (size_t index = 0; index < selected; ++index)
    {
        size_t best = index;
        for (size_t scan = index + 1; scan < candidate_count; ++scan)
        {
            int32_t order = reach_search_compare_rank_keys(&keys[scan], &keys[best]);
            if (order == 0)
            {
                order = reach_search_compare_paths_ci(candidates[scan].path, candidates[best].path);
            }
            if (order < 0)
            {
                best = scan;
            }
        }
        if (best != index)
        {
            reach_search_candidate temp_candidate = candidates[index];
            candidates[index] = candidates[best];
            candidates[best] = temp_candidate;
            reach_search_rank_key temp_key = keys[index];
            keys[index] = keys[best];
            keys[best] = temp_key;
        }
    }

    size_t matched_count = 0;
    while (matched_count < selected && keys[matched_count].match_tier != REACH_SEARCH_MATCH_NONE)
    {
        ++matched_count;
    }
    return matched_count;
}
