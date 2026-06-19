#include "windows_adapters_internal.h"

#include "reach/support/search_types.h"

#include <windows.h>

#include <new>
#include <vector>

static const DWORD REACH_EVERYTHING_REQUEST_FLAGS = 0x00000004 | 0x00000100;
static const DWORD REACH_EVERYTHING_CANDIDATE_LIMIT = REACH_SEARCH_CANDIDATE_MAX;

typedef void(__stdcall *reach_everything_set_search_w_fn)(LPCWSTR);
typedef void(__stdcall *reach_everything_set_regex_fn)(BOOL);
typedef void(__stdcall *reach_everything_set_max_fn)(DWORD);
typedef void(__stdcall *reach_everything_set_request_flags_fn)(DWORD);
typedef BOOL(__stdcall *reach_everything_query_w_fn)(BOOL);
typedef DWORD(__stdcall *reach_everything_get_num_results_fn)(void);
typedef DWORD(__stdcall *reach_everything_get_result_full_path_name_w_fn)(DWORD, LPWSTR, DWORD);
typedef DWORD(__stdcall *reach_everything_get_result_attributes_fn)(DWORD);
typedef DWORD(__stdcall *reach_everything_get_last_error_fn)(void);
typedef void(__stdcall *reach_everything_reset_fn)(void);

struct reach_everything_api
{
    HMODULE dll;
    reach_everything_set_search_w_fn set_search_w;
    reach_everything_set_regex_fn set_regex;
    reach_everything_set_max_fn set_max;
    reach_everything_set_request_flags_fn set_request_flags;
    reach_everything_query_w_fn query_w;
    reach_everything_get_num_results_fn get_num_results;
    reach_everything_get_result_full_path_name_w_fn get_result_full_path_name_w;
    reach_everything_get_result_attributes_fn get_result_attributes;
    reach_everything_get_last_error_fn get_last_error;
    reach_everything_reset_fn reset;
};

struct reach_search_provider
{
    reach_everything_api api;
    reach_search_candidate results[REACH_SEARCH_MAX_RESULTS];
    size_t result_count;
};

static void reach_search_copy_utf16(uint16_t *dst, size_t dst_count, const uint16_t *src)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }
    if (src == nullptr)
    {
        dst[0] = 0;
        return;
    }
    size_t index = 0;
    while (index + 1 < dst_count && src[index] != 0)
    {
        dst[index] = src[index];
        ++index;
    }
    dst[index] = 0;
}

static const uint16_t *reach_search_file_name(const uint16_t *path)
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

static uint16_t reach_search_lower(uint16_t ch)
{
    return ch >= 'A' && ch <= 'Z' ? (uint16_t)(ch - 'A' + 'a') : ch;
}

static int32_t reach_search_path_equals_ci(const uint16_t *a, const uint16_t *b)
{
    if (a == nullptr || b == nullptr)
    {
        return 0;
    }
    size_t index = 0;
    while (a[index] != 0 && b[index] != 0)
    {
        if (reach_search_lower(a[index]) != reach_search_lower(b[index]))
        {
            return 0;
        }
        ++index;
    }
    return a[index] == 0 && b[index] == 0;
}

static int32_t reach_search_path_has_extension_ci(const uint16_t *path, const char *extension)
{
    const uint16_t *name = reach_search_file_name(path);
    if (name == nullptr || extension == nullptr)
    {
        return 0;
    }

    const uint16_t *candidate_extension = nullptr;
    for (const uint16_t *scan = name; *scan != 0; ++scan)
    {
        if (*scan == '.')
        {
            candidate_extension = scan;
        }
    }
    if (candidate_extension == nullptr)
    {
        return 0;
    }

    size_t index = 0;
    while (candidate_extension[index] != 0 && extension[index] != 0)
    {
        if (reach_search_lower(candidate_extension[index]) !=
            reach_search_lower((uint16_t)extension[index]))
        {
            return 0;
        }
        ++index;
    }
    return candidate_extension[index] == 0 && extension[index] == 0;
}

static int32_t reach_search_query_is_regex_like(const uint16_t *query)
{
    if (query == nullptr)
    {
        return 0;
    }
    for (size_t index = 0; query[index] != 0; ++index)
    {
        switch (query[index])
        {
        case '^':
        case '$':
        case '.':
        case '*':
        case '+':
        case '?':
        case '[':
        case ']':
        case '(':
        case ')':
        case '{':
        case '}':
        case '|':
        case '\\':
            return 1;
        default:
            break;
        }
    }
    return 0;
}

static void reach_search_build_app_query(uint16_t *dst, size_t dst_count, const uint16_t *query)
{
    if (dst == nullptr || dst_count == 0)
    {
        return;
    }

    dst[0] = 0;
    if (query == nullptr || query[0] == 0)
    {
        return;
    }

    size_t write = 0;
    for (size_t read = 0; query[read] != 0 && write + 1 < dst_count; ++read)
    {
        dst[write++] = query[read];
    }

    const wchar_t *suffix = L" ext:exe";
    for (size_t read = 0; suffix[read] != 0 && write + 1 < dst_count; ++read)
    {
        dst[write++] = (uint16_t)suffix[read];
    }

    dst[write] = 0;
}

static FARPROC reach_everything_proc(HMODULE dll, const char *name)
{
    return dll != nullptr ? GetProcAddress(dll, name) : nullptr;
}

static int32_t reach_everything_load_api(reach_everything_api *api)
{
    if (api == nullptr)
    {
        return 0;
    }
    if (api->dll != nullptr)
    {
        return 1;
    }

    api->dll = LoadLibraryW(L"Everything64.dll");
    if (api->dll == nullptr)
    {
        api->dll = LoadLibraryW(L"Everything.dll");
    }
    if (api->dll == nullptr)
    {
        api->dll = LoadLibraryW(L"third_party\\everything\\dll\\Everything64.dll");
    }
    if (api->dll == nullptr)
    {
        return 0;
    }

    api->set_search_w = reinterpret_cast<reach_everything_set_search_w_fn>(
        reach_everything_proc(api->dll, "Everything_SetSearchW"));
    api->set_regex = reinterpret_cast<reach_everything_set_regex_fn>(
        reach_everything_proc(api->dll, "Everything_SetRegex"));
    api->set_max = reinterpret_cast<reach_everything_set_max_fn>(
        reach_everything_proc(api->dll, "Everything_SetMax"));
    api->set_request_flags = reinterpret_cast<reach_everything_set_request_flags_fn>(
        reach_everything_proc(api->dll, "Everything_SetRequestFlags"));
    api->query_w = reinterpret_cast<reach_everything_query_w_fn>(
        reach_everything_proc(api->dll, "Everything_QueryW"));
    api->get_num_results = reinterpret_cast<reach_everything_get_num_results_fn>(
        reach_everything_proc(api->dll, "Everything_GetNumResults"));
    api->get_result_full_path_name_w =
        reinterpret_cast<reach_everything_get_result_full_path_name_w_fn>(
            reach_everything_proc(api->dll, "Everything_GetResultFullPathNameW"));
    api->get_result_attributes = reinterpret_cast<reach_everything_get_result_attributes_fn>(
        reach_everything_proc(api->dll, "Everything_GetResultAttributes"));
    api->get_last_error = reinterpret_cast<reach_everything_get_last_error_fn>(
        reach_everything_proc(api->dll, "Everything_GetLastError"));
    api->reset = reinterpret_cast<reach_everything_reset_fn>(
        reach_everything_proc(api->dll, "Everything_Reset"));

    if (api->set_search_w == nullptr || api->set_regex == nullptr || api->set_max == nullptr ||
        api->set_request_flags == nullptr || api->query_w == nullptr ||
        api->get_num_results == nullptr || api->get_result_full_path_name_w == nullptr ||
        api->get_result_attributes == nullptr || api->get_last_error == nullptr ||
        api->reset == nullptr)
    {
        FreeLibrary(api->dll);
        *api = {};
        return 0;
    }

    return 1;
}

static void reach_search_add_candidate(std::vector<reach_search_candidate> *candidates,
                                       const uint16_t *path, int32_t is_directory)
{
    if (candidates == nullptr || path == nullptr || path[0] == 0)
    {
        return;
    }
    if (!is_directory && reach_search_path_has_extension_ci(path, ".lnk"))
    {
        return;
    }

    for (const reach_search_candidate &candidate : *candidates)
    {
        if (reach_search_path_equals_ci(candidate.path, path))
        {
            return;
        }
    }

    reach_search_candidate candidate = {};
    reach_search_copy_utf16(candidate.path, REACH_SEARCH_RESULT_PATH_CAPACITY, path);
    reach_search_copy_utf16(candidate.name, REACH_SEARCH_RESULT_NAME_CAPACITY,
                            reach_search_file_name(path));
    candidate.is_directory = is_directory ? 1 : 0;
    candidate.kind = reach_search_classify_result(candidate.path, candidate.is_directory);
    candidates->push_back(candidate);
}

static reach_result reach_everything_collect(reach_search_provider *provider, const uint16_t *query,
                                             int32_t regex,
                                             std::vector<reach_search_candidate> *candidates)
{
    if (provider == nullptr || query == nullptr || candidates == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }
    reach_everything_api *api = &provider->api;
    if (!reach_everything_load_api(api))
    {
        return REACH_ERROR;
    }

    api->reset();
    api->set_search_w(reinterpret_cast<LPCWSTR>(query));
    api->set_regex(regex ? TRUE : FALSE);
    api->set_max(REACH_EVERYTHING_CANDIDATE_LIMIT);
    api->set_request_flags(REACH_EVERYTHING_REQUEST_FLAGS);

    if (!api->query_w(TRUE))
    {
        (void)api->get_last_error();
        return REACH_ERROR;
    }

    DWORD count = api->get_num_results();
    if (count > REACH_EVERYTHING_CANDIDATE_LIMIT)
    {
        count = REACH_EVERYTHING_CANDIDATE_LIMIT;
    }

    for (DWORD index = 0; index < count; ++index)
    {
        uint16_t path[REACH_SEARCH_RESULT_PATH_CAPACITY] = {};
        DWORD copied = api->get_result_full_path_name_w(index, reinterpret_cast<LPWSTR>(path),
                                                        REACH_SEARCH_RESULT_PATH_CAPACITY);
        if (copied == 0 || path[0] == 0)
        {
            continue;
        }
        DWORD attributes = api->get_result_attributes(index);
        int32_t is_directory = (attributes & FILE_ATTRIBUTE_DIRECTORY) != 0;
        reach_search_add_candidate(candidates, path, is_directory);
    }

    return REACH_OK;
}

static reach_result reach_search_everything_query(reach_search_provider *provider,
                                                  const uint16_t *query)
{
    if (provider == nullptr || query == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    provider->result_count = 0;
    if (query[0] == 0)
    {
        return REACH_OK;
    }

    std::vector<reach_search_candidate> candidates;
    candidates.reserve(REACH_SEARCH_CANDIDATE_MAX);

    reach_result normal_result = reach_everything_collect(provider, query, 0, &candidates);

    uint16_t app_query[REACH_MAX_SEARCH_CHARS + 32] = {};
    reach_search_build_app_query(app_query, sizeof(app_query) / sizeof(app_query[0]), query);

    if (app_query[0] != 0)
    {
        (void)reach_everything_collect(provider, app_query, 0, &candidates);
    }

    if (reach_search_query_is_regex_like(query))
    {
        (void)reach_everything_collect(provider, query, 1, &candidates);
    }

    if (normal_result != REACH_OK && candidates.empty())
    {
        return REACH_ERROR;
    }

    size_t count = reach_search_rank_candidates(query, candidates.data(), candidates.size(),
                                                REACH_SEARCH_MAX_RESULTS);
    provider->result_count = count;
    for (size_t index = 0; index < count; ++index)
    {
        provider->results[index] = candidates[index];
    }

    return REACH_OK;
}

static size_t reach_search_everything_result_count(const reach_search_provider *provider)
{
    return provider != nullptr ? provider->result_count : 0;
}

static reach_result reach_search_everything_result_at(const reach_search_provider *provider,
                                                      size_t index, reach_search_result *out_result)
{
    if (provider == nullptr || out_result == nullptr || index >= provider->result_count)
    {
        return REACH_INVALID_ARGUMENT;
    }

    const reach_search_candidate *candidate = &provider->results[index];
    *out_result = {};
    out_result->id = (uint32_t)(index + 1);
    reach_search_copy_utf16(out_result->title, REACH_SEARCH_RESULT_NAME_CAPACITY, candidate->name);
    reach_search_copy_utf16(out_result->subtitle, REACH_SEARCH_RESULT_PATH_CAPACITY,
                            candidate->path);
    reach_search_copy_utf16(out_result->path, REACH_SEARCH_RESULT_PATH_CAPACITY, candidate->path);
    out_result->kind = candidate->kind;
    out_result->is_directory = candidate->is_directory;
    out_result->score = candidate->score;
    return REACH_OK;
}

static void reach_search_everything_destroy(reach_search_provider *provider)
{
    if (provider != nullptr && provider->api.dll != nullptr)
    {
        if (provider->api.reset != nullptr)
        {
            provider->api.reset();
        }
        FreeLibrary(provider->api.dll);
    }
    delete provider;
}

reach_result reach_windows_create_search_provider(reach_search_provider_port *out_port)
{
    if (out_port == nullptr)
    {
        return REACH_INVALID_ARGUMENT;
    }

    *out_port = {};
    reach_search_provider *provider = new (std::nothrow) reach_search_provider();
    if (provider == nullptr)
    {
        return REACH_ERROR;
    }

    out_port->provider = provider;
    out_port->ops.query = reach_search_everything_query;
    out_port->ops.result_count = reach_search_everything_result_count;
    out_port->ops.result_at = reach_search_everything_result_at;
    out_port->ops.destroy = reach_search_everything_destroy;
    return REACH_OK;
}
