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

static const uint16_t *reach_search_extension(const uint16_t *path)
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

reach_search_result_kind reach_search_classify_result(const uint16_t *path, int32_t is_directory)
{
    if (is_directory)
    {
        return REACH_SEARCH_RESULT_FOLDER;
    }

    static const char *const app_extensions[] = {".exe", ".lnk"};
    static const char *const photo_extensions[] = {".png", ".jpg", ".jpeg", ".webp",
                                                   ".gif", ".bmp", ".tif",  ".tiff"};
    static const char *const video_extensions[] = {".mp4", ".mkv", ".mov", ".avi", ".webm", ".wmv"};
    static const char *const music_extensions[] = {".mp3", ".wav", ".flac", ".aac",
                                                   ".ogg", ".m4a", ".wma"};
    static const char *const document_extensions[] = {".pdf", ".doc",  ".docx", ".xls", ".xlsx",
                                                      ".ppt", ".pptx", ".txt",  ".rtf", ".md"};

    const uint16_t *extension = reach_search_extension(path);
    if (reach_search_extension_in(extension, app_extensions,
                                  sizeof(app_extensions) / sizeof(app_extensions[0])))
    {
        return REACH_SEARCH_RESULT_APP;
    }
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

int32_t reach_search_score_candidate(const uint16_t *query, const reach_search_candidate *candidate)
{
    if (query == nullptr || query[0] == 0 || candidate == nullptr)
    {
        return 0;
    }

    const uint16_t *name =
        candidate->name[0] != 0 ? candidate->name : reach_search_basename(candidate->path);
    const int32_t app = candidate->kind == REACH_SEARCH_RESULT_APP;
    int32_t score = 0;

    if (app && (reach_search_equals_ci(name, query) || reach_search_stem_equals_ci(name, query)))
    {
        score = 900;
    }
    else if (app && reach_search_starts_with_ci(name, query))
    {
        score = 800;
    }
    else if (app && reach_search_contains_ci(name, query))
    {
        score = 700;
    }
    else if (app && reach_search_contains_ci(candidate->path, query))
    {
        score = 600;
    }
    else if (!app &&
             (reach_search_equals_ci(name, query) || reach_search_stem_equals_ci(name, query)))
    {
        score = 500;
    }
    else if (!app && reach_search_starts_with_ci(name, query))
    {
        score = 400;
    }
    else if (!app && reach_search_contains_ci(name, query))
    {
        score = 300;
    }
    else if (reach_search_contains_ci(candidate->path, query))
    {
        score = 200;
    }

    if (candidate->is_directory && score < 700)
    {
        score -= 150;
    }

    size_t name_length = reach_search_strlen(name);
    if (score > 0 && name_length < 96)
    {
        score += (int32_t)(96 - name_length);
    }

    return score;
}

size_t reach_search_rank_candidates(const uint16_t *query, reach_search_candidate *candidates,
                                    size_t candidate_count, size_t max_results)
{
    if (query == nullptr || candidates == nullptr || max_results == 0)
    {
        return 0;
    }

    for (size_t index = 0; index < candidate_count; ++index)
    {
        candidates[index].score = reach_search_score_candidate(query, &candidates[index]);
    }

    for (size_t index = 0; index < candidate_count; ++index)
    {
        size_t best = index;
        for (size_t scan = index + 1; scan < candidate_count; ++scan)
        {
            if (candidates[scan].score > candidates[best].score)
            {
                best = scan;
            }
        }
        if (best != index)
        {
            reach_search_candidate temp = candidates[index];
            candidates[index] = candidates[best];
            candidates[best] = temp;
        }
    }

    size_t positive_count = 0;
    while (positive_count < candidate_count && candidates[positive_count].score > 0)
    {
        ++positive_count;
    }

    if (positive_count > max_results)
    {
        positive_count = max_results;
    }
    return positive_count;
}
